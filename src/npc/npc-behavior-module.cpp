// 2026-11-08
/* purpose
* hot-reloadable utility goal/action policy for server NPCs
* host retains NPC state, movement, collision, physics, damage, and authority
* bounded decision timing is emitted through the canonical host logger
*/
#include "modules/api/npc-behavior-api.h"
#include "npc/npc-utility.h"

#include <chrono>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <module-identity.h>

namespace {

const mimita_module::HostServicesV1* gServices = nullptr;
std::atomic<std::uint32_t> gLastLoggedTick{0};

bool has(const mimita_npc_behavior::DecisionV1& request,
         mimita_npc_behavior::ContextFlag flag)
{
    return (request.contextFlags & static_cast<std::uint32_t>(flag)) != 0;
}

UtilityContext makeContext(const mimita_npc_behavior::DecisionV1& r)
{
    UtilityContext ctx;
    ctx.hasVisibleTarget = has(r, mimita_npc_behavior::HAS_VISIBLE_TARGET);
    ctx.hasKnownTarget = has(r, mimita_npc_behavior::HAS_KNOWN_TARGET);
    ctx.targetDistance = r.targetDistance;
    ctx.targetConfidence = r.targetConfidence;
    ctx.healthFraction = r.healthFraction;
    ctx.timeRemaining = r.timeRemaining;
    ctx.enemyCount = r.enemyCount;
    ctx.teamAlive = r.teamAlive;
    ctx.weaponReady = has(r, mimita_npc_behavior::WEAPON_READY);
    ctx.loSBlocked = has(r, mimita_npc_behavior::LOS_BLOCKED);
    ctx.objectiveKnown = has(r, mimita_npc_behavior::OBJECTIVE_KNOWN);
    ctx.payloadObjective = has(r, mimita_npc_behavior::PAYLOAD_OBJECTIVE);
    ctx.targetCartDistance = r.targetCartDistance;
    ctx.atObjective = has(r, mimita_npc_behavior::AT_OBJECTIVE);
    ctx.canPlant = has(r, mimita_npc_behavior::CAN_PLANT);
    ctx.canDefuse = has(r, mimita_npc_behavior::CAN_DEFUSE);
    ctx.onDefense = has(r, mimita_npc_behavior::ON_DEFENSE);
    ctx.team = r.team;
    ctx.planted = has(r, mimita_npc_behavior::PLANTED);
    ctx.bombDefendRadius = r.bombDefendRadius;
    ctx.enemyAreaKnown = has(r, mimita_npc_behavior::ENEMY_AREA_KNOWN);
    ctx.enemyAreaPos = glm::vec3(r.enemyAreaPosition[0], r.enemyAreaPosition[1],
                                 r.enemyAreaPosition[2]);
    ctx.objectivePos = glm::vec3(r.objectivePosition[0], r.objectivePosition[1],
                                 r.objectivePosition[2]);
    ctx.travelSlot = glm::vec3(r.travelSlot[0], r.travelSlot[1], r.travelSlot[2]);
    ctx.hazardPos = glm::vec3(r.hazardPosition[0], r.hazardPosition[1],
                              r.hazardPosition[2]);
    ctx.hazardRadius = r.hazardRadius;
    ctx.hazardKind = r.hazardKind;
    ctx.lookAwayFrom = glm::vec3(r.lookAwayFrom[0], r.lookAwayFrom[1],
                                 r.lookAwayFrom[2]);
    ctx.focusPos = glm::vec3(r.focusPosition[0], r.focusPosition[1],
                             r.focusPosition[2]);
    ctx.squadSlot = glm::vec3(r.squadSlot[0], r.squadSlot[1], r.squadSlot[2]);
    ctx.squadAnchor = glm::vec3(r.squadAnchor[0], r.squadAnchor[1], r.squadAnchor[2]);
    ctx.squadApproachDir = glm::vec3(r.squadApproachDirection[0],
                                     r.squadApproachDirection[1],
                                     r.squadApproachDirection[2]);
    ctx.squadApproachSector = r.squadApproachSector;
    ctx.plantedBombPos = glm::vec3(r.plantedBombPosition[0], r.plantedBombPosition[1],
                                   r.plantedBombPosition[2]);
    ctx.travelObjectiveBias = r.travelObjectiveBias;
    ctx.travelHuntBias = r.travelHuntBias;
    ctx.focusKnown = has(r, mimita_npc_behavior::FOCUS_KNOWN);
    ctx.squadSlotKnown = has(r, mimita_npc_behavior::SQUAD_SLOT_KNOWN);
    ctx.atSquadSlot = has(r, mimita_npc_behavior::AT_SQUAD_SLOT);
    ctx.bombDropped = has(r, mimita_npc_behavior::BOMB_DROPPED);
    ctx.isRecoverer = has(r, mimita_npc_behavior::IS_RECOVERER);
    ctx.isCarrier = has(r, mimita_npc_behavior::IS_CARRIER);
    ctx.isDefuserId = has(r, mimita_npc_behavior::IS_DEFUSER);
    ctx.travelSlotKnown = has(r, mimita_npc_behavior::IS_TRAVEL_SLOT_KNOWN);
    ctx.hazardActive = has(r, mimita_npc_behavior::HAZARD_ACTIVE);
    ctx.lookAwayKnown = has(r, mimita_npc_behavior::HAZARD_LOOK_AWAY_KNOWN);
    ctx.squadApproachKnown = has(r, mimita_npc_behavior::SQUAD_APPROACH_KNOWN);
    ctx.swarm = has(r, mimita_npc_behavior::SWARM);
    return ctx;
}

UtilityGoalScore makeScore(const mimita_npc_behavior::ScoreV1& score)
{
    UtilityGoalScore out;
    out.kind = static_cast<UtilityGoalKind>(score.kind);
    out.relevance = score.relevance;
    out.distance = score.distance;
    out.lineOfSight = score.lineOfSight;
    out.health = score.health;
    out.enemyCount = score.enemyCount;
    out.timeRemaining = score.timeRemaining;
    out.objectiveState = score.objectiveState;
    out.weaponReadiness = score.weaponReadiness;
    out.confidence = score.confidence;
    out.teamInformation = score.teamInformation;
    out.total = score.total;
    return out;
}

void storeScore(const UtilityGoalScore& score,
                mimita_npc_behavior::ScoreV1& out)
{
    out.kind = static_cast<std::uint32_t>(score.kind);
    out.relevance = score.relevance;
    out.distance = score.distance;
    out.lineOfSight = score.lineOfSight;
    out.health = score.health;
    out.enemyCount = score.enemyCount;
    out.timeRemaining = score.timeRemaining;
    out.objectiveState = score.objectiveState;
    out.weaponReadiness = score.weaponReadiness;
    out.confidence = score.confidence;
    out.teamInformation = score.teamInformation;
    out.total = score.total;
}

int decide(mimita_npc_behavior::DecisionV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    const auto started = std::chrono::steady_clock::now();
    UtilityContext context = makeContext(request);
    // Hidden enemies can be searched near the cart; stale reports beyond the
    // leash cannot pull a Payload NPC away from its assigned role.
    if (context.payloadObjective && !context.hasVisibleTarget &&
        context.targetCartDistance > 25.0f)
        context.hasKnownTarget = false;
    UtilityState state;
    state.currentGoal = static_cast<UtilityGoalKind>(request.currentGoal);
    state.currentAction = static_cast<UtilityActionKind>(request.currentAction);
    state.goalTimer = request.goalTimer;
    state.actionCooldown = request.actionCooldown;
    state.currentScore = makeScore(request.currentScore);

    UtilityGoalKind goal = selectUtilityGoal(
        context, state, request.dt, request.minGoalSeconds, request.switchMargin);
    if (context.payloadObjective &&
        ((context.hasVisibleTarget && !context.loSBlocked) ||
         (context.hasKnownTarget && context.targetCartDistance <= 25.0f))) {
        goal = UtilityGoalKind::KillTarget;
        if (state.currentGoal != goal) state.goalTimer = 0.0f;
        state.currentGoal = goal;
        state.currentScore = scoreUtilityGoal(goal, context);
        state.currentAction = actionForGoal(goal, context);
    } else if (context.payloadObjective && context.objectiveKnown &&
               (!context.hasKnownTarget || context.targetCartDistance > 25.0f)) {
        goal = context.onDefense ? UtilityGoalKind::DefendSite :
                                   UtilityGoalKind::MoveToObjective;
        if (state.currentGoal != goal) state.goalTimer = 0.0f;
        state.currentGoal = goal;
        state.currentScore = scoreUtilityGoal(goal, context);
        state.currentAction = actionForGoal(goal, context);
    }
    const auto finished = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        finished - started).count();
    request.outGoal = static_cast<std::uint32_t>(goal);
    request.outAction = static_cast<std::uint32_t>(state.currentAction);
    request.outGoalTimer = state.goalTimer;
    request.outActionCooldown = state.actionCooldown;
    storeScore(state.currentScore, request.outCurrentScore);
    request.outGeneration = MIMITA_MODULE_GENERATION;
    request.outDecisionMilliseconds = static_cast<float>(elapsedMs);
    // Stateless per-actor cadence: identical inputs at the same actor/tick
    // produce the same movement choice across module generations.
    std::uint32_t roll = request.actorId * 0x9e3779b9u ^ request.tick * 0x85ebca6bu;
    roll ^= roll >> 16;
    roll *= 0x7feb352du;
    roll ^= roll >> 15;
    const float unitRoll = static_cast<float>(roll & 0x00ffffffu) / 16777216.0f;
    request.outDownDash = has(request, mimita_npc_behavior::DOWN_DASH_OPPORTUNITY) &&
        unitRoll < std::clamp(request.downDashChance, 0.0f, 1.0f) ? 1u : 0u;
    request.outHoldPursuit = has(request, mimita_npc_behavior::HAS_KNOWN_TARGET) ? 1u : 0u;
    roll ^= 0xa511e9b3u;
    roll ^= roll >> 16;
    roll *= 0x7feb352du;
    roll ^= roll >> 15;
    const float dashRoll = static_cast<float>(roll & 0x00ffffffu) / 16777216.0f;
    request.outDash = has(request, mimita_npc_behavior::DASH_OPPORTUNITY) &&
        dashRoll < std::clamp(request.dashChance, 0.0f, 1.0f) ? 1u : 0u;

