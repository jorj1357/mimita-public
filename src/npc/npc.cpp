#include "npc.h"
#include "npc/npc-internal.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "combat/weapon-hit.h"
#include "debug/debug-log.h"
#include "perf/perf.h"
#include "physics/config.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "render/render-player.h"
#include "world/world.h"
#include "audio/audio.h"
#include "effects/effect-part.h"
#include "devtools/dev-npc-selection.h"
#include "npc/npc-navigation.h"
#include "npc/npc-combat.h"
#include "perf/perf.h"
#include "npc/npc-state-machine.h"

namespace {

glm::vec3 safePlanarNormal(glm::vec3 v, glm::vec3 fallback)
{
    v.z = 0.0f;
    float len = glm::length(v);
    if (len < 0.0001f)
        return fallback;
    return v / len;
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

static float reactionDelayForDifficulty(float difficulty) {
    float d01 = std::clamp(difficulty / 10.0f, 0.0f, 1.0f);
    float baseMs = 1500.0f - d01 * 1500.0f;
    float jitterMs = 200.0f + (1.0f - d01) * 300.0f;
    float totalMs = baseMs + (float)(rand() % (int)(jitterMs + 1.0f));
    return totalMs / 1000.0f;
}

static glm::vec3 delayedTarget(const Npc& npc, const glm::vec3& currentPos,
                                const glm::vec3& currentVel, float delaySeconds) {
    if (delaySeconds <= 0.001f || npc.posRingCount == 0)
        return currentPos;

    int tail = (npc.posRingHead - 1 + Npc::MAX_HISTORY_SAMPLES) % Npc::MAX_HISTORY_SAMPLES;
    float targetTime = npc.posRing[tail].time - delaySeconds;
    glm::vec3 bestPos = currentPos;
    glm::vec3 bestVel = currentVel;

    int count = std::min(npc.posRingCount, Npc::MAX_HISTORY_SAMPLES);
    for (int j = 0; j < count; ++j) {
        int i = (tail - j + Npc::MAX_HISTORY_SAMPLES) % Npc::MAX_HISTORY_SAMPLES;
        const auto& s = npc.posRing[i];
        if (s.time >= targetTime) {
            bestPos = s.pos;
            bestVel = s.vel;
        } else {
            int nextIdx = (i + 1) % Npc::MAX_HISTORY_SAMPLES;
            if (j > 0 && nextIdx < Npc::MAX_HISTORY_SAMPLES) {
                float t = (targetTime - s.time) / (npc.posRing[nextIdx].time - s.time);
                t = std::clamp(t, 0.0f, 1.0f);
                bestPos = glm::mix(s.pos, npc.posRing[nextIdx].pos, t);
                bestVel = glm::mix(s.vel, npc.posRing[nextIdx].vel, t);
            }
            break;
        }
    }

    float predictTime = delaySeconds * (1.0f - std::clamp((npc.difficulty / 10.0f), 0.0f, 1.0f));
    return bestPos + bestVel * predictTime;
}

void senseWorld(Npc& npc, const Player& player, float dt)
{
    NpcSensorContext sensors;
    sensors.selfVel = npc.body.vel + npc.body.externalImpulse;
    sensors.grounded = npc.body.ground.onGround;

    {
        int i = npc.posRingHead;
        npc.posRing[i] = {player.pos, player.vel + player.externalImpulse, npc.sensors.time + dt};
        npc.posRingHead = (i + 1) % Npc::MAX_HISTORY_SAMPLES;
        if (npc.posRingCount < Npc::MAX_HISTORY_SAMPLES)
            npc.posRingCount++;
    }

    float delay = reactionDelayForDifficulty(npc.difficulty);

    glm::vec3 rawPos = player.pos;
    glm::vec3 rawVel = player.vel + player.externalImpulse;
    sensors.targetPos = delayedTarget(npc, rawPos, rawVel, delay);
    sensors.targetVel = rawVel;
    sensors.toTarget = sensors.targetPos - npc.body.pos;
    sensors.targetDistance = glm::length(sensors.toTarget);
    sensors.hasTarget = npc.difficulty > 0.05f && sensors.targetDistance <= npc.tuning.awarenessRange;
    sensors.predictedTarget = sensors.targetPos + sensors.targetVel * (0.10f + npc.tuning.prediction * 0.55f);

    npc.previousPosition = npc.body.pos;
    npc.sensors = sensors;

    if (sensors.hasTarget)
    {
        npc.stateMachine.lastKnownTarget = sensors.targetPos;
        npc.stateMachine.lastKnownAge = 0.0f;
    }
    else
    {
        npc.stateMachine.lastKnownAge += dt;
    }

    if (sensors.hasTarget && npc.lastTargetLogDistance < 0.0f)
    {
        Debug::log(Debug::Category::General,
                   "[NPC] id=%u target acquired difficulty=%.1f distance=%.2f delay=%.0fms\n",
                   npc.id, npc.difficulty, sensors.targetDistance, delay * 1000.0f);
    }
    if (!sensors.hasTarget && npc.lastTargetLogDistance >= 0.0f)
    {
        Debug::log(Debug::Category::General,
                   "[NPC] id=%u target lost difficulty=%.1f\n",
                   npc.id);
    }
    npc.lastTargetLogDistance = sensors.hasTarget ? sensors.targetDistance : -1.0f;
}

void logStateChange(const Npc& npc, NpcState oldState, NpcState newState)
{
    if (oldState == newState)
        return;
    Debug::log(
        Debug::Category::General,
        "[NPC] id=%u difficulty=%.1f %s -> %s target=%d dist=%.2f\n",
        npc.id,
        npc.difficulty,
        npcStateName(oldState).c_str(),
        npcStateName(newState).c_str(),
        (int)npc.sensors.hasTarget,
        npc.sensors.targetDistance
    );
}

InputState buildInputState(const Npc& npc, glm::vec3 moveDir, bool jump, bool dash, bool attack, bool downDash)
{
    InputState input;
    input.wishMoveXY = {moveDir.x, moveDir.y};
    input.movementPressed = glm::length(moveDir) > 0.001f;
    input.jumpHeld = jump;
    input.jumpPressed = jump;
    input.dashPressed = dash;
    input.groundReturnPressed = false;
    input.downDashPressed = downDash;
    input.freezeHeld = false;

    if (npc.sensors.hasTarget)
    {
        glm::vec3 toTarget = npc.sensors.predictedTarget - npc.body.pos;
        input.camForward = safePlanarNormal(toTarget, {1.0f, 0.0f, 0.0f});
    }
    else
    {
        input.camForward = safePlanarNormal(moveDir, {1.0f, 0.0f, 0.0f});
    }

    return input;
}

} // anonymous namespace

void NpcSystem::update(const World& world, Player& player, float dt)
{
    Perf::ScopedTimer _updateTimer("NpcUpdate");
    for (Npc& npc : npcs)
        updateOneNpc(npc, world, player, dt);
}

void NpcSystem::updateOneNpc(Npc& npc, const World& world, Player& player, float dt)
{
    if (npc.body.dead) {
        npc.body.updateModelWorldTransforms();
        return;
    }

    float safeDt = std::max(dt, 0.0001f);

    npc.dashCooldown = std::max(0.0f, npc.dashCooldown - safeDt);
    npc.downDashCooldown = std::max(0.0f, npc.downDashCooldown - safeDt);
    npc.attackCooldown = std::max(0.0f, npc.attackCooldown - safeDt);
    npc.hitReactionTimer = std::max(0.0f, npc.hitReactionTimer - safeDt);
    npc.aimTimer = std::max(0.0f, npc.aimTimer);
    npc.stateMachine.stateTimer += safeDt;
    npc.stateMachine.nextDecisionTime -= safeDt;
    npc.stateMachine.retreatTimer += safeDt;

    senseWorld(npc, player, safeDt);

    bool wantDownDash = false;
    if (npc.sensors.hasTarget && !npc.sensors.grounded && npc.downDashCooldown <= 0.0f)
    {
        float heightAbove = npc.body.pos.z - npc.sensors.targetPos.z;
        if (heightAbove > 3.0f)
            wantDownDash = true;
    }

    if (npc.hitReactionTimer > 0.0f)
    {
        npc.stateMachine.currentState = NpcState::Recover;
        npc.stateMachine.recoverTimer = npc.hitReactionTimer;
        npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime, npc.hitReactionTimer + 0.1f);
    }

