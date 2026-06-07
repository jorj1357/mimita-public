#include "npc-state-machine.h"
#include "npc.h"
#include "npc-navigation.h"

#include <algorithm>
#include <glm/gtc/constants.hpp>

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
    // Jump chance per decision: 5-15% based on difficulty
    return random01(npc.rngState) < (0.05f + d01 * 0.10f);
}

bool shouldDash(Npc& npc, float d01, float distance)
{
    if (npc.dashCooldown > 0.0f)
        return false;
    // Dash when chasing at range, or when evading
    float chance = 0.10f + d01 * 0.20f;
    if (distance > 5.0f)
        chance += 0.10f;
    return random01(npc.rngState) < chance;
}

// Utility score for each state given current conditions
float scoreState(NpcState s, const Npc& npc, float d01)
{
    const auto& sensors = npc.sensors;
    float dist = sensors.targetDistance;
    float close01 = 1.0f - clamp01((dist - 2.0f) / 16.0f);
    float far01 = clamp01((dist - 4.0f) / 24.0f);
    float mid01 = 1.0f - std::fabs(dist - 8.0f) / 12.0f;
    float hasTarget = sensors.hasTarget ? 1.0f : 0.0f;
    float agg = npc.tuning.aggression;

    switch (s)
    {
        case NpcState::Idle:
            return (1.0f - hasTarget) * 0.8f;

        case NpcState::RandomWalk:
            return (1.0f - hasTarget) * 0.6f + 0.2f;

        case NpcState::Chase:
            // Chase when far or no target recently seen
            return hasTarget * (0.3f + far01 * 0.6f + (1.0f - close01) * 0.3f * agg);

        case NpcState::Circle:
            // Circle at medium-close range, higher aggression = more circling
            return hasTarget * (close01 * 0.5f + mid01 * 0.4f * agg);

        case NpcState::Strafe:
            // Strafe at medium range
            return hasTarget * (0.15f + mid01 * 0.5f + far01 * 0.2f);

        case NpcState::Retreat:
            // Retreat when very close (dodge) or after being hit recently
            return hasTarget * (close01 * 0.3f + (1.0f - agg) * 0.2f);

        case NpcState::Attack:
            // Attack when in range and cooldown ready
            if (npc.attackCooldown > 0.0f) return 0.0f;
            return hasTarget * (close01 * 0.8f + mid01 * 0.4f + far01 * 0.15f);

        case NpcState::Recover:
            // Only when recently hit
            return 0.0f;  // triggered externally via hitReactionTimer
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
    }
    return "UNKNOWN";
}

// Pick next state using utility scoring + forced transitions
NpcState pickNextState(Npc& npc)
{
    float d01 = difficulty01(npc.difficulty);
    const auto& sensors = npc.sensors;
    float dist = sensors.targetDistance;

    // Handle stuck: force jump + new direction regardless of state
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

    // If hit recently, force Recover
    if (npc.hitReactionTimer > 0.0f && random01(npc.rngState) < 0.6f)
        return NpcState::Recover;

    // No target → Idle or RandomWalk
    if (!sensors.hasTarget)
    {
        if (random01(npc.rngState) < 0.4f)
            return NpcState::Idle;
        return NpcState::RandomWalk;
    }

    // Retreat forced timeout: never retreat longer than max
    if (npc.stateMachine.currentState == NpcState::Retreat)
    {
        float maxRetreat = 0.5f + (1.0f - d01) * 2.5f;
        if (npc.stateMachine.retreatTimer > maxRetreat)
        {
            // After retreat timeout, go aggressive
            if (dist < 8.0f)
                return NpcState::Circle;
            return NpcState::Chase;
        }
    }

    // Score all candidate states
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

    // Avoid picking same state if score is low
    float randomness = 0.15f + d01 * 0.20f;
    for (auto& c : candidates)
        c.score *= (1.0f - randomness * random01(npc.rngState));

    // Never pick retreat at high aggression unless very close
    if (npc.tuning.aggression > 0.6f && dist > 4.0f)
    {
        for (auto& c : candidates)
            if (c.state == NpcState::Retreat)
                c.score *= 0.1f;
    }

    // Add bias toward changing state (avoid staying forever)
    for (auto& c : candidates)
    {
        if (c.state == npc.stateMachine.currentState)
            c.score *= 0.7f; // slight penalty to staying
    }

    if (candidates.empty())
        return NpcState::Chase;

    auto best = std::max_element(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.score < b.score; });

    return best->state;
}

