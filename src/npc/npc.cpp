#include "npc.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "combat/weapon-hit.h"
#include "debug/debug-log.h"
#include "physics/config.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "render/render-player.h"
#include "world/world.h"
#include "config.h"
#include "audio/audio.h"
#include "effects/effect-part.h"
#include "devtools/dev-npc-selection.h"
#include "npc/npc-navigation.h"
#include "npc/npc-combat.h"
#include "npc/npc-state-machine.h"

namespace {

float clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

float difficulty01(float difficulty)
{
    return clamp01(difficulty / 10.0f);
}

NpcDifficultyTuning tuningForDifficulty(float difficulty)
{
    float d = difficulty01(difficulty);
    NpcDifficultyTuning tuning;
    tuning.reactionDelay = 1.20f - d * 1.16f;
    tuning.actionInterval = 1.05f - d * 0.95f;
    tuning.aggression = 0.05f + d * 0.95f;
    tuning.dodgeChance = d * d;
    tuning.aimErrorRadians = 0.80f * (1.0f - d);
    tuning.movementPrecision = 0.12f + d * 0.88f;
    tuning.awarenessRange = 15.0f + d * 135.0f;
    tuning.prediction = d * d;
    return tuning;
}

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

void senseWorld(Npc& npc, const Player& player, float dt)
{
    NpcSensorContext sensors;
    sensors.selfVel = npc.body.vel + npc.body.externalImpulse;
    sensors.grounded = npc.body.onGround;
    sensors.targetPos = player.pos;
    sensors.targetVel = player.vel + player.externalImpulse;
    sensors.toTarget = player.pos - npc.body.pos;
    sensors.targetDistance = glm::length(sensors.toTarget);
    sensors.hasTarget = npc.difficulty > 0.05f && sensors.targetDistance <= npc.tuning.awarenessRange;
    sensors.predictedTarget = sensors.targetPos + sensors.targetVel * (0.10f + npc.tuning.prediction * 0.55f);

    npc.previousPosition = npc.body.pos;
    npc.sensors = sensors;

    // Track last known target
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
                   "[NPC] id=%u target acquired difficulty=%.1f distance=%.2f\n",
                   npc.id, npc.difficulty, sensors.targetDistance);
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
    input.dashPressed = dash;
    input.groundReturnPressed = false;
    input.downDashPressed = downDash;
    input.freezeHeld = false;

