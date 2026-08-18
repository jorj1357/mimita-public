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
#include "camera.h"
#include "replay/replay.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"

#include "combat/weapon-godball.h"
#include "debug/debug-visuals.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
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

} // namespace

void BombTagManager::freezeAll(Player& player, NpcSystem& npcs) {
    player.vel = glm::vec3(0.0f);
    for (auto& n : npcs.all())
        n.body.vel = glm::vec3(0.0f);
}

int BombTagManager::countLivingNpcs(NpcSystem& npcs) {
    int cnt = 0;
    for (const auto& n : npcs.all())
        if (!n.body.dead) ++cnt;
    return cnt;
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
                const auto& deCfg = HitEffects::config().deathEllipsoid;
                fx.spawnDeathEllipsoid(mBombWorldPos, glm::vec3(0,0,1),
                                       deCfg.length, deCfg.radius, deCfg.lifetime);
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
