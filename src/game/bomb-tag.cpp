#include "bomb-tag.h"
#include "spawn-utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
#include "gui/gui-button.h"
#include "devtools/terminal.h"
#include "debug/debug-visuals.h"
#include "effects/effect-part.h"
#include "audio/audio.h"
#include "physics/physics-types.h"
#include "config.h"

namespace {

// Capsule proximity check for bomb transfer.
// Uses actual capsule overlap with slight forgiveness.
static bool bombCapsuleContact(const Capsule& a, const Capsule& b) {
    float aMinZ = std::min(a.a.z, a.b.z) - a.r;
    float aMaxZ = std::max(a.a.z, a.b.z) + a.r;
    float bMinZ = std::min(b.a.z, b.b.z) - b.r;
    float bMaxZ = std::max(b.a.z, b.b.z) + b.r;
    if (aMaxZ <= bMinZ || bMaxZ <= aMinZ) return false;
    glm::vec2 ca = (glm::vec2(a.a.x, a.a.y) + glm::vec2(a.b.x, a.b.y)) * 0.5f;
    glm::vec2 cb = (glm::vec2(b.a.x, b.a.y) + glm::vec2(b.b.x, b.b.y)) * 0.5f;
    return glm::length(ca - cb) < (a.r + b.r) * 1.2f;
}

const char* npcName(int index) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "NPC %d", index + 1);
    return buf;
}

} // namespace

void BombTagManager::start(const BombTagConfig& cfg, Player& player, NpcSystem& npcs, World& world) {
    mConfig = cfg;
    mPhase = BombTagPhase::Countdown;
    mCountdown = 3.0f;
    mTimer = 0.0f;
    mBombTimer = 15.0f;
    mPassCooldown = 0.0f;
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

    // Destroy existing NPCs
    npcs.destroyAll();

    // Spawn NPCs immediately so they exist from the start
    // (they'll be frozen during countdown)
    for (int i = 0; i < cfg.numNpcs; ++i) {
        uint32_t npcId = (uint32_t)(100 + i);
        spawnNpcAtSafePosition(npcs, npcId, cfg.npcDifficulty, world, i);
    }

    // Place player
    glm::vec3 spawnPos = getSpawnPosition(world, cfg.numNpcs); // use different spawnpoint
    player.pos = spawnPos;
    player.vel = glm::vec3(0.0f);
    if (player.dead) {
        player.dead = false;
        player.currentHp = player.maxHp;
        player.spawnFlashTimer = 0.0f;
    }

    // Log diagnostics
    logSpawnDiagnostics(world, player, npcs);

    printf("[BOMB TAG] started: npcs=%d lives=%d time=%ds difficulty=%.0f map=%s\n",
           cfg.numNpcs, cfg.lives, cfg.timeLimitSeconds, cfg.npcDifficulty,
           cfg.mapPath.c_str());
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
        // Pick a living NPC, fall back to any NPC
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
            // Fall back to player
            mBombHolderIsPlayer = true;
            mBombHolderIndex = 0;
            mCurrentBombHolderNpc = -1;
            printf("[BOMB TAG] bomb given to player (no NPCs)\n");
        }
    }
    mBombTimer = 15.0f + (float)(rand() % 5);
}

