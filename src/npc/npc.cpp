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
    tuning.awarenessRange = 8.0f + d * 34.0f;
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

const char* actionName(NpcActionType type)
{
    switch (type)
    {
        case NpcActionType::Wander: return "wander";
        case NpcActionType::Chase: return "chase";
        case NpcActionType::Strafe: return "strafe";
        case NpcActionType::Evade: return "evade";
        case NpcActionType::Jump: return "jump";
        case NpcActionType::Dash: return "dash";
        case NpcActionType::Attack: return "attack";
        default: return "idle";
    }
}

void logActionChange(const Npc& npc, const NpcAction& next)
{
    if (npc.chosenAction.type == next.type)
        return;

    Debug::log(
        Debug::Category::General,
        "[NPC] id=%u difficulty=%.1f action=%s score=%.2f target=%d dist=%.2f\n",
        npc.id,
        npc.difficulty,
        next.name.c_str(),
        next.score,
        (int)npc.sensors.hasTarget,
        npc.sensors.targetDistance
    );
}

void senseWorld(Npc& npc, const Player& player, float dt)
{
    NpcSensorContext sensors;
    sensors.selfVel = npc.body.vel + glm::vec3(npc.body.dashVel.x, npc.body.dashVel.y, 0.0f);
    sensors.grounded = npc.body.onGround;
    sensors.targetPos = player.pos;
    sensors.targetVel = player.vel + glm::vec3(player.dashVel.x, player.dashVel.y, 0.0f);
    sensors.toTarget = player.pos - npc.body.pos;
    sensors.targetDistance = glm::length(sensors.toTarget);
    sensors.hasTarget = npc.difficulty > 0.05f && sensors.targetDistance <= npc.tuning.awarenessRange;
    sensors.visibleTarget = sensors.hasTarget;
    sensors.predictedTarget = sensors.targetPos + sensors.targetVel * (0.10f + npc.tuning.prediction * 0.55f);

    glm::vec3 moved = npc.body.pos - npc.previousPosition;
    bool tryingMove = glm::dot(npc.chosenAction.direction, npc.chosenAction.direction) > 0.01f;
    sensors.likelyBlocked = tryingMove && dt > 0.0f && glm::length(moved) < 0.03f && sensors.grounded;
    npc.stuckTimer = sensors.likelyBlocked ? npc.stuckTimer + dt : 0.0f;
    sensors.likelyBlocked = sensors.likelyBlocked && npc.stuckTimer > 0.20f;

    npc.previousPosition = npc.body.pos;
    npc.sensors = sensors;

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

NpcAction makeAction(NpcActionType type, glm::vec3 direction, glm::vec3 pathTarget, float score)
{
    NpcAction action;
    action.type = type;
    action.name = actionName(type);
    action.direction = safePlanarNormal(direction, {1.0f, 0.0f, 0.0f});
    action.pathTarget = pathTarget;
    action.score = score;
    return action;
}

NpcAction chooseAction(Npc& npc, float dt)
{
    const NpcSensorContext& s = npc.sensors;
    float d = difficulty01(npc.difficulty);

    if (npc.difficulty <= 0.05f)
        return makeAction(NpcActionType::Idle, {0.0f, 0.0f, 0.0f}, npc.body.pos, 1.0f);

    npc.wanderTimer -= dt;
    if (npc.wanderTimer <= 0.0f)
    {
        npc.wanderTimer = 1.0f + random01(npc.rngState) * (3.5f - d * 2.2f);
        npc.wanderDirection = randomPlanarDirection(npc.rngState);
    }

    std::vector<NpcAction> actions;
    actions.push_back(makeAction(
        NpcActionType::Wander,
        npc.wanderDirection,
        npc.body.pos + npc.wanderDirection * 6.0f,
        0.18f + (1.0f - d) * 0.35f
    ));

    if (!s.hasTarget)
        return *std::max_element(actions.begin(), actions.end(), [](const NpcAction& a, const NpcAction& b) { return a.score < b.score; });

    glm::vec3 toPredicted = s.predictedTarget - npc.body.pos;
    glm::vec3 chaseDir = safePlanarNormal(toPredicted, npc.wanderDirection);
    glm::vec3 lateral{-chaseDir.y, chaseDir.x, 0.0f};
    if ((npc.id % 2u) == 0u)
        lateral = -lateral;

    float close01 = 1.0f - clamp01((s.targetDistance - 2.0f) / 16.0f);
    float far01 = clamp01((s.targetDistance - 4.0f) / 24.0f);
    float attackRange01 = 1.0f - clamp01((s.targetDistance - 2.4f) / 2.8f);
    float speedThreat = clamp01(glm::length(s.targetVel) / 45.0f);

    actions.push_back(makeAction(
        NpcActionType::Chase,
        chaseDir,
        s.predictedTarget,
        0.35f + far01 * (0.55f + npc.tuning.aggression * 0.35f)
    ));

    actions.push_back(makeAction(
        NpcActionType::Strafe,
        chaseDir * 0.45f + lateral * 0.90f,
        s.targetPos + lateral * 5.0f,
        0.20f + close01 * (0.40f + d * 0.30f)
    ));

    actions.push_back(makeAction(
        NpcActionType::Evade,
        -chaseDir + lateral * 0.65f,
        npc.body.pos - chaseDir * 5.0f + lateral * 3.0f,
        close01 * (0.25f + npc.tuning.dodgeChance * 0.55f) + speedThreat * npc.tuning.dodgeChance
    ));

    if (s.likelyBlocked || (s.grounded && random01(npc.rngState) < (0.16f + d * 0.20f) * dt))
    {
        NpcAction jump = makeAction(NpcActionType::Jump, chaseDir, s.predictedTarget, s.likelyBlocked ? 0.95f : 0.38f + d * 0.20f);
        jump.jumpHeld = true;
        actions.push_back(jump);
    }

    if (npc.dashCooldown <= 0.0f && s.targetDistance > 3.5f)
    {
        NpcAction dash = makeAction(NpcActionType::Dash, chaseDir, s.predictedTarget, far01 * (0.25f + npc.tuning.aggression * 0.75f));
        dash.dashPressed = true;
        actions.push_back(dash);
    }

    if (npc.attackCooldown <= 0.0f && attackRange01 > 0.0f)
    {
        NpcAction attack = makeAction(NpcActionType::Attack, chaseDir + lateral * (1.0f - npc.tuning.movementPrecision) * 0.35f, s.targetPos, attackRange01 * (0.55f + npc.tuning.aggression * 0.70f));
        attack.attackPressed = true;
        if (npc.dashCooldown <= 0.0f && npc.tuning.aggression > 0.55f)
            attack.dashPressed = true;
        actions.push_back(attack);
    }

    return *std::max_element(actions.begin(), actions.end(), [](const NpcAction& a, const NpcAction& b) { return a.score < b.score; });
}

InputState inputFromAction(Npc& npc, NpcAction action)
{
    InputState input;

    if (npc.tuning.aimErrorRadians > 0.001f)
    {
        float error = (random01(npc.rngState) * 2.0f - 1.0f) * npc.tuning.aimErrorRadians;
        action.direction = rotatePlanar(action.direction, error);
    }

    float moveScale = std::clamp(0.35f + npc.tuning.movementPrecision * 0.65f, 0.0f, 1.0f);
    input.wishMoveXY = glm::vec2(action.direction.x, action.direction.y) * moveScale;
    input.jumpHeld = action.jumpHeld;
    input.dashPressed = action.dashPressed;
    input.groundReturnPressed = false;
    input.freezeHeld = false;
    input.camForward = safePlanarNormal(action.direction, {1.0f, 0.0f, 0.0f});
    return input;
}

}

