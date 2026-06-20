#include "bomb-tag.h"
#include "spawn-utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/glm.hpp>

#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "camera.h"
#include "replay/replay.h"
#include "renderer/renderer.h"
#include "game/duel.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"

#include "combat/weapon-godball.h"
#include "debug/debug-visuals.h"
#include "effects/effect-part.h"
#include "audio/audio.h"
#include "physics/physics-types.h"
#include "config.h"

namespace {

static bool bombCapsuleContact(const Capsule& a, const Capsule& b) {
    float aMinZ = std::min(a.a.z, a.b.z) - a.r;
    float aMaxZ = std::max(a.a.z, a.b.z) + a.r;
    float bMinZ = std::min(b.a.z, b.b.z) - b.r;
    float bMaxZ = std::max(b.a.z, b.b.z) + b.r;
    if (aMaxZ <= bMinZ || bMaxZ <= aMinZ) return false;
    glm::vec2 ca = (glm::vec2(a.a.x, a.a.y) + glm::vec2(a.b.x, a.b.y)) * 0.5f;
    glm::vec2 cb = (glm::vec2(b.a.x, b.a.y) + glm::vec2(b.b.x, b.b.y)) * 0.5f;
    return glm::length(ca - cb) < (a.r + b.r) * 1.3f;
}

static void setArmToWeaponPose(Player& p, bool hasBomb) {
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

} // namespace

static const char* npcName(int index) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "NPC %d", index + 1);
    return buf;
}