    // One record per actor per second, with the actual generation that made
    // the decision. The host owns writing the process's canonical events.jsonl.
    std::uint32_t lastLogged = gLastLoggedTick.load(std::memory_order_relaxed);
    const bool sampleThisDecision = request.actorId != 0 &&
        (request.tick - lastLogged >= 60u) &&
        gLastLoggedTick.compare_exchange_strong(lastLogged, request.tick,
            std::memory_order_relaxed, std::memory_order_relaxed);
    if (gServices && gServices->log && sampleThisDecision) {
        char fields[320]{};
        std::snprintf(fields, sizeof(fields),
            "{\"actor\":%u,\"decision_ms\":%.4f,\"goal\":%u,\"action\":%u,\"generation\":%u}",
            request.actorId, request.outDecisionMilliseconds,
            request.outGoal, request.outAction, request.outGeneration);
        char correlation[24]{};
        std::snprintf(correlation, sizeof(correlation), "%u", request.actorId);
        gServices->log(mimita_module::LOG_CATEGORY_NPC_MOVEMENT,
                       mimita_module::LOG_LEVEL_IMPORTANT,
                       "npc.behavior.decision-timing", correlation,
                       "sampled_60_tick_decision", request.tick, fields,
                       __FILE__, __LINE__, __FUNCTION__);
    }
    return 1;
}