Npc::Npc(std::uint32_t npcId, float npcDifficulty, glm::vec3 spawn)
    : id(npcId), difficulty(std::clamp(npcDifficulty, 0.0f, 10.0f))
{
    tuning = tuningForDifficulty(difficulty);
    rngState = 0x9e3779b9u ^ (id * 747796405u);
    body.reset();
    body.username = "npc-" + std::to_string(id);
    body.currentHp = body.maxHp;
    body.pos = spawn;
    body.vel = {0.0f, 0.0f, 0.0f};
    body.dashVel = {0.0f, 0.0f};
    body.syncLegacyStateToLayers();
    previousPosition = body.pos;
    wanderDirection = randomPlanarDirection(rngState);

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
    npcs.clear();
}

void NpcSystem::spawnNpc(float difficulty, glm::vec3 spawnPos)
{
    uint32_t id = nextNpcId();
    npcs.emplace_back(id, difficulty, spawnPos);
}

void NpcSystem::update(const World& world, const Player& player, float dt)
{
    for (Npc& npc : npcs)
    {
        if (npc.body.currentHp <= 0) {
            npc.body.vel = glm::vec3(0.0f);
            npc.body.dashVel = glm::vec2(0.0f);
            continue;
        }
        npc.reactionTimer -= dt;
        npc.actionTimer -= dt;
        npc.dashCooldown = std::max(0.0f, npc.dashCooldown - dt);
        npc.attackCooldown = std::max(0.0f, npc.attackCooldown - dt);

        senseWorld(npc, player, dt);

        if (npc.reactionTimer <= 0.0f && npc.actionTimer <= 0.0f)
        {
            NpcAction next = chooseAction(npc, dt);
            if (npc.sensors.likelyBlocked) {
                next.direction = randomPlanarDirection(npc.rngState);
                next.jumpHeld = true;
                next.dashPressed = npc.dashCooldown <= 0.0f;
                next.name = "unstuck";
                next.score = 2.0f;
            }
            logActionChange(npc, next);
            npc.chosenAction = next;
            npc.reactionTimer = npc.tuning.reactionDelay;
            npc.actionTimer = npc.tuning.actionInterval;
        }

        InputState input = inputFromAction(npc, npc.chosenAction);
        if (DebugConfig::DEBUG_NPC)
            Debug::log(Debug::Category::General, "[NPC COMMAND] id=%u action=%s jump=%d dash=%d move=(%.2f %.2f)\n",
                       npc.id, npc.chosenAction.name.c_str(), (int)input.jumpHeld,
                       (int)input.dashPressed, input.wishMoveXY.x, input.wishMoveXY.y);
        physicsMainUpdate(npc.body, world, input, dt);

        if (npc.chosenAction.dashPressed && npc.body.didDash)
        {
            npc.dashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.62f;
            Debug::log(Debug::Category::General,
                       "[NPC] id=%u dash chosen difficulty=%.1f\n",
                       npc.id, npc.difficulty);
        }

        if (npc.chosenAction.attackPressed && npc.attackCooldown <= 0.0f)
        {
            weaponHit(npc.body);
            npc.attackCooldown = 1.35f - difficulty01(npc.difficulty) * 1.05f;
            Debug::log(Debug::Category::General,
                       "[NPC] id=%u attack chosen difficulty=%.1f distance=%.2f\n",
                       npc.id, npc.difficulty, npc.sensors.targetDistance);
        }
    }

    // Resolve NPC vs Player collisions after all physics updates
    // Note: player is const here, but we need to modify it for collision resolution
    // This will be handled in main.cpp where we have non-const access to player
}

void NpcSystem::render(const Camera& camera) const
{
    for (const Npc& npc : npcs)
        if (npc.body.currentHp > 0)
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
        info.velocity = npc.body.vel + glm::vec3(npc.body.dashVel.x, npc.body.dashVel.y, 0.0f);
        info.targetPosition = npc.sensors.targetPos;
        info.moveDirection = npc.chosenAction.direction;
        info.pathTarget = npc.chosenAction.pathTarget;
        info.action = npc.chosenAction.name;
        info.difficulty = npc.difficulty;
        info.awarenessRadius = npc.tuning.awarenessRange;
        info.hasTarget = npc.sensors.hasTarget;
        out.push_back(info);
    }

    return out;
}