void BombTagManager::freezeAll(Player& player, NpcSystem& npcs) {
    player.vel = glm::vec3(0.0f);
    for (auto& n : npcs.all())
        n.body.vel = glm::vec3(0.0f);
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

int BombTagManager::countLivingNpcs(NpcSystem& npcs) {
    int cnt = 0;
    for (const auto& n : npcs.all())
        if (!n.body.dead) ++cnt;
    return cnt;
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

void BombTagManager::checkMatchEnd(Player& player, NpcSystem& npcs) {
    bool endByLives = (mPlayerLivesRemaining <= 0 && player.dead);
    bool endByTime = (mTimer >= (float)mConfig.timeLimitSeconds);
    bool endByNpcs = (countLivingNpcs(npcs) == 0);

    if (!endByLives && !endByTime && !endByNpcs)
        return;

    mPhase = BombTagPhase::MatchEnd;

    // Find the entity with the most kills
    int maxKills = mPlayerKills;
    int maxKillsEntity = -1; // -1 = player

    // Check ties: count how many entities share the max kill count
    bool isTie = false;
    bool allZeroKills = (mPlayerKills == 0);

    for (int i = 0; i < (int)mNpcKills.size(); ++i) {
        if (mNpcKills[i] > maxKills) {
            maxKills = mNpcKills[i];
            maxKillsEntity = i;
            isTie = false;
        } else if (mNpcKills[i] == maxKills && maxKills > 0) {
            isTie = true;
        }
        if (mNpcKills[i] > 0)
            allZeroKills = false;
    }

    // If even the player now ties for highest kills (>0)
    if (mPlayerKills == maxKills && maxKills > 0 && maxKillsEntity >= 0)
        isTie = true;

    if (allZeroKills)
    {
        // Stalemate: nobody scored
        mWinnerIndex = -2;
        mWinnerKills = 0;
        mWinnerIsTie = false;
    }
    else if (isTie)
    {
        // Tie: multiple entities share highest kills
        mWinnerIndex = -3;
        mWinnerKills = maxKills;
        mWinnerIsTie = true;
    }
    else
    {
        // Clear winner
        mWinnerIndex = maxKillsEntity;
        mWinnerKills = maxKills;
        mWinnerIsTie = false;
    }
}

void BombTagManager::update(float dt, Player& player, NpcSystem& npcs, World& world) {
    if (!mConfig.enabled) return;

    switch (mPhase) {
        case BombTagPhase::Off:
            return;

        case BombTagPhase::Countdown: {
            mCountdown -= dt;
            freezeAll(player, npcs);
            if (mCountdown <= 0.0f) {
                mPhase = BombTagPhase::Active;
                mCountdown = 0.0f;
                assignInitialBomb(player, npcs);
                logSpawnDiagnostics(world, player, npcs);
                printf("[BOMB TAG] FIGHT!\n");
            }
            break;
        }

        case BombTagPhase::Active: {
            mTimer += dt;
            mPassCooldown = std::max(0.0f, mPassCooldown - dt);
            mBombTimer -= dt;
            mTransferCooldown = std::max(0.0f, mTransferCooldown - dt);

            if (mBombHolderIsPlayer)
                mBombWorldPos = WeaponGodball::getHandPosition(player);
            else if (mCurrentBombHolderNpc >= 0 && mCurrentBombHolderNpc < (int)npcs.all().size())
                mBombWorldPos = WeaponGodball::getHandPosition(npcs.all()[mCurrentBombHolderNpc].body);
            else
                mBombWorldPos = player.pos + glm::vec3(0.0f, 0.0f, 1.5f);

            // Transfer only when cooldown is expired
            if (mTransferCooldown <= 0.0f) {
                if (mBombHolderIsPlayer && !player.dead) {
                    Capsule pc = player.getCapsule();
                    for (int i = 0; i < (int)npcs.all().size(); ++i) {
                        if (npcs.all()[i].body.dead) continue;
                        if (bombCapsuleContact(pc, npcs.all()[i].body.getCapsule())) {
                            passBomb(player, npcs); break;
                        }
                    }
                } else if (!mBombHolderIsPlayer && mCurrentBombHolderNpc >= 0) {
                    int ci = mCurrentBombHolderNpc;
                    if (ci < (int)npcs.all().size() && !npcs.all()[ci].body.dead) {
                        Capsule hc = npcs.all()[ci].body.getCapsule();
                        if (!player.dead && bombCapsuleContact(hc, player.getCapsule()))
                            passBomb(player, npcs);
                        else {
                            for (int j = 0; j < (int)npcs.all().size(); ++j) {
                                if (j == ci || npcs.all()[j].body.dead) continue;
                                if (bombCapsuleContact(hc, npcs.all()[j].body.getCapsule()))
                                { passBomb(player, npcs); break; }
                            }
                        }
                    }
                }
            }

            // Inactive sound on cooldown start
            if (mTransferCooldown > 0.0f && !mCooldownSoundPlayed) {
                playWorldSound("weapon/bomb/bombinactive1", mBombWorldPos, 1.0f, 1.0f, 30.0f);
                mCooldownSoundPlayed = true;
            }

            // Bomb explosion
            if (mBombTimer <= 0.0f) {
                auto& fx = EffectPartSystem::instance();
                fx.spawnDeathEllipsoid(mBombWorldPos, glm::vec3(0,0,1), 6.0f, 3.0f, 2.5f);
                for (int i = 0; i < 10; ++i)
                    fx.spawnWorldDebris(mBombWorldPos,
                        glm::vec3((rand()%200-100)*0.01f,(rand()%200-100)*0.01f,(rand()%200)*0.01f), 10.0f);
                explodeBomb(player, npcs);
                checkMatchEnd(player, npcs);
                break;
            }

            // Tick sound once per second with pitch variation
            int curSec = (int)ceilf(mBombTimer);
            if (curSec != mLastTickSecond) {
                mLastTickSecond = curSec;
                mTickPitch = 0.97f + (float)(rand() % 7) * 0.01f;
                playWorldSound("weapon/bomb/bombtick1", mBombWorldPos, 1.0f, mTickPitch, 30.0f);
                if (DebugConfig::DEBUG_BOMBTAG)
                    printf("[BOMBTAG TICK] second=%d pitch=%.2f\n", curSec, mTickPitch);
            }

            // Bomb rendering + world timer
            if (mCamera) {
                bool blink = ((int)ceilf(mBombTimer) % 2) == 0;
                bool isCooldown = mTransferCooldown > 0.0f;
                glm::vec4 col;
                if (isCooldown)
                    col = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f); // black during cooldown
                else if (blink)
                    col = glm::vec4(1.0f, 0.15f, 0.1f, 1.0f);   // red blink
                else
                    col = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);     // white

                DebugVis::drawFilledSphere(*mCamera, mBombWorldPos, 0.5f, col);
                if (DebugConfig::DEBUG_VISUALS_MASTER)
                    DebugVis::drawWireSphere(*mCamera, mBombWorldPos, 0.5f, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));

                // Timer label rendered via screen projection (not gated behind DEBUG_VISUALS_MASTER)
                char timerTxt[32];
                snprintf(timerTxt, sizeof(timerTxt), "%.2f", std::max(0.0f, mBombTimer));
                float sx = 0, sy = 0;
                glm::vec3 labelPos = mBombWorldPos + glm::vec3(0.0f, 0.0f, 1.3f);
                if (DebugVis::projectToScreen(*mCamera, labelPos, sx, sy)) {
                    float tw = uiMeasureText(timerTxt, 0.40f);
                    uiDrawText(timerTxt, sx - tw * 0.5f, sy - 16.0f, 0.40f, col);
                }
            }

            if (DebugConfig::DEBUG_BOMBTAG)
                printf("[BOMBTAG] holder=%s idx=%d time=%.2f cooldown=%.2f pos=(%.1f %.1f %.1f)\n",
                       mBombHolderIsPlayer ? "player" : "npc",
                       mBombHolderIsPlayer ? 0 : mCurrentBombHolderNpc,
                       mBombTimer, mTransferCooldown,
                       mBombWorldPos.x, mBombWorldPos.y, mBombWorldPos.z);

            // NPC bomb AI flags
            for (int i = 0; i < (int)npcs.all().size(); ++i) {
                Npc& npc = npcs.all()[i];
                npc.bombTagActive = true;
                if (!mBombHolderIsPlayer && mCurrentBombHolderNpc == i) {
                    npc.bombTagHasBomb = true;
                    glm::vec3 best = player.pos;
                    float bd = glm::length(player.pos - npc.body.pos);
                    for (int j = 0; j < (int)npcs.all().size(); ++j) {
                        if (j == i || npcs.all()[j].body.dead) continue;
                        float d = glm::length(npcs.all()[j].body.pos - npc.body.pos);
                        if (d < bd) { bd = d; best = npcs.all()[j].body.pos; }
                    }
                    npc.bombTagChaseTarget = best;
                } else {
                    npc.bombTagHasBomb = false;
                    if (mBombHolderIsPlayer)
                        npc.bombTagFleeFrom = player.pos;
                    else if (mCurrentBombHolderNpc >= 0 && mCurrentBombHolderNpc < (int)npcs.all().size())
                        npc.bombTagFleeFrom = npcs.all()[mCurrentBombHolderNpc].body.pos;
                }
                // Force right arm to revolver hold pose for bomb holder
                setArmToWeaponPose(npc.body, npc.bombTagHasBomb);
            }

            // Force player arm to hold pose if holding bomb
            setArmToWeaponPose(player, mBombHolderIsPlayer && !player.dead);

            checkMatchEnd(player, npcs);
            break;
        }

        case BombTagPhase::MatchEnd: {
            if (mEndState == BombTagEndState::None) {
                mEndState = BombTagEndState::VictoryScreen;
                mVictoryTimer = 3.0f;
                freezeAll(player, npcs);
            }
            break;
        }

        default: break;
    }
}