    if (npc.trainingMode != 2) {
        if (npc.trainingMode == 0) {
            npc.stateMachine.currentState = NpcState::Idle;
            npc.stateMachine.nextDecisionTime = 2.0f;
        } else if (npc.trainingMode == 1) {
            npc.stateMachine.currentState = NpcState::Retreat;
            npc.stateMachine.retreatTimer = 0.0f;
            npc.stateMachine.nextDecisionTime = 0.3f;
        }
    } else {
        if (npc.stateMachine.nextDecisionTime <= 0.0f)
        {
            NpcState oldState = npc.stateMachine.currentState;
            NpcState newState = pickNextState(npc);

            if (newState == NpcState::Retreat && oldState != NpcState::Retreat)
                npc.stateMachine.retreatTimer = 0.0f;

            if (newState == NpcState::Recover)
                npc.stateMachine.recoverTimer = 0.2f + random01(npc.rngState) * 0.3f;

            if (newState == NpcState::Circle && oldState != NpcState::Circle)
            {
                npc.stateMachine.orbitSwapTimer = 0.1f + random01(npc.rngState) * 1.5f;
                npc.stateMachine.orbitDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                npc.stateMachine.orbitDistance = 1.0f + random01(npc.rngState) * 9.0f;
            }

            if (newState == NpcState::Strafe && oldState != NpcState::Strafe)
            {
                npc.stateMachine.strafeDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                npc.stateMachine.strafeSwapTimer = 0.3f + random01(npc.rngState) * 2.0f;
            }

            logStateChange(npc, oldState, newState);
            npc.stateMachine.previousState = oldState;
            npc.stateMachine.currentState = newState;
            npc.stateMachine.stateTimer = 0.0f;

            float minT = stateMinTime(newState, difficulty01(npc.difficulty));
            float maxT = stateMaxTime(newState, difficulty01(npc.difficulty));
            npc.stateMachine.nextDecisionTime = minT + random01(npc.rngState) * (maxT - minT);
        }
    }

