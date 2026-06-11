#include "npc-state-machine.h"
#include "npc.h"
#include "npc-navigation.h"

#include <algorithm>
#include <glm/gtc/constants.hpp>

#include "config.h"
#include "debug/debug-log.h"

namespace {

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float difficulty01(float difficulty) { return clamp01(difficulty / 10.0f); }

float random01(unsigned int& state)
{
    state = state * 1664525u + 1013904223u;
    return (float)((state >> 8) & 0x00ffffffu) / (float)0x01000000u;
}

glm::vec3 randomPlanarDirection(unsigned int& state)
{
    float angle = random01(state) * glm::two_pi<float>();
    return {std::cos(angle), std::sin(angle), 0.0f};
}

glm::vec3 rotatePlanar(glm::vec3 v, float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return {
        v.x * c - v.y * s,
        v.x * s + v.y * c,
        0.0f
    };
}

bool shouldJump(Npc& npc, float d01)
{
    return random01(npc.rngState) < (0.05f + d01 * 0.10f);
}

bool shouldDash(Npc& npc, float d01, float distance)
{
    if (npc.dashCooldown > 0.0f)
        return false;
    float chance = 0.10f + d01 * 0.20f;
    if (distance > 5.0f)
        chance += 0.10f;
    return random01(npc.rngState) < chance;
}

float scoreState(NpcState s, const Npc& npc, float d01)
{
    const auto& sensors = npc.sensors;
    float dist = sensors.targetDistance;
    float close01 = 1.0f - clamp01((dist - 2.0f) / 16.0f);
    float far01 = clamp01((dist - 4.0f) / 146.0f);
    float mid01 = 1.0f - std::fabs(dist - 8.0f) / 12.0f;
    float longRange01 = clamp01((dist - 20.0f) / 130.0f);
    float hasTarget = sensors.hasTarget ? 1.0f : 0.0f;
    float agg = npc.tuning.aggression;

    switch (s)
    {
        case NpcState::Idle:
            return (1.0f - hasTarget) * 0.8f;

        case NpcState::RandomWalk:
            return (1.0f - hasTarget) * 0.6f + 0.2f;

        case NpcState::Chase:
            return hasTarget * (0.3f + far01 * 0.6f + (1.0f - close01) * 0.3f * agg);

        case NpcState::Circle:
            return hasTarget * (close01 * 0.5f + mid01 * 0.4f * agg);

        case NpcState::Strafe:
            return hasTarget * (0.15f + mid01 * 0.5f + (1.0f - longRange01) * 0.3f);

        case NpcState::Retreat:
            return hasTarget * (close01 * 0.3f + (1.0f - agg) * 0.2f);

        case NpcState::Attack:
            if (npc.attackCooldown > 0.0f) return 0.0f;
            return hasTarget * (close01 * 0.8f + mid01 * 0.4f + far01 * 0.15f);

        case NpcState::Recover:
            return 0.0f;

        // --- New states ---
        case NpcState::Advance:
            // Advance when at medium-long range with aggression
            return hasTarget * (longRange01 * 0.6f * agg + far01 * 0.3f);

        case NpcState::HoldPosition:
            // Hold at medium range, especially after advancing
            return hasTarget * (mid01 * 0.3f + (1.0f - agg) * 0.15f);

        case NpcState::Peek:
            // Peek at medium-close range with cover awareness
            return hasTarget * (mid01 * 0.25f + close01 * 0.15f);

        case NpcState::Aim:
            // Aim state: stand and shoot at various ranges
            if (npc.attackCooldown > 0.0f) return 0.0f;
            return hasTarget * (mid01 * 0.3f + far01 * 0.2f + close01 * 0.1f);

        case NpcState::ZigZag:
            // Zig-zag approach when at medium range
            return hasTarget * (mid01 * 0.35f * agg + far01 * 0.2f);
    }
    return 0.0f;
}

} // anonymous namespace

float stateMinTime(NpcState s, float d01)
{
    (void)d01;
    switch (s)
    {
        case NpcState::Idle:        return 0.5f;
        case NpcState::RandomWalk:  return 1.0f;
        case NpcState::Chase:       return 0.5f;
        case NpcState::Circle:      return 0.5f;
        case NpcState::Strafe:      return 0.5f;
        case NpcState::Retreat:     return 0.3f;
        case NpcState::Attack:      return 0.1f;
        case NpcState::Recover:     return 0.15f;
        case NpcState::Advance:     return 0.5f;
        case NpcState::HoldPosition: return 0.5f;
        case NpcState::Peek:        return 0.4f;
        case NpcState::Aim:         return 0.2f;
        case NpcState::ZigZag:      return 0.6f;
    }
    return 0.3f;
}