void BombTagManager::renderHud() {
    if (!mConfig.enabled) return;
    float sw = uiScreenW();
    float cx = sw * 0.5f;

    if (mPhase == BombTagPhase::Countdown) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", std::ceil(mCountdown));
        uiDrawText(buf, cx - 20.0f, uiScreenH() * 0.5f - 40.0f, 1.2f, {1,1,1,1});
        return;
    }

    if (mPhase == BombTagPhase::Active) {
        float remaining = std::max(0.0f, (float)mConfig.timeLimitSeconds - mTimer);
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", remaining);
        uiDrawText(buf, cx - 20.0f, 30.0f, 0.45f, {1,1,1,0.8f});

        if (mBombHolderIsPlayer) {
            snprintf(buf, sizeof(buf), "YOU HAVE THE BOMB! (%.0fs)", mBombTimer);
            uiDrawText(buf, cx - 120.0f, 70.0f, 0.4f, {1,0.3f,0.3f,1});
        } else if (mCurrentBombHolderNpc >= 0) {
            snprintf(buf, sizeof(buf), "NPC %d has the bomb", mCurrentBombHolderNpc + 1);
            uiDrawText(buf, cx - 100.0f, 70.0f, 0.35f, {1,0.6f,0.2f,1});
        }

        snprintf(buf, sizeof(buf), "Kills: %d", mPlayerKills);
        uiDrawText(buf, 20.0f, 30.0f, 0.35f, {0.3f,1,0.3f,1});

        if (mConfig.lives > 0) {
            snprintf(buf, sizeof(buf), "Lives: %d/%d", mPlayerLivesRemaining, mConfig.lives);
            uiDrawText(buf, 20.0f, 65.0f, 0.30f, {1,0.85f,0.25f,1});
        }
    }
}

