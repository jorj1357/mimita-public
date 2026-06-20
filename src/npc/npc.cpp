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
#include "perf/perf.h"
#include "npc/npc-state-machine.h"
// Bomb tag behavior is controlled via Npc::bombTag* flags set by BombTagManager

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
    // Reaction speed: diff 1 = 0.8s, diff 10 = 0.12s (faster reactions, not stat boosts)
    tuning.reactionDelay = 0.80f - d * 0.68f;
    // Decision interval: diff 1 = 0.9s, diff 10 = 0.15s (quicker decisions)
    tuning.actionInterval = 0.90f - d * 0.75f;
    // Aggression: diff 1 = 0.15, diff 10 = 0.95 (more aggressive at high diff)
    tuning.aggression = 0.15f + d * 0.80f;
    // Dodge chance: diff 1 = 0%, diff 10 = 50%
    tuning.dodgeChance = d * 0.50f;
    // Aim error: diff 1 = 12 deg, diff 10 = 0.5 deg (improving aim)
    tuning.aimErrorRadians = 0.0f; // not used - uses aimErrorDegrees() instead
    // Movement precision / variety: diff 1 = 0.15, diff 10 = 0.95
    tuning.movementPrecision = 0.15f + d * 0.80f;
    // Awareness: diff 1 = 20m, diff 10 = 150m
    tuning.awarenessRange = 20.0f + d * 130.0f;
    // Prediction: diff 1 = 0.02, diff 10 = 0.65 (better leading)
    tuning.prediction = 0.02f + d * 0.63f;
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

// Compute difficulty-based reaction delay in seconds.
// Difficulty 10 = 0ms, Difficulty 5 = 250-500ms, Difficulty 1 = 1000-1500ms
static float reactionDelayForDifficulty(float difficulty) {
    float d01 = std::clamp(difficulty / 10.0f, 0.0f, 1.0f);
    float baseMs = 1500.0f - d01 * 1500.0f;               // 1500ms at diff1, 0ms at diff10
    float jitterMs = 200.0f + (1.0f - d01) * 300.0f;       // 500ms jitter at diff1, 200ms at diff10
    float totalMs = baseMs + (float)(rand() % (int)(jitterMs + 1.0f));
    return totalMs / 1000.0f;                               // convert to seconds
}

// Get delayed target position from history based on reaction delay.
static glm::vec3 delayedTarget(const Npc& npc, const glm::vec3& currentPos,
                                const glm::vec3& currentVel, float delaySeconds) {
    if (delaySeconds <= 0.001f || npc.posHistory.empty())
        return currentPos;

    // Find the sample closest to (current_time - delay)
    float targetTime = npc.posHistory.back().time - delaySeconds;
    glm::vec3 bestPos = currentPos;
    glm::vec3 bestVel = currentVel;
    float bestTime = -999.0f;

    for (int i = (int)npc.posHistory.size() - 1; i >= 0; --i) {
        if (npc.posHistory[i].time >= targetTime) {
            bestPos = npc.posHistory[i].pos;
            bestVel = npc.posHistory[i].vel;
            bestTime = npc.posHistory[i].time;
        } else {
            // Interpolate between this sample and the next
            if (i + 1 < (int)npc.posHistory.size()) {
                float t = (targetTime - npc.posHistory[i].time)
                        / (npc.posHistory[i + 1].time - npc.posHistory[i].time);
                t = std::clamp(t, 0.0f, 1.0f);
                bestPos = glm::mix(npc.posHistory[i].pos, npc.posHistory[i + 1].pos, t);
                bestVel = glm::mix(npc.posHistory[i].vel, npc.posHistory[i + 1].vel, t);
            }
            break;
        }
    }

    // Add velocity-based prediction to the delayed position
    // This makes high-difficulty NPCs accurate despite the delay
    float predictTime = delaySeconds * (1.0f - std::clamp((npc.difficulty / 10.0f), 0.0f, 1.0f));
    return bestPos + bestVel * predictTime;
}