int assignPayloadRole(mimita_npc_behavior::PayloadRoleV1& role)
{
    if (role.version != mimita_npc_behavior::ABI_VERSION ||
        role.structSize != sizeof(role) || role.teamSlot < 0 ||
        role.maxPushAttackers < 0 || role.maxPushAttackers > 64 ||
        role.humanPushers < 0 || role.humanPushers > 64)
        return 0;
    const bool attacking = role.team == role.attackerTeam;
    if (!attacking && role.team != role.defenderTeam) return 0;
    const int availablePushers = std::max(0, role.maxPushAttackers - role.humanPushers);
    const bool push = attacking && role.teamSlot < availablePushers;
    const bool contest = !attacking && role.teamSlot == 0;
    role.outRole = push ? 1u : attacking ? 2u : contest ? 3u : 4u;
    const int index = push ? role.teamSlot :
        attacking ? role.teamSlot - availablePushers : role.teamSlot;
    const float angle = 6.283185307f * static_cast<float>(index % 12) / 12.0f;
    const float halfX = std::max(0.1f, role.interactionSize[0] * 0.5f);
    const float halfY = std::max(0.1f, role.interactionSize[1] * 0.5f);
    const float insideRadius = std::min(1.2f, std::min(halfX, halfY) * 0.6f);
    const float radius = push ? insideRadius : contest ? insideRadius * 0.3f :
        std::max(halfX, halfY) + 4.0f + 2.0f * (index / 12);
    const float x = std::cos(angle) * radius;
    const float y = std::sin(angle) * radius;
    const float* m = role.interactionTransform;
    for (int axis = 0; axis < 3; ++axis)
        role.outPosition[axis] = m[12 + axis] + m[axis] * x + m[4 + axis] * y;
    role.outPosition[2] = role.cartPosition[2];
    role.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int MIMITA_MODULE_CALL initialize(const mimita_module::HostServicesV1* services)
{
    if (!services || services->version != mimita_module::ABI_VERSION ||
        services->structSize != sizeof(*services) || !services->log)
        return 0;
    gServices = services;
    return 1;
}

void MIMITA_MODULE_CALL shutdown() { gServices = nullptr; }

int MIMITA_MODULE_CALL onEvent(mimita_module::ModuleEventV1* event)
{
    if (!event || !event->payload) return 0;
    if (event->typeId == mimita_module::EVENT_NPC_PAYLOAD_ROLE &&
        event->payloadSize == sizeof(mimita_npc_behavior::PayloadRoleV1))
        return assignPayloadRole(*static_cast<mimita_npc_behavior::PayloadRoleV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_BEHAVIOR_DECISION &&
        event->payloadSize == sizeof(mimita_npc_behavior::DecisionV1))
        return decide(*static_cast<mimita_npc_behavior::DecisionV1*>(event->payload));
    return 0;
}

std::uint32_t MIMITA_MODULE_CALL exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int MIMITA_MODULE_CALL importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_module::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION, sizeof(mimita_module::ModuleV1),
     "npc-behavior", MIMITA_MODULE_GENERATION, MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize, shutdown, onEvent, exportState, importState
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    return abiVersion == mimita_module::ABI_VERSION ? &gModule.base : nullptr;
}
