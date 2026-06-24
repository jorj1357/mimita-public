#include "bomb-tag.h"
#include "spawn-utils.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <glm/glm.hpp>

#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "camera.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"

#include "combat/weapon-godball.h"
#include "debug/debug-visuals.h"
#include "audio/audio.h"
#include "config.h"

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

static const char* npcName(int index) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "NPC %d", index + 1);
    return buf;
}

void BombTagManager::start(const BombTagConfig& cfg, Player& player, NpcSystem& npcs, World& world) {
    mConfig = cfg;
    mPhase = BombTagPhase::Countdown;
    mCountdown = 3.0f;
    mTimer = 0.0f;
    mBombTimer = 15.0f;
    mPassCooldown = 0.0f;
    mTransferCooldown = 0.0f;
    mLastTickSecond = -1;
    mTickPitch = 1.0f;
    mCooldownSoundPlayed = false;
    mPlayerKills = 0;
    mPlayerDeaths = 0;
    mPlayerBombPasses = 0;
    mNpcKills.assign(cfg.numNpcs, 0);
    mNpcPasses.assign(cfg.numNpcs, 0);
    mPlayerLivesRemaining = (cfg.lives > 0) ? cfg.lives : 999999;
    mEndState = BombTagEndState::None;
    mBombHolderIsPlayer = false;
    mBombHolderIndex = -1;
    mCurrentBombHolderNpc = -1;
    mWinnerIndex = -1;
    mWinnerKills = 0;
    matchEndTick = 0;
    finalKillSavedOnce = false;

    npcs.destroyAll();
    for (int i = 0; i < cfg.numNpcs; ++i)
        spawnNpcAtSafePosition(npcs, (uint32_t)(100 + i), cfg.npcDifficulty, world, i);

    glm::vec3 spawnPos = getSpawnPosition(world, cfg.numNpcs);
    player.pos = spawnPos;
    player.vel = glm::vec3(0.0f);
    if (player.dead) {
        player.dead = false;
        player.currentHp = player.maxHp;
        player.spawnFlashTimer = 0.0f;
    }

    logSpawnDiagnostics(world, player, npcs);
    printf("[BOMB TAG] started: npcs=%d lives=%d time=%ds difficulty=%.0f map=%s\n",
           cfg.numNpcs, cfg.lives, cfg.timeLimitSeconds, cfg.npcDifficulty, cfg.mapPath.c_str());
}

void BombTagManager::stop() {
    mConfig.enabled = false;
    mPhase = BombTagPhase::Off;
    printf("[BOMB TAG] stopped\n");
}

void BombTagManager::assignInitialBomb(Player& player, NpcSystem& npcs) {
    int npcCount = (int)npcs.all().size();
    int total = 1 + npcCount;
    int chosen = rand() % total;
    if (chosen == 0 && !player.dead) {
        mBombHolderIsPlayer = true;
        mBombHolderIndex = 0;
        mCurrentBombHolderNpc = -1;
        printf("[BOMB TAG] player has the bomb\n");
    } else {
        int npcIdx = -1;
        for (int attempt = 0; attempt < npcCount * 2; ++attempt) {
            int idx = rand() % npcCount;
            if (!npcs.all()[idx].body.dead) { npcIdx = idx; break; }
        }
        if (npcIdx < 0) npcIdx = chosen > 0 ? chosen - 1 : 0;
        npcIdx = std::min(npcIdx, npcCount - 1);
        if (npcIdx >= 0) {
            mBombHolderIsPlayer = false;
            mBombHolderIndex = npcIdx;
            mCurrentBombHolderNpc = npcIdx;
            printf("[BOMB TAG] %s has the bomb\n", npcName(npcIdx));
        } else {
            mBombHolderIsPlayer = true;
            mBombHolderIndex = 0;
            mCurrentBombHolderNpc = -1;
            printf("[BOMB TAG] bomb given to player (no NPCs)\n");
        }
    }
    // Timer stays at whatever start() set — transfer only changes owner
    mTransferCooldown = 2.0f;
    mCooldownSoundPlayed = false;
    printf("[BOMB TAG] initial bomb assignment, timer=%.2f\n", mBombTimer);
}