int BombTagManager::findNearestLivingNpc(const glm::vec3& pos, NpcSystem& npcs, int excludeIndex) {
    float bestDist = 1e9f;
    int best = -1;
    const auto& all = npcs.all();
    for (int i = 0; i < (int)all.size(); ++i) {
        if (i == excludeIndex) continue;
        if (all[i].body.dead) continue;
        float d = glm::distance(pos, all[i].body.pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

int BombTagManager::countLivingNpcs(NpcSystem& npcs) {
    int count = 0;
    for (const auto& n : npcs.all())
        if (!n.body.dead) ++count;
    return count;
}

void BombTagManager::passBomb(Player& player, NpcSystem& npcs) {
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
        int currentNpc = mCurrentBombHolderNpc;
        const auto& all = npcs.all();
        if (currentNpc < 0 || currentNpc >= (int)all.size()) {
            // Invalid holder, give to player
            if (!player.dead) {
                mBombHolderIsPlayer = true;
                mBombHolderIndex = 0;
                mCurrentBombHolderNpc = -1;
                printf("[BOMB TAG] bomb returned to player (invalid holder)\n");
            }
            return;
        }
        glm::vec3 pos = all[currentNpc].body.pos;

        // Check player proximity
        float playerDist = glm::distance(pos, player.pos);
        if (playerDist < 15.0f && !player.dead) {
            mBombHolderIsPlayer = true;
            mBombHolderIndex = 0;
            mCurrentBombHolderNpc = -1;
            mNpcPasses[currentNpc]++;
            printf("[BOMB TAG] %s -> player\n", npcName(currentNpc));
        } else {
            int target = findNearestLivingNpc(pos, npcs, currentNpc);
            if (target >= 0) {
                mBombHolderIsPlayer = false;
                mBombHolderIndex = target;
                mCurrentBombHolderNpc = target;
                mNpcPasses[currentNpc]++;
                printf("[BOMB TAG] %s -> %s\n", npcName(currentNpc), npcName(target));
            }
        }
    }
    mBombTimer = 12.0f + (float)(rand() % 8);
    mPassCooldown = 2.0f;
}

void BombTagManager::explodeBomb(Player& player, NpcSystem& npcs) {
    printf("[BOMB TAG] BOOM! bomb exploded on holder\n");

    // Award kill to the PREVIOUS passer (or nearest living entity as fallback)
    if (mBombHolderIsPlayer) {
        mPlayerDeaths++;
        player.dead = true;
        player.currentHp = 0;
        player.respawnTimer = 3.0f;
        player.killedBy = "Bomb";
        Terminal::instance().addLog("[BOMB TAG] Player exploded");

        int killerNpc = findNearestLivingNpc(player.pos, npcs, -1);
        if (killerNpc >= 0) {
            mNpcKills[killerNpc]++;
            printf("[BOMB TAG] %s +1 kill\n", npcName(killerNpc));
        }
    } else {
        int idx = mCurrentBombHolderNpc;
        if (idx >= 0 && idx < (int)npcs.all().size()) {
            npcs.all()[idx].body.dead = true;
            npcs.all()[idx].body.currentHp = 0;
            Terminal::instance().addLog("[BOMB TAG] " + std::string(npcName(idx)) + " exploded");

            if (!player.dead) {
                mPlayerKills++;
                printf("[BOMB TAG] Player +1 kill\n");
            }
        }
    }

    mBombHolderIsPlayer = false;
    mBombHolderIndex = -1;
    mCurrentBombHolderNpc = -1;

    // Assign bomb to a living entity
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
            printf("[BOMB TAG] no one left to hold the bomb\n");
            mBombTimer = 999.0f;
            return;
        }
    }
    mBombTimer = 15.0f + (float)(rand() % 5);
    printf("[BOMB TAG] new holder: %s\n",
           mBombHolderIsPlayer ? "player" : npcName(mCurrentBombHolderNpc));
}

void BombTagManager::checkMatchEnd(Player& player, NpcSystem& npcs) {
    if (mPlayerLivesRemaining <= 0 && player.dead) {
        mPhase = BombTagPhase::MatchEnd;
        mWinnerIndex = 0;
        mWinnerKills = 0;
        for (int i = 0; i < (int)mNpcKills.size(); ++i)
            if (mNpcKills[i] > mWinnerKills) { mWinnerKills = mNpcKills[i]; mWinnerIndex = i; }
        printf("[BOMB TAG] match ended: player out of lives\n");
        return;
    }
    if (mTimer >= (float)mConfig.timeLimitSeconds) {
        mPhase = BombTagPhase::MatchEnd;
        mWinnerIndex = -1;
        mWinnerKills = mPlayerKills;
        printf("[BOMB TAG] match ended by time limit\n");
        return;
    }
    int livingNpcs = countLivingNpcs(npcs);
    if (livingNpcs == 0) {
        mPhase = BombTagPhase::MatchEnd;
        mWinnerIndex = -1;
        mWinnerKills = mPlayerKills;
        printf("[BOMB TAG] match ended: all NPCs eliminated\n");
        return;
    }
}