// Compute movement direction and flags for the current state
void computeStateMovement(Npc& npc, glm::vec3& outMoveDir, bool& outJump, bool& outDash, bool& outAttack)
{
    float d01 = difficulty01(npc.difficulty);
    const auto& sensors = npc.sensors;
    auto& sm = npc.stateMachine;

    outJump = false;
    outDash = false;
    outAttack = false;

    // Direction toward target (planar)
    glm::vec3 toTarget = sensors.hasTarget
        ? sensors.predictedTarget - npc.body.pos
        : glm::vec3{0.0f};
    toTarget.z = 0.0f;
    float toTargetLen = glm::length(toTarget);
    glm::vec3 chaseDir = toTargetLen > 0.001f ? toTarget / toTargetLen : glm::vec3{1.0f, 0.0f, 0.0f};
    glm::vec3 lateral{-chaseDir.y, chaseDir.x, 0.0f};

    float dist = sensors.targetDistance;

    switch (sm.currentState)
    {
        case NpcState::Idle:
            outMoveDir = {0.0f, 0.0f, 0.0f};
            // Stand still, slight look movement
            return;

        case NpcState::RandomWalk:
        {
            // Walk toward wander target
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
                sm.wanderTimer = 0.0f; // force repick
                outMoveDir = {0.0f, 0.0f, 0.0f};
            }
            else
            {
                outMoveDir = toWander / wLen;
            }
            // Random jump while wandering
            if (sensors.grounded && random01(npc.rngState) < 0.02f)
                outJump = true;
            return;
        }

        case NpcState::Chase:
        {
            outMoveDir = chaseDir;
            // Jump when approaching obstacle
            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
            // Dash to close distance
            if (shouldDash(npc, d01, dist))
                outDash = true;
            return;
        }

        case NpcState::Circle:
        {
            // Orbit around target at preferred distance
            sm.orbitSwapTimer -= 0.016f;
            if (sm.orbitSwapTimer <= 0.0f)
            {
                sm.orbitDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                sm.orbitSwapTimer = 0.1f + random01(npc.rngState) * 3.0f;
                sm.orbitDistance = 1.0f + random01(npc.rngState) * 9.0f;
            }

            float angleSpeed = 2.0f + d01 * 3.0f;
            sm.orbitAngle += angleSpeed * 0.016f * sm.orbitDirection;

            // Compute orbit position
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

            // Move laterally + slightly toward/away
            float towardBias = dist > 12.0f ? 0.4f : (dist < 5.0f ? -0.3f : 0.0f);
            outMoveDir = chaseDir * towardBias + lateral * sm.strafeDirection;

            // Strafe + jump
            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;

            // Attack while strafing
            if (npc.attackCooldown <= 0.0f)
                outAttack = true;
            return;
        }

        case NpcState::Retreat:
        {
            // Move away from target
            outMoveDir = -chaseDir;
            // Add lateral wobble
            outMoveDir += lateral * (random01(npc.rngState) * 2.0f - 1.0f) * 0.3f;
            float rLen = glm::length(outMoveDir);
            if (rLen > 0.001f)
                outMoveDir /= rLen;

            // Shoot while retreating
            if (npc.attackCooldown <= 0.0f && dist < 20.0f)
                outAttack = true;

            // Jump over obstacles while retreating
            if (sensors.grounded && shouldJump(npc, d01))
                outJump = true;
            return;
        }

        case NpcState::Attack:
        {
            // Fire weapon (handled by combat system)
            outAttack = true;
            // Keep moving slightly while attacking
            outMoveDir = chaseDir * 0.5f + lateral * (random01(npc.rngState) * 2.0f - 1.0f) * 0.3f;
            float aLen = glm::length(outMoveDir);
            if (aLen > 0.001f)
                outMoveDir /= aLen;
            return;
        }

        case NpcState::Recover:
        {
            // Briefly move erratically
            outMoveDir = lateral * (random01(npc.rngState) * 2.0f - 1.0f);
            // Maybe jump
            if (sensors.grounded && random01(npc.rngState) < 0.3f)
                outJump = true;
            return;
        }
    }
}