int BombTagManager::findNearestLivingNpc(const glm::vec3& pos, NpcSystem& npcs, int excludeIndex) {
    float bestDist = 1e9f;
    int best = -1;
    for (int i = 0; i < (int)npcs.all().size(); ++i) {
        if (i == excludeIndex || npcs.all()[i].body.dead) continue;
        float d = glm::distance(pos, npcs.all()[i].body.pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void BombTagManager::passBomb(Player& player, NpcSystem& npcs) {
    glm::vec3 passPos = mBombWorldPos;

    if (mBombHolderIsPlayer) {
        int target = findNearestLivingNpc(player.pos, npcs, -1);
        if (target >= 0) {
            mBombHolderIsPlayer = false;
            mBombHolderIndex = target;
            mCurrentBombHolderNpc = target;
            mPlayerBombPasses++;
            printf("[BOMB TAG] player -> %s\n", npcName(target));
        }
    } else {
        int cn = mCurrentBombHolderNpc;
        if (cn < 0 || cn >= (int)npcs.all().size()) {
            if (!player.dead) {
                mBombHolderIsPlayer = true;
                mBombHolderIndex = 0;
                mCurrentBombHolderNpc = -1;
                printf("[BOMB TAG] bomb -> player (invalid holder)\n");
            }
            return;
        }
        glm::vec3 pos = npcs.all()[cn].body.pos;
        float playerDist = glm::distance(pos, player.pos);
        if (playerDist < 15.0f && !player.dead) {
            mBombHolderIsPlayer = true;
            mBombHolderIndex = 0;
            mCurrentBombHolderNpc = -1;
            mNpcPasses[cn]++;
            printf("[BOMB TAG] %s -> player\n", npcName(cn));
        } else {
            int target = findNearestLivingNpc(pos, npcs, cn);
            if (target >= 0) {
                mBombHolderIsPlayer = false;
                mBombHolderIndex = target;
                mCurrentBombHolderNpc = target;
                mNpcPasses[cn]++;
                printf("[BOMB TAG] %s -> %s\n", npcName(cn), npcName(target));
            }
        }
    }

    // Transfer cooldown + sounds
    mTransferCooldown = 2.0f;
    mCooldownSoundPlayed = false;
    playWorldSound("weapon/bomb/bombpass1", passPos, 1.0f, 1.0f, 30.0f);

    // IMPORTANT: Timer belongs to the bomb, NOT the holder.
    // Transfer only changes ownership, never resets the timer.
    printf("[BOMB TAG] transfer completed, cooldown=2s timer=%.2f\n", mBombTimer);
}

void BombTagManager::explodeBomb(Player& player, NpcSystem& npcs) {
    printf("[BOMB TAG] BOOM! holder=%s\n", mBombHolderIsPlayer ? "player" : "npc");

    playWorldSound("weapon/bomb/explosion2", mBombWorldPos, 1.0f, 1.0f, 50.0f);

    if (mBombHolderIsPlayer) {
        mPlayerDeaths++;
        mPlayerLivesRemaining--;
        player.dead = true;
        player.currentHp = 0;
        player.respawnTimer = 3.0f;
        int killerNpc = findNearestLivingNpc(player.pos, npcs, -1);
        if (killerNpc >= 0) mNpcKills[killerNpc]++;
    } else {
        int idx = mCurrentBombHolderNpc;
        if (idx >= 0 && idx < (int)npcs.all().size()) {
            npcs.all()[idx].body.dead = true;
            npcs.all()[idx].body.currentHp = 0;
            if (!player.dead) mPlayerKills++;
        }
    }

    mBombHolderIsPlayer = false;
    mBombHolderIndex = -1;
    mCurrentBombHolderNpc = -1;

    if (!player.dead) {
        mBombHolderIsPlayer = true;
        mBombHolderIndex = 0;
    } else {
        int target = findNearestLivingNpc(player.pos, npcs, -1);
        if (target >= 0) {
            mBombHolderIsPlayer = false;
            mBombHolderIndex = target;
            mCurrentBombHolderNpc = target;
        } else {
            mBombTimer = 999.0f;
            return;
        }
    }
    mBombTimer = 15.0f + (float)(rand() % 5);
    mTransferCooldown = 2.0f;
    mCooldownSoundPlayed = false;
}

void BombTagManager::renderHud() {
    if (!mConfig.enabled) return;
    GuiLayout& btLayout = GuiLayoutManager::instance().getLayout("config/gui/bomb-tag-hud.json");
    auto btText = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = btLayout.get(id);
        if (!el) return;
        float s = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), s, el->getTextColorVec());
    };

    if (mPhase == BombTagPhase::Countdown) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", std::ceil(mCountdown));
        btText("countdownText", buf);
        return;
    }

    if (mPhase == BombTagPhase::Active) {
        char buf[256];
        float remaining = std::max(0.0f, (float)mConfig.timeLimitSeconds - mTimer);
        snprintf(buf, sizeof(buf), "%.0f", remaining);
        btText("timerText", buf);

        if (mBombHolderIsPlayer) {
            snprintf(buf, sizeof(buf), "YOU HAVE THE BOMB! (%.0fs)", mBombTimer);
            btText("bombAlert", buf);
        } else if (mCurrentBombHolderNpc >= 0) {
            snprintf(buf, sizeof(buf), "NPC %d has the bomb", mCurrentBombHolderNpc + 1);
            btText("npcBombAlert", buf);
        }

        snprintf(buf, sizeof(buf), "Kills: %d", mPlayerKills);
        btText("killsText", buf);

        if (mConfig.lives > 0) {
            snprintf(buf, sizeof(buf), "Lives: %d/%d", mPlayerLivesRemaining, mConfig.lives);
            btText("livesText", buf);
        }
    }
}

