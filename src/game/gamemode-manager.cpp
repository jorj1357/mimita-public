// 09 06 2026, 00 00
/* purpose
* Generic gamemode manager implementation. Reads features from the active
* gamemode's JSON config and renders HUD/world elements accordingly.
* Data-driven: adding a new feature to the JSON automatically triggers
* the correct rendering path without hardcoded mode name checks.
* Does NOT run gameplay simulation — the server owns all gameplay decisions.
* Does NOT produce per-frame log spam — uses throttled debug logging.
*/

#include "gamemode-manager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>

#include "entities/player.h"
#include "camera.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "debug/debug-visuals.h"
#include "audio/audio.h"
#include "network/community-match-client.h"
#include "network/multiplayer-context.h"
#include "gamemode/gamemode.h"
#include "terminal/terminal-state.h"
#include "npc/npc.h"

using namespace MimitaNet;

// ── Arm pose for bomb holder ──────────────────────────────────────────
void setArmToWeaponPose(Player& p, bool hasBomb) {
    if (!hasBomb) return;
    for (PhysicalBodyPart& part : p.physicalBody.parts) {
        if (part.name == "rightArm") {
            WeaponPoseConfig* revPose = nullptr;
            auto it = gPlayerProcedural.weaponPoses.find("revolver");
            if (it != gPlayerProcedural.weaponPoses.end())
                revPose = &it->second;
            if (revPose && revPose->useWeaponPose) {
                ProceduralPose target;
                target.rotationEuler = revPose->rightArm.rotation;
                target.translation = revPose->rightArm.translation;
                part.perfectPose = target;
                part.pose = target;
                part.translationSpring = SpringState{};
                part.rotationSpring = SpringState{};
            }
            break;
        }
    }
}

// ── Helper: get current gamemode features ──────────────────────────────
static const GamemodeFeatures& currentFeatures() {
    static const GamemodeFeatures empty;
    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return empty;
    const Gamemode& gm = GamemodeRegistry::instance().get(c.mode());
    return gm.features;
}

// ── GamemodeManager implementation ─────────────────────────────────────

void GamemodeManager::start() {
    mEnabled = true;
    mClientBombTick = 0;
    mPassBeamTimer = 0.0f;
    mPrevBombTimerTicks = 0;
    mPrevBombHolderId = 0;
    mInactiveSoundPlaying = false;
    Debug::log(Debug::Category::Duel, "[GAMEMODE MANAGER] started mode=%s\n",
        CommunityMatchClient::instance().mode().c_str());
}

void GamemodeManager::stop() {
    mEnabled = false;
    mPassBeamTimer = 0.0f;
    Debug::log(Debug::Category::Duel, "[GAMEMODE MANAGER] stopped\n");
}

bool GamemodeManager::isActive() const {
    const auto& c = CommunityMatchClient::instance();
    return c.active() && c.phase() == DUEL_PHASE_ACTIVE;
}

bool GamemodeManager::isCountdownActive() const {
    const auto& c = CommunityMatchClient::instance();
    return c.active() && (c.phase() == DUEL_PHASE_COUNTDOWN ||
                           c.phase() == DUEL_PHASE_GO);
}

bool GamemodeManager::isMatchEnd() const {
    const auto& c = CommunityMatchClient::instance();
    return c.active() && (c.phase() == DUEL_PHASE_MATCH_END ||
                           c.phase() == DUEL_PHASE_RESULTS);
}

const char* GamemodeManager::currentModeId() const {
    return CommunityMatchClient::instance().mode().c_str();
}

bool GamemodeManager::hasFeature(const char* featureName) const {
    const auto& f = currentFeatures();
    if (std::string(featureName) == "world_timer") return f.worldTimer;
    if (std::string(featureName) == "bomb_holder_text") return f.bombHolderText;
    if (std::string(featureName) == "bomb_blink") return f.bombBlink;
    if (std::string(featureName) == "infinite_rounds") return f.infiniteRounds;
    if (std::string(featureName) == "no_weapons_except_bomb") return f.noWeaponsExceptBomb;
    if (std::string(featureName) == "boss_healthbar") return f.bossHealthbar;
    if (std::string(featureName) == "world_text") return f.worldText;
    if (std::string(featureName) == "timer_above_entity") return f.timerAboveEntity;
    return false;
}

