// 09 06 2026, 00 00
/* purpose
* Client-side Bomb Tag rendering: HUD text, bomb sphere visual, world timer,
* and pass beam effect. All data comes from server-authoritative replicated
* state via CommunityMatchClient. No gameplay simulation runs here.
* Does NOT decide bomb ownership, manage timers, validate passes, or
* apply damage — the server owns all gameplay decisions.
* Does NOT produce per-frame log spam — uses throttled debug logging.
*/

#include "bomb-tag.h"

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
#include "terminal/terminal-state.h"

using namespace MimitaNet;

// ── Arm pose for bomb holder ──────────────────────────────────────────
// Forces the right arm into a forward+up pose (different from revolver).
// The exact rotation values should be tuned for a "presenting the bomb" look.
void setArmToWeaponPose(Player& p, bool hasBomb) {
    if (!hasBomb) return;
    for (PhysicalBodyPart& part : p.physicalBody.parts) {
        if (part.name == "rightArm") {
            // Use revolver pose as base, but the actual visual distinction
            // comes from the bomb sphere attached to the hand.
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

// ── BombTagManager implementation ──────────────────────────────────────

void BombTagManager::start() {
    mEnabled = true;
    mClientBombTick = 0;
    mPassBeamTimer = 0.0f;
    Debug::log(Debug::Category::Duel, "[BOMB TAG CLIENT] started\n");
}

void BombTagManager::stop() {
    mEnabled = false;
    mPassBeamTimer = 0.0f;
    Debug::log(Debug::Category::Duel, "[BOMB TAG CLIENT] stopped\n");
}

bool BombTagManager::isActive() const {
    const auto& c = CommunityMatchClient::instance();
    return c.isBombTag() && c.phase() == DUEL_PHASE_ACTIVE;
}

bool BombTagManager::isCountdownActive() const {
    const auto& c = CommunityMatchClient::instance();
    return c.isBombTag() && (c.phase() == DUEL_PHASE_COUNTDOWN ||
                              c.phase() == DUEL_PHASE_PRE_MATCH ||
                              c.phase() == DUEL_PHASE_GO);
}

bool BombTagManager::isMatchEnd() const {
    const auto& c = CommunityMatchClient::instance();
    return c.isBombTag() && (c.phase() == DUEL_PHASE_MATCH_END ||
                              c.phase() == DUEL_PHASE_RESULTS);
}

bool BombTagManager::playerIsBombHolder(uint32_t localPlayerId) const {
    const auto& c = CommunityMatchClient::instance();
    return c.isBombTag() && c.bombOwnerType() == BOMB_OWNER_PLAYER
        && c.bombOwnerPlayerId() == localPlayerId;
}

const char* BombTagManager::bombHolderName(uint32_t localPlayerId, const Player& player) const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag()) return "";
    if (c.bombOwnerType() == BOMB_OWNER_PLAYER) {
        if (c.bombOwnerPlayerId() == localPlayerId)
            return "You";
        // Look up remote player name
        auto it = MP_CONTEXT.remotePlayers.find(c.bombOwnerPlayerId());
        if (it != MP_CONTEXT.remotePlayers.end())
            return it->second.username.c_str();
        return "Player";
    }
    return "Bomb Holder";
}

float BombTagManager::bombSecondsRemaining() const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag()) return 0.0f;
    return c.bombSecondsRemaining();
}

bool BombTagManager::bombIsActive() const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag()) return false;
    return c.bombIsActive();
}

glm::vec3 BombTagManager::bombWorldPosition() const {
    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag()) return glm::vec3(0.0f);
    return c.bombPosition();
}

void BombTagManager::update(float dt, Player& player) {
    if (!mEnabled) return;

    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag()) return;

    // Update client-side bomb tick for blink timing (visual only)
    mClientBombTick++;

    // Update pass beam timer
    if (mPassBeamTimer > 0.0f)
        mPassBeamTimer -= dt;

    // Force arm pose on bomb holder
    setArmToWeaponPose(player, playerIsBombHolder(MP_CONTEXT.localPlayerId));
}

void BombTagManager::renderHud() {
    if (!mEnabled) return;

    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag()) return;

    GuiLayout& btLayout = GuiLayoutManager::instance().getLayout("config/gui/bomb-tag-hud.json");
    auto btText = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = btLayout.get(id);
        if (!el) return;
        float s = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), s, el->getTextColorVec());
    };

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

        // Timer display
        snprintf(buf, sizeof(buf), "%.2f", std::max(0.0f, seconds));
        btText("timerText", buf);

        // Bomb holder alert
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

void BombTagManager::renderBombVisual(Camera& camera, Player& player) {
    if (!mEnabled) return;

    const auto& c = CommunityMatchClient::instance();
    if (!c.isBombTag() || c.phase() != DUEL_PHASE_ACTIVE) return;

    glm::vec3 bombPos = bombWorldPosition();
    if (bombPos == glm::vec3(0.0f)) return;

    bool isActive = bombIsActive();
    float timerTicks = (float)c.bombTimerTicks();

    renderBombSphere(bombPos, timerTicks, isActive);
    renderWorldTimer(bombPos, bombSecondsRemaining());
}

void BombTagManager::renderPassEffect(Camera& camera) {
    if (!mEnabled) return;
    if (mPassBeamTimer <= 0.0f) return;

    float alpha = std::min(1.0f, mPassBeamTimer * 2.0f);
    glm::vec4 beamColor(0.2f, 0.8f, 1.0f, alpha);
    DebugVis::drawFilledBeam(camera, mPassBeamStart, mPassBeamEnd, 0.05f, beamColor);
}

void BombTagManager::renderBombSphere(const glm::vec3& pos, float timerTicks, bool isActive) {
    if (!mCamera) return;

    // Spec: blink every 30 ticks between (10,10,10) and (255,0,0)
    // Inactive: grey (80,80,80)
    glm::vec4 col;
    if (!isActive) {
        // Inactive: grey
        col = glm::vec4(80.0f/255.0f, 80.0f/255.0f, 80.0f/255.0f, 1.0f);
    } else {
        // Active: blink every 30 ticks
        uint32_t tick = (uint32_t)std::floor(timerTicks);
        bool blink = (tick % 60) < 30;  // First 30 of each 60-tick cycle = dark
        if (blink)
            col = glm::vec4(10.0f/255.0f, 10.0f/255.0f, 10.0f/255.0f, 1.0f);
        else
            col = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    DebugVis::drawFilledSphere(*mCamera, pos, 0.5f, col);
}

void BombTagManager::renderWorldTimer(const glm::vec3& pos, float seconds) {
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