BombTagMenuAction BombTagManager::renderMatchOverScreen(GLFWwindow* win) {
    if (mPhase != BombTagPhase::MatchEnd) return BombTagMenuAction::None;
    float sw = uiScreenW(), sh = uiScreenH();

    GuiLayout& btLayout = GuiLayoutManager::instance().getLayout("config/gui/bomb-tag-hud.json");
    auto btText = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = btLayout.get(id);
        if (!el) return;
        float s = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), s, el->getTextColorVec());
    };

    if (mEndState == BombTagEndState::VictoryScreen) {
        drawGuiElement(win, *btLayout.get("victoryDim"));
        char buf[128];
        if (mWinnerIndex == -2) {
            btText("stalemateText", "STALEMATE");
            btText("stalemateDesc", "Nobody scored a kill.");
        } else if (mWinnerIsTie) {
            btText("tieText", "TIE");
            snprintf(buf, sizeof(buf), "Kills: %d", mWinnerKills);
            btText("tieScoreText", buf);
        } else {
            bool playerWon = mWinnerKills >= 0 && mWinnerIndex < 0;
            btText(playerWon ? "winnerText" : "gameOverText",
                   playerWon ? "WINNER" : "GAME OVER");
            snprintf(buf, sizeof(buf), "Kills: %d  Deaths: %d", mPlayerKills, mPlayerDeaths);
            btText("scoreText", buf);
        }
    }

    if (mEndState == BombTagEndState::Countdown || mEndState == BombTagEndState::FinalKillReplay) {
        const GuiElement* pa = btLayout.get("playAgainButton");
        if (pa && drawGuiElement(win, *pa).clicked)
            return BombTagMenuAction::PlayAgain;
        const GuiElement* ex = btLayout.get("exitButton");
        if (ex && drawGuiElement(win, *ex).clicked)
            return BombTagMenuAction::ExitToMenu;
    }
    return BombTagMenuAction::None;
}