float stateMaxTime(NpcState s, float d01)
{
    float base = 1.0f - d01 * 0.5f;
    switch (s)
    {
        case NpcState::Idle:        return 3.0f * base;
        case NpcState::RandomWalk:  return 4.0f * base;
        case NpcState::Chase:       return 2.0f * base;
        case NpcState::Circle:      return 3.0f * base;
        case NpcState::Strafe:      return 2.5f * base;
        case NpcState::Retreat:     return 1.5f * base;
        case NpcState::Attack:      return 0.3f;
        case NpcState::Recover:     return 0.5f * base;
        case NpcState::Advance:     return 2.0f * base;
        case NpcState::HoldPosition: return 2.5f * base;
        case NpcState::Peek:        return 2.0f * base;
        case NpcState::Aim:         return 1.5f * base;
        case NpcState::ZigZag:      return 2.5f * base;
    }
    return 1.5f * base;
}

std::string npcStateName(NpcState s)
{
    switch (s)
    {
        case NpcState::Idle:        return "IDLE";
        case NpcState::RandomWalk:  return "RANDOMWALK";
        case NpcState::Chase:       return "CHASE";
        case NpcState::Circle:      return "CIRCLE";
        case NpcState::Strafe:      return "STRAFE";
        case NpcState::Retreat:     return "RETREAT";
        case NpcState::Attack:      return "ATTACK";
        case NpcState::Recover:     return "RECOVER";
        case NpcState::Advance:     return "ADVANCE";
        case NpcState::HoldPosition: return "HOLD";
        case NpcState::Peek:        return "PEEK";
        case NpcState::Aim:         return "AIM";
        case NpcState::ZigZag:      return "ZIGZAG";
    }
    return "UNKNOWN";
}

NpcState pickNextState(Npc& npc)
{
    float d01 = difficulty01(npc.difficulty);
    const auto& sensors = npc.sensors;
    float dist = sensors.targetDistance;

    if (NpcNavigation::isStuck(npc))
    {
        npc.stateMachine.stuckTimer += 0.016f;
        if (npc.stateMachine.stuckTimer > 0.3f)
        {
            if (random01(npc.rngState) < 0.5f)
                return NpcState::Chase;
            else
                return NpcState::RandomWalk;
        }
    }
    else
    {
        npc.stateMachine.stuckTimer = 0.0f;
    }

    if (npc.hitReactionTimer > 0.0f && random01(npc.rngState) < 0.6f)
        return NpcState::Recover;

    if (!sensors.hasTarget)
    {
        if (random01(npc.rngState) < 0.4f)
            return NpcState::Idle;
        return NpcState::RandomWalk;
    }

    if (npc.stateMachine.currentState == NpcState::Retreat)
    {
        float maxRetreat = 0.5f + (1.0f - d01) * 2.5f;
        if (npc.stateMachine.retreatTimer > maxRetreat)
        {
            if (dist < 8.0f)
                return NpcState::Circle;
            return NpcState::Chase;
        }
    }

    struct Candidate {
        NpcState state;
        float score;
    };
    std::vector<Candidate> candidates;

    auto add = [&](NpcState s) {
        float score = scoreState(s, npc, d01);
        if (score > 0.01f)
            candidates.push_back({s, score});
    };

    add(NpcState::Chase);
    add(NpcState::Circle);
    add(NpcState::Strafe);
    add(NpcState::Retreat);
    add(NpcState::Attack);
    add(NpcState::Advance);
    add(NpcState::HoldPosition);
    add(NpcState::Peek);
    add(NpcState::Aim);
    add(NpcState::ZigZag);

    float randomness = 0.15f + d01 * 0.20f;
    for (auto& c : candidates)
        c.score *= (1.0f - randomness * random01(npc.rngState));

    if (npc.tuning.aggression > 0.6f && dist > 4.0f)
    {
        for (auto& c : candidates)
            if (c.state == NpcState::Retreat)
                c.score *= 0.1f;
    }

    for (auto& c : candidates)
    {
        if (c.state == npc.stateMachine.currentState)
            c.score *= 0.7f;
    }

    if (candidates.empty())
        return NpcState::Chase;

    auto best = std::max_element(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.score < b.score; });

    return best->state;
}