bool GamemodeManager::playerIsBombHolder(uint32_t localPlayerId) const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return false;
    const Gamemode& gm = GamemodeRegistry::instance().get(c.mode());
    if (!gm.features.bombHolderText) return false;
    return c.bombOwnerType() == BOMB_OWNER_PLAYER && c.bombOwnerPlayerId() == localPlayerId;
}

const char* GamemodeManager::bombHolderName(uint32_t localPlayerId, const Player& player) const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return "";
    const Gamemode& gm = GamemodeRegistry::instance().get(c.mode());
    if (!gm.features.bombHolderText) return "";
    if (c.bombOwnerType() == BOMB_OWNER_PLAYER) {
        if (c.bombOwnerPlayerId() == localPlayerId)
            return "You";
        auto it = MP_CONTEXT.remotePlayers.find(c.bombOwnerPlayerId());
        if (it != MP_CONTEXT.remotePlayers.end())
            return it->second.username.c_str();
        return "Player";
    }
    if (c.bombOwnerType() == BOMB_OWNER_NPC && gpNpcSystem) {
        uint32_t npcId = c.bombOwnerNpcIndex();
        auto& npcs = gpNpcSystem->all();
        for (const Npc& npc : npcs) {
            if (npc.id == npcId) {
                if (!npc.avatarName.empty())
                    return npc.avatarName.c_str();
                break;
            }
        }
        return "NPC";
    }
    return "Bomb Holder";
}

float GamemodeManager::bombSecondsRemaining() const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return 0.0f;
    return c.bombSecondsRemaining();
}

bool GamemodeManager::bombIsActive() const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return false;
    return c.bombIsActive();
}

glm::vec3 GamemodeManager::bombWorldPosition() const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return glm::vec3(0.0f);
    return c.bombPosition();
}

void GamemodeManager::update(float dt, Player& player) {
    if (!mEnabled) return;

    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return;

    mClientBombTick++;

    if (mPassBeamTimer > 0.0f)
        mPassBeamTimer -= dt;

    // Force arm pose on bomb holder if the mode has bomb_holder_text feature
    const Gamemode& gm = GamemodeRegistry::instance().get(c.mode());
    if (gm.features.bombHolderText) {
        setArmToWeaponPose(player, playerIsBombHolder(MP_CONTEXT.localPlayerId));
    }

    // ── Bomb sound playback (client-side, from replicated state) ────
    if (gm.features.bombHolderText && c.phase() == DUEL_PHASE_ACTIVE) {
        uint32_t curTimer = c.bombTimerTicks();
        uint32_t curHolder = c.bombOwnerPlayerId();

        // Explosion: timer was > 0, now == 0
        if (mPrevBombTimerTicks > 0 && curTimer == 0) {
            playEventSound("assets/sound/weapon/bomb/explosion2.wav", 1.0f);
        }

        // Pass: holder changed (both > 0 means a real transfer, not initial assignment)
        if (mPrevBombHolderId != 0 && curHolder != 0 && mPrevBombHolderId != curHolder) {
            playEventSound("assets/sound/weapon/bomb/bombpass1.wav", 1.0f);
        }

        // Tick sound: timer crossed a 60-tick boundary (once per second)
        if (curTimer > 0 && mPrevBombTimerTicks > 0) {
            uint32_t prevSecond = mPrevBombTimerTicks / 60;
            uint32_t curSecond = curTimer / 60;
            if (curSecond < prevSecond) {
                playEventSound("assets/sound/weapon/bomb/bombtick1.wav", 0.8f);
            }
        }

        // Inactive sound: play while inactive, stop when active
        if (c.bombInactiveTicks() > 0) {
            if (!mInactiveSoundPlaying) {
                playEventSound("assets/sound/weapon/bomb/bombinactive1.wav", 0.6f);
                mInactiveSoundPlaying = true;
            }
        } else {
            mInactiveSoundPlaying = false;
        }

        mPrevBombTimerTicks = curTimer;
        mPrevBombHolderId = curHolder;
    }
}