    glm::vec3 moveDir;
    bool jump, dash, attack;
    computeStateMovement(npc, moveDir, jump, dash, attack);

    if (npc.bombTagActive)
    {
        if (npc.bombTagHasBomb)
        {
            glm::vec3 toTarget = npc.bombTagChaseTarget - npc.body.pos;
            float dist = glm::length(toTarget);
            if (dist > 0.5f)
            {
                moveDir = toTarget / dist;
                jump = dist > 2.0f && npc.body.pos.z < npc.bombTagChaseTarget.z - 0.5f;
                attack = false;
                dash = dist > 4.0f && npc.dashCooldown <= 0.0f;
            }
            npc.sensors.hasTarget = true;
            npc.sensors.targetPos = npc.bombTagChaseTarget;
        }
        else
        {
            glm::vec3 fromTarget = npc.body.pos - npc.bombTagFleeFrom;
            float dist = glm::length(fromTarget);
            if (dist > 0.1f)
            {
                moveDir = fromTarget / dist;
                if (dist < 3.0f && npc.dashCooldown <= 0.0f)
                    dash = true;
            }
            if (dist < 8.0f)
                npc.sensors.hasTarget = true;
        }
    }

    {
        Perf::ScopedTimer _pathTimer("NpcPathfinding");
        if (glm::length(moveDir) > 0.001f)
            moveDir = NpcNavigation::wallAvoidDirection(npc, moveDir, world);

        if (NpcNavigation::isStuck(npc))
        {
            npc.stateMachine.stuckTimer += safeDt;
            if (npc.stateMachine.stuckTimer > 0.3f)
            {
                moveDir = NpcNavigation::unstuckDirection(npc, npc.rngState, world);
                jump = true;
                dash = npc.dashCooldown <= 0.0f;
                npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime, 0.3f);
            }
        }
        else
        {
            npc.stateMachine.stuckTimer = 0.0f;
        }
    }

    InputState input = buildInputState(npc, moveDir, jump, dash, attack, wantDownDash);
    if (input.dashPressed)
        npc.dashCommandConsumed = true;

    bool downDashAvailableBefore = npc.body.dash.downDashAvailable;
    {
        Perf::ScopedTimer _npcCollision("NpcCollision");
        glm::vec3 velocityBefore = npc.body.vel;
        float planarSpeedBefore = glm::length(glm::vec2(velocityBefore.x, velocityBefore.y));

        physicsMainUpdate(npc.body, world, input, safeDt);

        float planarSpeedAfter = glm::length(glm::vec2(npc.body.vel.x, npc.body.vel.y));
        npc.lastMoveInput = input.wishMoveXY;
        npc.lastAcceleration = (npc.body.vel - velocityBefore) / safeDt;
        npc.lastGravityDelta = npc.body.vel.z - velocityBefore.z;
        npc.lastFrictionDelta = input.movementPressed ? 0.0f : planarSpeedAfter - planarSpeedBefore;
        npc.lastFinalSpeed = glm::length(npc.body.vel + npc.body.externalImpulse);

        if (DebugConfig::DEBUG_NPC)
        {
            std::string cmdKey = "npc-cmd-" + std::to_string(npc.id);
            Debug::logThrottled(Debug::Category::General, cmdKey.c_str(), DebugConfig::PRINT_INTERVAL,
                "[NPC] id=%u state=%s jump=%d dash=%d move=(%.2f %.2f)\n",
                npc.id, npcStateName(npc.stateMachine.currentState).c_str(),
                (int)input.jumpHeld, (int)input.dashPressed,
                input.wishMoveXY.x, input.wishMoveXY.y);

            std::string physKey = "npc-phys-" + std::to_string(npc.id);
            Debug::logThrottled(Debug::Category::General, physKey.c_str(), DebugConfig::PRINT_INTERVAL,
            "[NPC PHYS] id=%u grounded=%d vel=(%.2f %.2f %.2f) finalSpeed=%.2f\n",
            npc.id, (int)npc.body.ground.onGround,
            npc.body.vel.x, npc.body.vel.y, npc.body.vel.z,
            npc.lastFinalSpeed);
    }

    if (DebugConfig::DEBUG_NPC_COMBAT && npc.sensors.hasTarget)
    {
        float aimErrDeg = NpcCombat::aimErrorDegrees(npc.difficulty);
        printf("[NPC] id=%u state=%s dist=%.1f aimError=%.2f canSee=%d "
               "aimTimer=%.2f reactionTimer=%.2f\n",
               npc.id, npcStateName(npc.stateMachine.currentState).c_str(),
               npc.sensors.targetDistance, aimErrDeg,
               (int)(npc.sensors.targetDistance <= npc.tuning.awarenessRange),
               npc.aimTimer, npc.reactionTimer);
    }
    }

    if (input.dashPressed && npc.body.dash.didDash)
    {
        npc.dashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.62f;
        Debug::log(Debug::Category::General, "[NPC] id=%u dashed\n", npc.id);
        EffectPartSystem::instance().spawnDash(npc.body.pos);
        playWorldSound("entity/player/dash", npc.body.pos, 1.0f, 1.0f, 36.0f);
    }

    if (wantDownDash && downDashAvailableBefore && !npc.body.dash.downDashAvailable)
    {
        npc.downDashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.50f;
        Debug::log(Debug::Category::General, "[NPC] id=%u down-dashed\n", npc.id);
    }

    if (npc.sensors.hasTarget)
    {
        npc.reactionTimer -= safeDt;
    }
    else
    {
        npc.reactionTimer = 0.05f + random01(npc.rngState) * 0.30f;
    }

    if (attack && npc.attackCooldown <= 0.0f && npc.trainingMode == 2 && npc.reactionTimer <= 0.0f)
    {
        Perf::ScopedTimer _combatTimer("NpcCombat");
        bool fired = NpcCombat::tryFire(npc, world, player, safeDt);
        if (fired)
        {
            npc.stateMachine.nextDecisionTime = 0.0f;
        }
    }
}

