#include "npc-state-machine.h"
#include "npc.h"

#include <algorithm>
#include <glm/gtc/constants.hpp>

#include "npc/npc-internal.h"
#include "npc/npc-difficulty-config.h"
#include "npc/npc-movement-policy.h"

void computeStateMovement(Npc& npc, glm::vec3& outMoveDir, bool& outJump, bool& outDash, bool& outAttack, float dt)
{
    float d01 = difficulty01(npc.difficulty);
    const auto& sensors = npc.sensors;
    auto& sm = npc.stateMachine;

    outJump = false;
    outDash = false;
    outAttack = false;

    // Visible target: use the current predicted position. Hidden target with
    // memory: use the position where the target was last seen. The old code
    // used zero here, which normalized to +X and made Chase run in an
    // unrelated direction after the player went behind cover.
    const bool hasRememberedTarget = !sensors.hasTarget && npc.targetMemory.hasMemory;
    glm::vec3 rememberedTarget = npc.targetMemory.lastKnownPosition;
    if (hasRememberedTarget && npc.behavior.active &&
        npc.behavior.continuePredictedPath)
    {
        // When the target is hidden and radar is delayed or unavailable, keep
        // moving along the target's last known travel direction instead of
        // stopping at the first corner.
        const float predictionSeconds = std::min(npc.targetMemory.ageSeconds, 1.5f);
        rememberedTarget += npc.targetMemory.lastKnownVelocity * predictionSeconds;
    }
    glm::vec3 toTarget = sensors.hasTarget
        ? sensors.predictedTarget - npc.body.pos
        : (hasRememberedTarget ? rememberedTarget - npc.body.pos : glm::vec3{0.0f});
    toTarget.z = 0.0f;
    float toTargetLen = glm::length(toTarget);
    glm::vec3 chaseDir = toTargetLen > 0.001f ? toTarget / toTargetLen : glm::vec3{1.0f, 0.0f, 0.0f};
    if (npc.stateMachine.pursuitSearchActive &&
        glm::length(npc.stateMachine.pursuitSearchDir) > 0.001f)
        chaseDir = glm::normalize(glm::vec3(npc.stateMachine.pursuitSearchDir.x,
                                            npc.stateMachine.pursuitSearchDir.y, 0.0f));
    glm::vec3 lateral{-chaseDir.y, chaseDir.x, 0.0f};

    float dist = sensors.targetDistance;

    // An actor-preset movement policy owns its own directional noise. A value
    // of 0.0 means the actor moves directly (no jitter at all).
    const NpcMovementPolicy* policy = activeMovementPolicy(npc);
    const float policyNoise = policy ? std::clamp(policy->movementNoise, 0.0f, 1.0f) : 1.0f;

    npc.moveNoiseTimer -= dt;
    if (policyNoise <= 0.0f)
    {
        npc.moveOffset = {0.0f, 0.0f};
    }
    else if (npc.moveNoiseTimer <= 0.0f)
    {
        const auto& cfg = NpcDifficultyConfig::instance().settings();
        npc.moveNoiseTimer = 0.1f + random01(npc.rngState) * 0.4f;
        float noiseScale = 0.3f * (1.0f - d01 * 0.5f) * cfg.movementNoiseScale * policyNoise;
        npc.moveOffset.x = (random01(npc.rngState) * 2.0f - 1.0f) * noiseScale;
        npc.moveOffset.y = (random01(npc.rngState) * 2.0f - 1.0f) * noiseScale;
    }

    switch (sm.currentState)
    {
        case NpcState::Idle:
            outMoveDir = {0.0f, 0.0f, 0.0f};
            return;

        case NpcState::RandomWalk:
        {
            sm.wanderTimer -= dt;
            if (sm.wanderTimer <= 0.0f)
            {
                float range = 4.0f + random01(npc.rngState) * 8.0f;
                sm.wanderTarget = npc.body.pos + randomPlanarDirection(npc.rngState) * range;
                sm.wanderTimer = 2.0f + random01(npc.rngState) * 3.0f;
            }
            glm::vec3 toWander = sm.wanderTarget - npc.body.pos;
            toWander.z = 0.0f;
            float wLen = glm::length(toWander);
            if (wLen < 1.0f)
            {
                sm.wanderTimer = 0.0f;
                outMoveDir = {0.0f, 0.0f, 0.0f};
            }
            else
            {
                outMoveDir = toWander / wLen;
            }
            return;
        }

        case NpcState::Chase:
        {
            outMoveDir = chaseDir + glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;
            if (npc.attackCooldown <= 0.0f && sensors.hasTarget)
                outAttack = true;
            return;
        }

        case NpcState::Circle:
        {
            sm.orbitSwapTimer -= dt;
            if (sm.orbitSwapTimer <= 0.0f)
            {
                sm.orbitDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                sm.orbitSwapTimer = 0.1f + random01(npc.rngState) * 3.0f;
                sm.orbitDistance = 1.0f + random01(npc.rngState) * 9.0f;
            }

            float angleSpeed = 2.0f + d01 * 3.0f;
            sm.orbitAngle += angleSpeed * dt * sm.orbitDirection;

            glm::vec3 orbitTarget = sensors.hasTarget
                ? sensors.targetPos
                : sm.lastKnownTarget;
            glm::vec3 orbitPos = orbitTarget + glm::vec3{
                std::cos(sm.orbitAngle) * sm.orbitDistance,
                std::sin(sm.orbitAngle) * sm.orbitDistance,
                0.0f
            };

            glm::vec3 toOrbit = orbitPos - npc.body.pos;
            toOrbit.z = 0.0f;
            float oLen = glm::length(toOrbit);
            outMoveDir = oLen > 0.001f ? toOrbit / oLen : lateral * sm.orbitDirection;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;
            if (npc.attackCooldown <= 0.0f && sensors.hasTarget)
                outAttack = true;
            return;
        }

        case NpcState::Strafe:
        {
            sm.strafeSwapTimer -= dt;
            if (sm.strafeSwapTimer <= 0.0f)
            {
                sm.strafeDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                sm.strafeSwapTimer = 0.3f + random01(npc.rngState) * 2.0f;
            }

            float towardBias = dist > 12.0f ? 0.4f : (dist < 5.0f ? -0.3f : 0.0f);
            outMoveDir = chaseDir * towardBias + lateral * sm.strafeDirection;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::Retreat:
        {
            outMoveDir = -chaseDir;
            outMoveDir += lateral * (random01(npc.rngState) * 2.0f - 1.0f) * 0.3f;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float rLen = glm::length(outMoveDir);
            if (rLen > 0.001f)
                outMoveDir /= rLen;

            if (npc.attackCooldown <= 0.0f && dist < 20.0f)
                outAttack = true;
            return;
        }

        case NpcState::Attack:
        {
            outAttack = true;
            outMoveDir = chaseDir * 0.5f + lateral * (random01(npc.rngState) * 2.0f - 1.0f) * 0.3f;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float aLen = glm::length(outMoveDir);
            if (aLen > 0.001f)
                outMoveDir /= aLen;
            return;
        }

        case NpcState::Recover:
        {
            outMoveDir = lateral * (random01(npc.rngState) * 2.0f - 1.0f);
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;
            return;
        }

        case NpcState::Advance:
        {
            outMoveDir = chaseDir;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::Patrol:
        {
            // No target: keep advancing forward along the navigator's committed
            // direction (the single direction owner). Walls are handled
            // centrally by wall avoidance + navigator replan/recommit.
            if (npc.navigator.commitmentActive &&
                glm::length(glm::vec3(npc.navigator.committedDirection.x,
                                      npc.navigator.committedDirection.y, 0.0f)) > 0.001f) {
                outMoveDir = glm::vec3(npc.navigator.committedDirection.x,
                                       npc.navigator.committedDirection.y, 0.0f);
            } else {
                outMoveDir = glm::length(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f)) > 0.001f
                    ? glm::normalize(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f))
                    : glm::vec3(1.0f, 0.0f, 0.0f);
            }
            return;
        }

        case NpcState::HoldPosition:
        {
            float swayAmount = 0.15f;
            outMoveDir = lateral * std::sin(sm.holdTimer * 2.0f) * swayAmount;
            outMoveDir += chaseDir * std::cos(sm.holdTimer * 1.7f) * swayAmount * 0.3f;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;
            sm.holdTimer += dt;

            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::Peek:
        {
            sm.peekTimer -= dt;
            if (sm.peekTimer <= 0.0f)
            {
                sm.peekDir *= -1.0f;
                sm.peekTimer = 0.5f + random01(npc.rngState) * 1.0f;
                sm.peekStartPos = npc.body.pos;
            }

            float peekPhase = std::abs(std::sin(sm.peekTimer * 3.0f));
            float peekStrength = 1.0f + dist * 0.02f;
            outMoveDir = lateral * sm.peekDir * peekPhase * peekStrength;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            if (peekPhase > 0.4f && npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::Aim:
        {
            outMoveDir = glm::vec3(npc.moveOffset, 0.0f) * 0.5f;
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::ZigZag:
        {
            const auto& cfg = NpcDifficultyConfig::instance().settings();
            sm.zigTimer -= dt;
            if (sm.zigTimer <= 0.0f)
            {
                sm.zigPhase = random01(npc.rngState) * glm::two_pi<float>();
                sm.zigTimer = (0.3f + random01(npc.rngState) * 0.8f) / std::max(cfg.jukeFrequency, 0.1f);
            }

            float zigFreq = (3.0f + d01 * 4.0f) * cfg.jukeFrequency;
            float zigAmp = 1.0f + (1.0f - d01) * 2.0f;
            sm.zigPhase += zigFreq * dt;

            outMoveDir = chaseDir + lateral * std::sin(sm.zigPhase) * zigAmp;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }
    }
}