void GamemodeManager::renderHud() {
    if (!mEnabled) return;

    const auto& c = CommunityMatchClient::instance();
    if (!c.active()) return;

    const Gamemode& gm = GamemodeRegistry::instance().get(c.mode());

    // ── Feature: bomb_holder_text ────────────────────────────────────
    if (gm.features.bombHolderText) {
        renderBombHolderText();
    }

    // ── Feature: objective_bomb (Counter-Strike style) ───────────────
    if (gm.features.objectiveBomb) {
        renderObjectiveBombHud();
    }

    // ── Future features render here ──────────────────────────────────
    // if (gm.features.bossHealthbar) renderBossHealthbar();
    // if (gm.features.worldText) renderWorldText();
}

void GamemodeManager::renderWorldElements(Camera& camera, Player& player) {
    if (!mEnabled) return;

    const auto& c = CommunityMatchClient::instance();
    if (!c.active() || c.phase() != DUEL_PHASE_ACTIVE) return;

    const Gamemode& gm = GamemodeRegistry::instance().get(c.mode());

    // ── Feature: bomb_blink + world_timer ────────────────────────────
    if (gm.features.bombBlink || gm.features.worldTimer) {
        renderBombVisual(camera, player);
    }

    // ── Feature: objective_bomb (Counter-Strike style) ───────────────
    // Render the dropped/planted C4 and its timer from replicated state.
    if (gm.features.objectiveBomb) {
        const uint8_t bombState = c.objectiveBombState();
        if (bombState == BOMB_OBJ_DROPPED || bombState == BOMB_OBJ_PLANTED) {
            const glm::vec3 pos = c.bombPosition();
            if (pos != glm::vec3(0.0f)) {
                renderBombSphere(pos, (float)c.bombTimerTicks(),
                                 bombState == BOMB_OBJ_PLANTED);
                renderWorldTimer(pos, c.bombSecondsRemaining());
            }
        }
    }

    // ── Feature: pass effect ─────────────────────────────────────────
    renderPassEffect(camera);
}

void GamemodeManager::renderBombHolderText() {
    GuiLayout& btLayout = GuiLayoutManager::instance().getGamemodeLayout("bombtag");
    auto btText = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = btLayout.get(id);
        if (!el) return;
        float s = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), s, el->getTextColorVec());
    };

    const auto& c = CommunityMatchClient::instance();

    if (isCountdownActive()) {
        float timer = c.phaseTimer();
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f", std::max(0.0f, timer));
        btText("countdownText", buf);
        return;
    }

    if (isActive()) {
        float seconds = bombSecondsRemaining();
        char buf[256];

        snprintf(buf, sizeof(buf), "%.2f", std::max(0.0f, seconds));
        btText("timerText", buf);

        uint32_t localId = MP_CONTEXT.localPlayerId;
        if (playerIsBombHolder(localId)) {
            snprintf(buf, sizeof(buf), "You have the bomb!!!! %.2f until it explodes!!!", std::max(0.0f, seconds));
            btText("bombAlert", buf);
        } else {
            const char* name = bombHolderName(localId, THE_PLAYER);
            snprintf(buf, sizeof(buf), "%s has the bomb!!!! %.2f until it explodes!!!", name, std::max(0.0f, seconds));
            btText("npcBombAlert", buf);
        }
    }
}