    // Face movement direction or target
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

Npc::Npc(std::uint32_t npcId, float npcDifficulty, glm::vec3 spawn)
    : id(npcId), difficulty(std::clamp(npcDifficulty, 0.0f, 10.0f))
{
    tuning = tuningForDifficulty(difficulty);
    rngState = 0x9e3779b9u ^ (id * 747796405u);
    body.reset();
    body.username = "npc-" + std::to_string(id);
    body.currentHp = body.maxHp;
    body.pos = spawn;
    body.respawnPosition = spawn;
    body.vel = {0.0f, 0.0f, 0.0f};
    body.syncLegacyStateToLayers();
    previousPosition = body.pos;

    stateMachine.nextDecisionTime = 0.5f + random01(rngState) * 1.5f;
    stateMachine.wanderTarget = spawn + randomPlanarDirection(rngState) * 5.0f;
    stateMachine.wanderTimer = 2.0f + random01(rngState) * 3.0f;
    stateMachine.orbitAngle = random01(rngState) * glm::two_pi<float>();
    stateMachine.orbitSwapTimer = 0.5f + random01(rngState) * 2.0f;

    aimTimer = 0.0f;
    reactionTimer = 0.05f + random01(rngState) * 0.30f;
    moveNoiseTimer = 0.1f + random01(rngState) * 0.3f;
    moveOffset = {0.0f, 0.0f};

    Debug::log(Debug::Category::General,
               "[NPC] spawned id=%u difficulty=%.1f reaction=%.2f aggression=%.2f awareness=%.1f\n",
               id,
               difficulty,
               tuning.reactionDelay,
               tuning.aggression,
               tuning.awarenessRange);
}

void NpcSystem::spawnPrototypeScene()
{
    clear();
    npcs.emplace_back(1, 1.0f, glm::vec3(-6.0f, 6.0f, 32.0f));
    npcs.emplace_back(2, 3.0f, glm::vec3(8.0f, 5.0f, 32.0f));
    npcs.emplace_back(3, 5.0f, glm::vec3(-4.0f, -7.0f, 32.0f));
    npcs.emplace_back(4, 7.0f, glm::vec3(7.0f, -6.0f, 32.0f));
    npcs.emplace_back(5, 10.0f, glm::vec3(0.0f, 12.0f, 32.0f));
}

void NpcSystem::clear()
{
    for (const Npc& npc : npcs) {
        AudioManager::instance().stopOwner(npc.id);
        EffectPartSystem::instance().destroyOwner(npc.id);
        Debug::log(Debug::Category::General, "[NPC] destroyed id=%u\n", npc.id);
    }
    npcs.clear();
    NpcSelectionManager::instance().clear();
    Debug::log(Debug::Category::General, "[NPC] cleanup complete\n");
}

void NpcSystem::spawnNpc(float difficulty, glm::vec3 spawnPos)
{
    float d = globalDifficulty_ > 0.0f ? globalDifficulty_ : difficulty;
    uint32_t id = nextNpcId();
    npcs.emplace_back(id, d, spawnPos);
    AudioManager::instance().play({"npc_spawn", AudioCategory::NPC, true, spawnPos, 0.8f, 1.0f, 35.0f, id});
    Debug::log(Debug::Category::General, "[NPC] spawned id=%u (global diff=%.1f)\n", id, d);
}

void NpcSystem::spawnNpc(uint32_t id, float difficulty, glm::vec3 spawnPos)
{
    float d = globalDifficulty_ > 0.0f ? globalDifficulty_ : difficulty;
    npcs.emplace_back(id, d, spawnPos);
    AudioManager::instance().play({"npc_spawn", AudioCategory::NPC, true, spawnPos, 0.8f, 1.0f, 35.0f, id});
    Debug::log(Debug::Category::General, "[NPC] spawned id=%u (network, diff=%.1f)\n", id, d);
}

void NpcSystem::destroySelected(const std::vector<std::uint32_t>& ids)
{
    npcs.erase(std::remove_if(npcs.begin(), npcs.end(), [&](const Npc& npc) {
        if (std::find(ids.begin(), ids.end(), npc.id) == ids.end()) return false;
        AudioManager::instance().stopOwner(npc.id);
        EffectPartSystem::instance().destroyOwner(npc.id);
        NpcSelectionManager::instance().deselect(npc.id);
        Debug::log(Debug::Category::General, "[NPC] destroyed id=%u\n", npc.id);
        return true;
    }), npcs.end());
    Debug::log(Debug::Category::General, "[NPC] cleanup complete\n");
}

void NpcSystem::destroyAll() { clear(); }

void NpcSystem::setGlobalDifficulty(float d)
{
    globalDifficulty_ = std::clamp(d, 1.0f, 10.0f);
    // Update tuning for all existing NPCs
    for (Npc& npc : npcs)
    {
        npc.difficulty = globalDifficulty_;
        npc.tuning = tuningForDifficulty(globalDifficulty_);
    }
    Debug::log(Debug::Category::General, "[NPC] global difficulty set to %.1f for %zu NPCs\n",
               globalDifficulty_, npcs.size());
}

void NpcSystem::update(const World& world, Player& player, float dt)
{
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

    // Update timers
    npc.dashCooldown = std::max(0.0f, npc.dashCooldown - safeDt);
    npc.downDashCooldown = std::max(0.0f, npc.downDashCooldown - safeDt);
    npc.attackCooldown = std::max(0.0f, npc.attackCooldown - safeDt);
    npc.hitReactionTimer = std::max(0.0f, npc.hitReactionTimer - safeDt);
    npc.aimTimer = std::max(0.0f, npc.aimTimer);
    npc.stateMachine.stateTimer += safeDt;
    npc.stateMachine.nextDecisionTime -= safeDt;
    npc.stateMachine.retreatTimer += safeDt;

    // Sense the world
    senseWorld(npc, player, safeDt);

    // Down dash decision: slam down when airborne above the target
    bool wantDownDash = false;
    if (npc.sensors.hasTarget && !npc.sensors.grounded && npc.downDashCooldown <= 0.0f)
    {
        float heightAbove = npc.body.pos.z - npc.sensors.targetPos.z;
        if (heightAbove > 3.0f)
            wantDownDash = true;
    }

    // Handle hit reaction
    if (npc.hitReactionTimer > 0.0f)
    {
        npc.stateMachine.currentState = NpcState::Recover;
        npc.stateMachine.recoverTimer = npc.hitReactionTimer;
        npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime, npc.hitReactionTimer + 0.1f);
    }