void computeStateMovement(Npc& npc, glm::vec3& outMoveDir, bool& outJump, bool& outDash, bool& outAttack)
{
    float d01 = difficulty01(npc.difficulty);
    const auto& sensors = npc.sensors;
    auto& sm = npc.stateMachine;

    outJump = false;
    outDash = false;
    outAttack = false;

    glm::vec3 toTarget = sensors.hasTarget
        ? sensors.predictedTarget - npc.body.pos
        : glm::vec3{0.0f};
    toTarget.z = 0.0f;
    float toTargetLen = glm::length(toTarget);
    glm::vec3 chaseDir = toTargetLen > 0.001f ? toTarget / toTargetLen : glm::vec3{1.0f, 0.0f, 0.0f};
    glm::vec3 lateral{-chaseDir.y, chaseDir.x, 0.0f};

    float dist = sensors.targetDistance;

    // === MICRO-MOVEMENT NOISE ===
    // Add small random wobble every 0.1-0.5s for human-like imperfection
    npc.moveNoiseTimer -= 0.016f;
    if (npc.moveNoiseTimer <= 0.0f)
    {
        npc.moveNoiseTimer = 0.1f + random01(npc.rngState) * 0.4f;
        float noiseScale = 0.3f * (1.0f - d01 * 0.5f);
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
            sm.wanderTimer -= 0.016f;
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
            if (sensors.grounded && random01(npc.rngState) < 0.02f)
                outJump = true;
            return;
        }

        case NpcState::Chase:
        {
            outMoveDir = chaseDir + glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;
            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
            if (shouldDash(npc, d01, dist))
                outDash = true;
            return;
        }

        case NpcState::Circle:
        {
            sm.orbitSwapTimer -= 0.016f;
            if (sm.orbitSwapTimer <= 0.0f)
            {
                sm.orbitDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                sm.orbitSwapTimer = 0.1f + random01(npc.rngState) * 3.0f;
                sm.orbitDistance = 1.0f + random01(npc.rngState) * 9.0f;
            }

            float angleSpeed = 2.0f + d01 * 3.0f;
            sm.orbitAngle += angleSpeed * 0.016f * sm.orbitDirection;

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

            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
            if (shouldDash(npc, d01, dist) && random01(npc.rngState) < 0.3f)
                outDash = true;
            return;
        }

        case NpcState::Strafe:
        {
            sm.strafeSwapTimer -= 0.016f;
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

            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
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
            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
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
            if (sensors.grounded && random01(npc.rngState) < 0.3f)
                outJump = true;
            return;
        }

        // --- New states ---
        case NpcState::Advance:
        {
            // Aggressively move toward the player
            outMoveDir = chaseDir;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
            if (shouldDash(npc, d01, dist))
                outDash = true;
            // Shoot while advancing
            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::HoldPosition:
        {
            // Minimal movement, mostly stand and aim
            float swayAmount = 0.15f;
            outMoveDir = lateral * std::sin(sm.holdTimer * 2.0f) * swayAmount;
            outMoveDir += chaseDir * std::cos(sm.holdTimer * 1.7f) * swayAmount * 0.3f;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;
            sm.holdTimer += 0.016f;

            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::Peek:
        {
            // Peek out from cover: move laterally, pause, move back
            sm.peekTimer -= 0.016f;
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

            // Shoot during peek exposure
            if (peekPhase > 0.4f && npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::Aim:
        {
            // Stand and aim carefully - minimal movement
            outMoveDir = glm::vec3(npc.moveOffset, 0.0f) * 0.5f;
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            // Fire when ready
            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::ZigZag:
        {
            // Zig-zag approach toward player
            sm.zigTimer -= 0.016f;
            if (sm.zigTimer <= 0.0f)
            {
                sm.zigPhase = random01(npc.rngState) * glm::two_pi<float>();
                sm.zigTimer = 0.3f + random01(npc.rngState) * 0.8f;
            }

            float zigFreq = 3.0f + d01 * 4.0f;
            float zigAmp = 1.0f + (1.0f - d01) * 2.0f;
            sm.zigPhase += zigFreq * 0.016f;

            outMoveDir = chaseDir + lateral * std::sin(sm.zigPhase) * zigAmp;
            outMoveDir += glm::vec3(npc.moveOffset, 0.0f);
            float len = glm::length(outMoveDir);
            if (len > 0.001f) outMoveDir /= len;

            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }
    }
}