void NpcSystem::render(const Camera& camera) const
{
    Perf::ScopedTimer _t("NpcRender");
    for (const Npc& npc : npcs)
        renderPlayer(npc.body, camera);
}

void NpcSystem::drawDebug(const Camera& camera) const
{
    DebugVis::drawNpcDebugStuff(debugInfo(), camera);
}

std::vector<DebugVis::NpcDebugInfo> NpcSystem::debugInfo() const
{
    std::vector<DebugVis::NpcDebugInfo> out;
    out.reserve(npcs.size());

    for (const Npc& npc : npcs)
    {
        DebugVis::NpcDebugInfo info;
        info.position = npc.body.pos;
        info.velocity = npc.body.vel + npc.body.externalImpulse;
        info.acceleration = npc.lastAcceleration;
        info.targetPosition = npc.sensors.targetPos;
        info.moveDirection = glm::vec3(npc.lastMoveInput, 0.0f);
        info.pathTarget = npc.stateMachine.wanderTarget;
        info.action = npcStateName(npc.stateMachine.currentState);
        info.difficulty = npc.difficulty;
        info.awarenessRadius = npc.tuning.awarenessRange;
        info.finalSpeed = npc.lastFinalSpeed;
        info.grounded = npc.body.ground.stableOnGround;
        info.hasTarget = npc.sensors.hasTarget;
        out.push_back(info);
    }

    return out;
}