    // Training mode overrides
    if (npc.trainingMode != 2) {
        if (npc.trainingMode == 0) {
            // Idle: stand still, never attack
            npc.stateMachine.currentState = NpcState::Idle;
            npc.stateMachine.nextDecisionTime = 2.0f;
        } else if (npc.trainingMode == 1) {
            // Flee: always retreat away from player, never attack
            npc.stateMachine.currentState = NpcState::Retreat;
            npc.stateMachine.retreatTimer = 0.0f;
            npc.stateMachine.nextDecisionTime = 0.3f;
        }
    } else {
        // trainingMode == 2: normal AI decision
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

    // Compute movement from current state
    glm::vec3 moveDir;
    bool jump, dash, attack;
    computeStateMovement(npc, moveDir, jump, dash, attack);

    // Apply wall avoidance
    if (glm::length(moveDir) > 0.001f)
        moveDir = NpcNavigation::wallAvoidDirection(npc, moveDir, world);

    // Stuck detection override
    if (NpcNavigation::isStuck(npc))
    {
        npc.stateMachine.stuckTimer += safeDt;
        if (npc.stateMachine.stuckTimer > 0.3f)
        {
            // Force unstuck: pick a free direction and jump
            moveDir = NpcNavigation::unstuckDirection(npc, npc.rngState, world);
            jump = true;
            dash = npc.dashCooldown <= 0.0f;
            // Force re-evaluation soon
            npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime, 0.3f);
        }
    }
    else
    {
        npc.stateMachine.stuckTimer = 0.0f;
    }

    // Build input
    InputState input = buildInputState(npc, moveDir, jump, dash, attack, wantDownDash);
    if (input.dashPressed)
        npc.dashCommandConsumed = true;

    // Physics update (uses same player movement system)
    glm::vec3 velocityBefore = npc.body.vel;
    float planarSpeedBefore = glm::length(glm::vec2(velocityBefore.x, velocityBefore.y));
    bool downDashAvailableBefore = npc.body.downDashAvailable;

    physicsMainUpdate(npc.body, world, input, safeDt);

    // Track physics stats
    float planarSpeedAfter = glm::length(glm::vec2(npc.body.vel.x, npc.body.vel.y));
    npc.lastMoveInput = input.wishMoveXY;
    npc.lastAcceleration = (npc.body.vel - velocityBefore) / safeDt;
    npc.lastGravityDelta = npc.body.vel.z - velocityBefore.z;
    npc.lastFrictionDelta = input.movementPressed ? 0.0f : planarSpeedAfter - planarSpeedBefore;
    npc.lastFinalSpeed = glm::length(npc.body.vel + npc.body.externalImpulse);

    // Debug logging
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
            npc.id, (int)npc.body.onGround,
            npc.body.vel.x, npc.body.vel.y, npc.body.vel.z,
            npc.lastFinalSpeed);
    }

    // NPC Combat debug (state, distance, aim error)
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

    // Dash cooldown
    if (input.dashPressed && npc.body.didDash)
    {
        npc.dashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.62f;
        Debug::log(Debug::Category::General, "[NPC] id=%u dashed\n", npc.id);
    }

    // Down dash cooldown: detect if down dash was consumed this frame
    if (wantDownDash && downDashAvailableBefore && !npc.body.downDashAvailable)
    {
        npc.downDashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.50f;
        Debug::log(Debug::Category::General, "[NPC] id=%u down-dashed\n", npc.id);
    }

    // Reaction delay: random delay before first shot after acquiring target
    if (npc.sensors.hasTarget)
    {
        npc.reactionTimer -= safeDt;
    }
    else
    {
        npc.reactionTimer = 0.05f + random01(npc.rngState) * 0.30f;
    }

    // Attack (disabled for training modes 0 idle and 1 flee)
    if (attack && npc.attackCooldown <= 0.0f && npc.trainingMode == 2 && npc.reactionTimer <= 0.0f)
    {
        bool fired = NpcCombat::tryFire(npc, world, player, safeDt);
        if (fired)
        {
            npc.stateMachine.nextDecisionTime = 0.0f;
        }
    }
}

void NpcSystem::render(const Camera& camera) const
{
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
        info.grounded = npc.body.stableOnGround;
        info.hasTarget = npc.sensors.hasTarget;
        out.push_back(info);
    }

    return out;
}