void GamemodeManager::renderObjectiveBombHud() {
    const auto& c = CommunityMatchClient::instance();
    const uint8_t state = c.objectiveBombState();
    if (state == BOMB_OBJ_NONE) return;

    GuiLayout& layout = GuiLayoutManager::instance().getGamemodeLayout(c.mode());
    auto draw = [&](const char* id, const std::string& text) {
        const GuiElement* el = layout.get(id);
        if (!el || !el->visible) return;
        const float scale = el->fontSize > 0.0f ? el->fontSize : 0.36f;
        const float w = uiMeasureText(text.c_str(), scale);
        uiDrawText(text.c_str(), uiScreenW() * 0.5f - w * 0.5f,
                   uiScaleY(el->y), scale, el->getTextColorVec());
    };

    if (state == BOMB_OBJ_CARRIED) {
        char buf[96];
        if (c.bombOwnerPlayerId() == MP_CONTEXT.localPlayerId) {
            if (c.objectivePlantPercent() > 0)
                snprintf(buf, sizeof(buf), "Planting... %u%%", (unsigned)c.objectivePlantPercent());
            else
                snprintf(buf, sizeof(buf), "You have the bomb! Plant it in a bomb site.");
            draw("bombAlert", buf);
        }
    } else if (state == BOMB_OBJ_DROPPED) {
        draw("bombAlert", "Bomb dropped - Terrorists can pick it up.");
    } else if (state == BOMB_OBJ_PLANTED) {
        char buf[64];
        snprintf(buf, sizeof(buf), "BOMB PLANTED  %.1fs",
                 std::max(0.0f, c.bombSecondsRemaining()));
        draw("bombPlanted", buf);
        if (c.objectiveDefusePercent() > 0) {
            char def[64];
            snprintf(def, sizeof(def), "DEFUSING... %u%%", (unsigned)c.objectiveDefusePercent());
            draw("bombAlert", def);
        }
    }
}

void GamemodeManager::renderBombVisual(Camera& camera, Player& player) {
    if (!mCamera) return;

    glm::vec3 bombPos = bombWorldPosition();
    if (bombPos == glm::vec3(0.0f)) return;

    bool isActive = bombIsActive();
    float timerTicks = (float)CommunityMatchClient::instance().bombTimerTicks();

    renderBombSphere(bombPos, timerTicks, isActive);
    renderWorldTimer(bombPos, bombSecondsRemaining());
}

void GamemodeManager::renderPassEffect(Camera& camera) {
    if (!mCamera) return;
    if (mPassBeamTimer <= 0.0f) return;

    float alpha = std::min(1.0f, mPassBeamTimer * 2.0f);
    glm::vec4 beamColor(0.2f, 0.8f, 1.0f, alpha);
    DebugVis::drawFilledBeam(camera, mPassBeamStart, mPassBeamEnd, 0.05f, beamColor);
}

void GamemodeManager::renderBombSphere(const glm::vec3& pos, float timerTicks, bool isActive) {
    if (!mCamera) return;

    const Gamemode& gm = GamemodeRegistry::instance().get(
        CommunityMatchClient::instance().mode());
    uint32_t blinkTicks = (uint32_t)gm.blinkTicks;

    glm::vec4 col;
    if (!isActive) {
        col = glm::vec4(80.0f/255.0f, 80.0f/255.0f, 80.0f/255.0f, 1.0f);
    } else {
        uint32_t tick = (uint32_t)std::floor(timerTicks);
        bool blink = (tick % (blinkTicks * 2)) < blinkTicks;
        if (blink)
            col = glm::vec4(10.0f/255.0f, 10.0f/255.0f, 10.0f/255.0f, 1.0f);
        else
            col = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    DebugVis::drawFilledSphere(*mCamera, pos, 0.5f, col);
}

void GamemodeManager::renderWorldTimer(const glm::vec3& pos, float seconds) {
    if (!mCamera) return;

    char timerTxt[32];
    snprintf(timerTxt, sizeof(timerTxt), "%.2f", std::max(0.0f, seconds));
    float sx = 0, sy = 0;
    glm::vec3 labelPos = pos + glm::vec3(0.0f, 0.0f, 1.3f);
    if (DebugVis::projectToScreen(*mCamera, labelPos, sx, sy)) {
        float tw = uiMeasureText(timerTxt, 0.40f);
        uiDrawText(timerTxt, sx - tw * 0.5f, sy - 16.0f, 0.40f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
}