void BombTagManager::freezeAll(Player& player, NpcSystem& npcs) {
    player.vel = glm::vec3(0.0f);
    for (auto& n : npcs.all())
        n.body.vel = glm::vec3(0.0f);
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

            // Compute bomb world position from holder's right hand
            mBombWorldPos = getBombWorldPos(player, npcs);

            // Bomb transfer via capsule proximity
            if (mBombHolderIsPlayer && !player.dead) {
                Capsule pc = player.getCapsule();
                for (int i = 0; i < (int)npcs.all().size(); ++i) {
                    if (npcs.all()[i].body.dead) continue;
                    Capsule nc = npcs.all()[i].body.getCapsule();
                    if (bombCapsuleContact(pc, nc)) {
                        printf("[BOMB TAG] transfer: player collided with %s\n", npcName(i));
                        passBomb(player, npcs);
                        break;
                    }
                }
            } else if (!mBombHolderIsPlayer && mCurrentBombHolderNpc >= 0) {
                int idx = mCurrentBombHolderNpc;
                if (idx >= 0 && idx < (int)npcs.all().size() && !npcs.all()[idx].body.dead) {
                    Capsule nc = npcs.all()[idx].body.getCapsule();
                    if (!player.dead) {
                        Capsule pc = player.getCapsule();
                        if (bombCapsuleContact(nc, pc)) {
                            printf("[BOMB TAG] transfer: %s collided with player\n", npcName(idx));
                            passBomb(player, npcs);
                        } else {
                            for (int j = 0; j < (int)npcs.all().size(); ++j) {
                                if (j == idx || npcs.all()[j].body.dead) continue;
                                Capsule oc = npcs.all()[j].body.getCapsule();
                                if (bombCapsuleContact(nc, oc)) {
                                    printf("[BOMB TAG] transfer: %s collided with %s\n",
                                           npcName(idx), npcName(j));
                                    passBomb(player, npcs);
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    assignInitialBomb(player, npcs);
                }
            }

            // Bomb explosion
            if (mBombTimer <= 0.0f) {
                auto& fx = EffectPartSystem::instance();
                fx.spawnDeathEllipsoid(mBombWorldPos, glm::vec3(0,0,1), 6.0f, 3.0f, 2.5f);
                for (int i = 0; i < 10; ++i)
                    fx.spawnWorldDebris(mBombWorldPos,
                        glm::vec3((rand()%200-100)*0.01f,(rand()%200-100)*0.01f,(rand()%200)*0.01f), 10.0f);
                playWorldSound("npc_death", mBombWorldPos, 1.5f, 1.0f, 50.0f);
                explodeBomb(player, npcs);
                checkMatchEnd(player, npcs);
                break;
            }

            // Bomb rendering
            if (mCamera) {
                bool blink = ((int)ceilf(mBombTimer) % 2) == 0;
                glm::vec4 col = blink ? glm::vec4(1.0f, 0.15f, 0.1f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                DebugVis::drawFilledSphere(*mCamera, mBombWorldPos, 0.5f, col);
                DebugVis::drawWireSphere(*mCamera, mBombWorldPos, 0.5f, glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
                char txt[32];
                snprintf(txt, sizeof(txt), "%.2f", std::max(0.0f, mBombTimer));
                DebugVis::drawWorldLabel(mBombWorldPos + glm::vec3(0.0f, 0.0f, 0.8f), txt, col);
            }

            if (DebugConfig::DEBUG_BOMBTAG)
                printf("[BOMBTAG] holder=%s idx=%d time=%.2f pos=(%.1f %.1f %.1f)\n",
                       mBombHolderIsPlayer ? "player" : "npc",
                       mBombHolderIsPlayer ? 0 : mCurrentBombHolderNpc,
                       mBombTimer, mBombWorldPos.x, mBombWorldPos.y, mBombWorldPos.z);

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
            }

            checkMatchEnd(player, npcs);
            break;
        }

        case BombTagPhase::MatchEnd: {
            if (mEndState == BombTagEndState::None) {
                mEndState = BombTagEndState::VictoryScreen;
                mVictoryTimer = 3.0f;
                freezeAll(player, npcs);
            }
            if (mEndState == BombTagEndState::VictoryScreen) {
                mVictoryTimer -= dt;
                if (mVictoryTimer <= 0.0f) {
                    mEndState = BombTagEndState::Countdown;
                    mCountdownTimer = 4.0f;
                    mCurrentCountdownNumber = 3;
                }
            }
            if (mEndState == BombTagEndState::Countdown) {
                mCountdownTimer -= dt;
                int num = (int)std::ceil(mCountdownTimer);
                if (num != mCurrentCountdownNumber) mCurrentCountdownNumber = num;
                if (mCountdownTimer <= 0.0f)
                    mEndState = BombTagEndState::FinalKillReplay;
            }
            break;
        }

        default:
            break;
    }
}

void BombTagManager::renderHud() {
    if (!mConfig.enabled) return;
    float sw = uiScreenW();
    float cx = sw * 0.5f;

    if (mPhase == BombTagPhase::Countdown) {
        char text[64];
        snprintf(text, sizeof(text), "%.0f", std::ceil(mCountdown));
        uiDrawText(text, cx - 20.0f, uiScreenH() * 0.5f - 40.0f, 1.2f, {1, 1, 1, 1});
        return;
    }

    if (mPhase == BombTagPhase::Active) {
        float remaining = std::max(0.0f, (float)mConfig.timeLimitSeconds - mTimer);
        char timerText[32];
        snprintf(timerText, sizeof(timerText), "%.0f", remaining);
        uiDrawText(timerText, cx - 20.0f, 30.0f, 0.45f, {1, 1, 1, 0.8f});

        char bombText[64];
        if (mBombHolderIsPlayer) {
            snprintf(bombText, sizeof(bombText), "YOU HAVE THE BOMB! (%.0fs)", mBombTimer);
            uiDrawText(bombText, cx - 120.0f, 70.0f, 0.4f, {1.0f, 0.3f, 0.3f, 1.0f});
        } else if (mCurrentBombHolderNpc >= 0) {
            snprintf(bombText, sizeof(bombText), "%s has the bomb", npcName(mCurrentBombHolderNpc));
            uiDrawText(bombText, cx - 100.0f, 70.0f, 0.35f, {1.0f, 0.6f, 0.2f, 1.0f});
        }

        char killsText[64];
        snprintf(killsText, sizeof(killsText), "Kills: %d", mPlayerKills);
        uiDrawText(killsText, 20.0f, 30.0f, 0.35f, {0.3f, 1.0f, 0.3f, 1.0f});

        if (mConfig.lives > 0) {
            char livesText[64];
            snprintf(livesText, sizeof(livesText), "Lives: %d/%d",
                     mPlayerLivesRemaining, mConfig.lives);
            uiDrawText(livesText, 20.0f, 65.0f, 0.30f, {1.0f, 0.85f, 0.25f, 1.0f});
        }
        return;
    }
}

BombTagMenuAction BombTagManager::renderMatchOverScreen(GLFWwindow* win) {
    if (mPhase != BombTagPhase::MatchEnd) return BombTagMenuAction::None;
    float sw = uiScreenW(), sh = uiScreenH();

    if (mEndState == BombTagEndState::VictoryScreen) {
        uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.6f}, "victory-dim");
        bool playerWon = (mWinnerIndex < 0);
        uiDrawText(playerWon ? "YOU WIN!" : "GAME OVER",
                   sw * 0.5f - 200.0f, sh * 0.35f, 1.5f,
                   playerWon ? glm::vec4(0.3f,1,0.3f,1) : glm::vec4(1,0.3f,0.3f,1));
        char buf[64];
        snprintf(buf, sizeof(buf), "Kills: %d  Deaths: %d", mPlayerKills, mPlayerDeaths);
        uiDrawText(buf, sw * 0.5f - 120.0f, sh * 0.45f, 0.5f, {1,1,1,1});
        return BombTagMenuAction::None;
    }

    if (mEndState == BombTagEndState::Countdown) {
        uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.6f}, "countdown-dim");
        char numText[8];
        snprintf(numText, sizeof(numText), "%d", mCurrentCountdownNumber);
        uiDrawText(numText, sw * 0.5f - 30.0f, sh * 0.5f - 40.0f, 2.0f, {1,1,1,1});
        return BombTagMenuAction::None;
    }

    if (mEndState == BombTagEndState::FinalKillReplay) {
        if (uiButton(win, "PLAY AGAIN",
            {sw * 0.5f - 120.0f, sh * 0.7f, 240.0f, 50.0f},
            {0.2f, 0.6f, 0.3f, 1.0f}).clicked)
            return BombTagMenuAction::PlayAgain;
        if (uiButton(win, "EXIT TO MENU",
            {sw * 0.5f - 120.0f, sh * 0.7f + 60.0f, 240.0f, 50.0f},
            {0.6f, 0.2f, 0.2f, 1.0f}).clicked)
            return BombTagMenuAction::ExitToMenu;
        return BombTagMenuAction::None;
    }

    return BombTagMenuAction::None;
}

glm::vec3 BombTagManager::getBombWorldPos(Player& player, NpcSystem& npcs) {
    if (mBombHolderIsPlayer) {
        for (const auto& p : player.physicalBody.parts)
            if (p.name == "rightArm") {
                glm::vec3 hp = glm::vec3(p.worldTransform[3]);
                return hp + glm::normalize(glm::vec3(p.worldTransform[1])) * 0.3f;
            }
        return player.pos + glm::vec3(0.5f, 0.0f, 1.5f);
    }
    if (mCurrentBombHolderNpc >= 0 && mCurrentBombHolderNpc < (int)npcs.all().size()) {
        for (const auto& p : npcs.all()[mCurrentBombHolderNpc].body.physicalBody.parts)
            if (p.name == "rightArm") {
                glm::vec3 hp = glm::vec3(p.worldTransform[3]);
                return hp + glm::normalize(glm::vec3(p.worldTransform[1])) * 0.3f;
            }
        return npcs.all()[mCurrentBombHolderNpc].body.pos + glm::vec3(0.5f, 0.0f, 1.5f);
    }
    return glm::vec3(0.0f);
}