BombTagMenuAction BombTagManager::renderMatchOverScreen(GLFWwindow* win) {
    if (mPhase != BombTagPhase::MatchEnd) return BombTagMenuAction::None;
    float sw = uiScreenW(), sh = uiScreenH();

    if (mEndState == BombTagEndState::VictoryScreen) {
        uiDrawRect({0,0,sw,sh}, {0,0,0,0.6f}, "bt-over");
        char buf[128];
        if (mWinnerIndex == -2) {
            // Stalemate: nobody scored
            uiDrawText("STALEMATE", sw*0.5f-160, sh*0.3f, 1.2f, {0.5f,0.5f,0.5f,1});
            uiDrawText("Nobody scored a kill.", sw*0.5f-140, sh*0.4f, 0.5f, {0.6f,0.6f,0.6f,1});
        } else if (mWinnerIsTie) {
            // Tie: multiple entities share highest kills
            uiDrawText("TIE", sw*0.5f-80, sh*0.3f, 1.2f, {1,1,0.3f,1});
            snprintf(buf, sizeof(buf), "Kills: %d", mWinnerKills);
            uiDrawText(buf, sw*0.5f-80, sh*0.4f, 0.5f, {1,1,0.3f,1});
        } else {
            // Normal win
            bool playerWon = mWinnerKills >= 0 && mWinnerIndex < 0;
            if (playerWon) {
                uiDrawText("WINNER", sw*0.5f-100, sh*0.3f, 1.5f, {0.3f,1,0.3f,1});
            } else {
                uiDrawText("GAME OVER", sw*0.5f-140, sh*0.3f, 1.5f, {1,0.3f,0.3f,1});
            }
            snprintf(buf, sizeof(buf), "Kills: %d  Deaths: %d", mPlayerKills, mPlayerDeaths);
            uiDrawText(buf, sw*0.5f-120, sh*0.42f, 0.5f, {1,1,1,1});
        }
    }

    if (mEndState == BombTagEndState::Countdown || mEndState == BombTagEndState::FinalKillReplay) {
        if (uiButton(win, "PLAY AGAIN", {sw*0.5f-120, sh*0.7f, 240, 50}, {0.2f,0.6f,0.3f,1}).clicked)
            return BombTagMenuAction::PlayAgain;
        if (uiButton(win, "EXIT TO MENU", {sw*0.5f-120, sh*0.7f+60, 240, 50}, {0.6f,0.2f,0.2f,1}).clicked)
            return BombTagMenuAction::ExitToMenu;
    }
    return BombTagMenuAction::None;
}