void senseWorld(Npc& npc, const Player& player, float dt)
{
    NpcSensorContext sensors;
    sensors.selfVel = npc.body.vel + npc.body.externalImpulse;
    sensors.grounded = npc.body.onGround;

    // Record current player state into position history
    npc.posHistory.push_back({player.pos, player.vel + player.externalImpulse, npc.sensors.time + dt});
    while ((int)npc.posHistory.size() > Npc::MAX_HISTORY_SAMPLES)
        npc.posHistory.erase(npc.posHistory.begin());

    // Compute reaction delay based on difficulty
    float delay = reactionDelayForDifficulty(npc.difficulty);

    // Use delayed player position for sensing
    glm::vec3 rawPos = player.pos;
    glm::vec3 rawVel = player.vel + player.externalImpulse;
    sensors.targetPos = delayedTarget(npc, rawPos, rawVel, delay);
    sensors.targetVel = rawVel; // velocity is from current frame (always accurate)
    sensors.toTarget = sensors.targetPos - npc.body.pos;
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
    for (int i = 0; i < 5; ++i) {
        float d = 1.0f + i * 2.0f;
        if (i == 4) d = 10.0f;
        spawnNpc(d);
    }
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

void NpcSystem::spawnNpc(float difficulty)
{
    Perf::ScopedTimer _spawnTimer("NpcSpawn");
    float d = globalDifficulty_ > 0.0f ? globalDifficulty_ : difficulty;
    uint32_t id = nextNpcId();
    npcs.emplace_back(id, d, npcSpawnPoint);
    AudioManager::instance().play({"npc_spawn", AudioCategory::NPC, true, npcSpawnPoint, 0.8f, 1.0f, 35.0f, id});
    Debug::log(Debug::Category::General, "[NPC] spawned id=%u at (%.2f, %.2f, %.2f) (global diff=%.1f)\n",
               id, npcSpawnPoint.x, npcSpawnPoint.y, npcSpawnPoint.z, d);
}

void NpcSystem::spawnNpc(uint32_t id, float difficulty, glm::vec3 spawnPos)
{
    Perf::ScopedTimer _spawnTimer("NpcSpawn");
    float d = globalDifficulty_ > 0.0f ? globalDifficulty_ : difficulty;
    npcs.emplace_back(id, d, spawnPos);
    AudioManager::instance().play({"npc_spawn", AudioCategory::NPC, true, spawnPos, 0.8f, 1.0f, 35.0f, id});
    Debug::log(Debug::Category::General, "[NPC] spawned id=%u at (%.2f, %.2f, %.2f) (network, diff=%.1f)\n",
               id, spawnPos.x, spawnPos.y, spawnPos.z, d);
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

    // Bomb tag override: chase when holding bomb, flee when not
    if (npc.bombTagActive)
    {
        if (npc.bombTagHasBomb)
        {
            // Chase the bomb tag target
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
            // Flee from bomb holder
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

    // Apply wall avoidance
    {
        Perf::ScopedTimer _pathTimer("NpcPathfinding");
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
    }

    // Build input
    InputState input = buildInputState(npc, moveDir, jump, dash, attack, wantDownDash);
    if (input.dashPressed)
        npc.dashCommandConsumed = true;

    // Physics update (uses same player movement system)
    bool downDashAvailableBefore = npc.body.downDashAvailable;
    {
        Perf::ScopedTimer _npcCollision("NpcCollision");
        glm::vec3 velocityBefore = npc.body.vel;
        float planarSpeedBefore = glm::length(glm::vec2(velocityBefore.x, velocityBefore.y));

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
        // TODO(debug): migrate to Debug::log(Debug::Category::NpcMovement)
        printf("[NPC] id=%u state=%s dist=%.1f aimError=%.2f canSee=%d "
               "aimTimer=%.2f reactionTimer=%.2f\n",
               npc.id, npcStateName(npc.stateMachine.currentState).c_str(),
               npc.sensors.targetDistance, aimErrDeg,
               (int)(npc.sensors.targetDistance <= npc.tuning.awarenessRange),
               npc.aimTimer, npc.reactionTimer);
    }
    } // Perf::ScopedTimer NpcCollision

    // Dash cooldown + effects
    if (input.dashPressed && npc.body.didDash)
    {
        npc.dashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.62f;
        Debug::log(Debug::Category::General, "[NPC] id=%u dashed\n", npc.id);
        EffectPartSystem::instance().spawnDash(npc.body.pos);
        playWorldSound("entity/player/dash", npc.body.pos, 1.0f, 1.0f, 36.0f);
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
