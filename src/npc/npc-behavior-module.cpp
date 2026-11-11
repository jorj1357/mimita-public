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
#include <string>

#include <module-identity.h>

namespace {

const mimita_module::HostServicesV1* gServices = nullptr;
std::atomic<std::uint32_t> gLastLoggedTick{0};

bool has(const mimita_npc_behavior::DecisionV1& request,
         mimita_npc_behavior::ContextFlag flag)
{
    return (request.contextFlags & static_cast<std::uint32_t>(flag)) != 0;
}

bool has(const mimita_npc_behavior::ActionV1& request,
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

int actionForGoalEvent(mimita_npc_behavior::ActionV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    UtilityContext ctx;
    ctx.weaponReady = has(request, mimita_npc_behavior::WEAPON_READY);
    ctx.loSBlocked = has(request, mimita_npc_behavior::LOS_BLOCKED);
    ctx.atObjective = has(request, mimita_npc_behavior::AT_OBJECTIVE);
    ctx.atSquadSlot = has(request, mimita_npc_behavior::AT_SQUAD_SLOT);
    ctx.targetDistance = request.targetDistance;
    request.outAction = static_cast<std::uint32_t>(
        actionForGoal(static_cast<UtilityGoalKind>(request.goal), ctx));
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int decideGrenade(mimita_npc_behavior::GrenadeDecisionV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    GrenadeThrowContext ctx;
    auto flag = [&](mimita_npc_behavior::GrenadeFlag value) {
        return (request.flags & static_cast<std::uint32_t>(value)) != 0;
    };
    ctx.hasTarget = flag(mimita_npc_behavior::GRENADE_HAS_TARGET);
    ctx.targetBehindCover = flag(mimita_npc_behavior::GRENADE_TARGET_BEHIND_COVER);
    ctx.selfInBlast = flag(mimita_npc_behavior::GRENADE_SELF_IN_BLAST);
    ctx.trajectoryBlocked = flag(mimita_npc_behavior::GRENADE_TRAJECTORY_BLOCKED);
    ctx.duplicateThrow = flag(mimita_npc_behavior::GRENADE_DUPLICATE_THROW);
    ctx.haveGrenade = flag(mimita_npc_behavior::GRENADE_HAVE_GRENADE);
    ctx.siteDefense = flag(mimita_npc_behavior::GRENADE_SITE_DEFENSE);
    ctx.siteRetake = flag(mimita_npc_behavior::GRENADE_SITE_RETAKE);
    ctx.enemyGroupDensity = request.enemyGroupDensity;
    ctx.friendlyNearImpact = request.friendlyNearImpact;
    request.outScore = scoreGrenadeThrow(ctx);
    request.outAllowed = grenadeThrowAllowed(ctx, request.threshold) ? 1u : 0u;
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int runSelfTest(mimita_npc_behavior::SelfTestV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    std::string report;
    const bool ok = request.which == 1
        ? npcGrenadeReasoningSelfTest(report)
        : npcUtilitySelfTest(report);
    std::snprintf(request.outReport, sizeof(request.outReport), "%s",
                  report.c_str());
    request.outOk = ok ? 1u : 0u;
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int scoreGoal(mimita_npc_behavior::GoalScoreV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    UtilityContext context = makeContext(request.context);
    const UtilityGoalScore score = scoreUtilityGoal(
        static_cast<UtilityGoalKind>(request.goal), context);
    storeScore(score, request.outScore);
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int selectTarget(mimita_npc_behavior::TargetSelectV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request) ||
        request.candidateCount > mimita_npc_behavior::MAX_TARGET_CANDIDATES)
        return 0;
    float bestScore = -1e30f;
    float currentScore = -1e30f;
    std::uint32_t best = 0;
    for (std::uint32_t i = 0; i < request.candidateCount; ++i) {
        const auto& candidate = request.candidates[i];
        if (candidate.id == 0)
            continue;
        const bool finite =
            std::isfinite(candidate.position[0]) &&
            std::isfinite(candidate.position[1]) &&
            std::isfinite(candidate.position[2]) &&
            std::isfinite(candidate.threat);
        if (!finite)
            continue;
        const float distance = std::sqrt(
            candidate.position[0] * candidate.position[0] +
            candidate.position[1] * candidate.position[1] +
            candidate.position[2] * candidate.position[2]);
        const float distanceScore = 1.0f / (1.0f + distance);
        const float healthFraction = candidate.maxHealth > 0
            ? std::clamp(static_cast<float>(candidate.health) /
                         static_cast<float>(candidate.maxHealth), 0.0f, 1.0f)
            : 1.0f;
        const float vulnerability = 1.0f - healthFraction;
        float score = distanceScore * request.distanceBias +
            vulnerability * request.lowHealthBias +
            candidate.threat * request.threatBias;
        const bool isCurrent = candidate.id == request.currentTargetId;
        if (isCurrent) {
            score += request.stickiness;
            currentScore = score;
        }
        if (score > bestScore) {
            bestScore = score;
            best = candidate.id;
        }
    }
    request.outReason = mimita_npc_behavior::TARGET_NONE;
    if (best == 0) {
        request.outTargetId = 0;
    } else if (request.currentTargetId != 0 &&
               best != request.currentTargetId &&
               currentScore > -1e29f &&
               bestScore <= currentScore + request.switchThreshold) {
        request.outTargetId = request.currentTargetId;
        request.outReason = mimita_npc_behavior::TARGET_STICKY;
    } else {
        request.outTargetId = best;
        request.outReason = best == request.currentTargetId
            ? mimita_npc_behavior::TARGET_STICKY
            : mimita_npc_behavior::TARGET_SCORED;
    }
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int decideAttack(mimita_npc_behavior::AttackIntentV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    auto has = [&](mimita_npc_behavior::AttackFlag flag) {
        return (request.flags & static_cast<std::uint32_t>(flag)) != 0;
    };
    request.outAttack = 0;
    if (!has(mimita_npc_behavior::ATTACK_HAS_TARGET)) {
        request.outReason = mimita_npc_behavior::ATTACK_REASON_NO_TARGET;
    } else if (!has(mimita_npc_behavior::ATTACK_TARGET_VISIBLE) ||
               !has(mimita_npc_behavior::ATTACK_LOS_CLEAR)) {
        request.outReason = mimita_npc_behavior::ATTACK_REASON_LOS_BLOCKED;
    } else if (has(mimita_npc_behavior::ATTACK_SAME_TEAM)) {
        request.outReason = mimita_npc_behavior::ATTACK_REASON_SAME_TEAM;
    } else if (!has(mimita_npc_behavior::ATTACK_WEAPON_READY)) {
        request.outReason = mimita_npc_behavior::ATTACK_REASON_WEAPON_NOT_READY;
    } else if (!has(mimita_npc_behavior::ATTACK_REACTION_READY)) {
        request.outReason = mimita_npc_behavior::ATTACK_REASON_REACTION;
    } else if (!has(mimita_npc_behavior::ATTACK_COOLDOWN_READY)) {
        request.outReason = mimita_npc_behavior::ATTACK_REASON_COOLDOWN;
    } else {
        request.outAttack = 1;
        request.outReason = mimita_npc_behavior::ATTACK_REASON_ELIGIBLE;
    }
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int decideFireGate(mimita_npc_behavior::FireGateV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    auto has = [&](mimita_npc_behavior::FireFlag flag) {
        return (request.flags & static_cast<std::uint32_t>(flag)) != 0;
    };
    request.outUseModule = 1;
    request.outAllow = 0;
    request.outStartReload = 0;
    request.outReason = mimita_npc_behavior::FIRE_REASON_ALLOW;
    if (!has(mimita_npc_behavior::FIRE_TARGET_ALIVE)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_TARGET_DEAD;
    } else if (!has(mimita_npc_behavior::FIRE_COOLDOWN_READY)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_COOLDOWN;
    } else if (!has(mimita_npc_behavior::FIRE_HAS_WEAPON)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_NO_WEAPON;
    } else if (has(mimita_npc_behavior::FIRE_RANGE_BLOCKED)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_RANGE;
    } else if (!has(mimita_npc_behavior::FIRE_HAS_AMMO_OR_RESERVE)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_NO_AMMO;
    } else if (has(mimita_npc_behavior::FIRE_MAGAZINE_EMPTY) &&
               !has(mimita_npc_behavior::FIRE_RELOADING)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_START_RELOAD;
        request.outStartReload = 1;
    } else if (has(mimita_npc_behavior::FIRE_RELOADING)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_RELOADING;
    } else if (has(mimita_npc_behavior::FIRE_MAGAZINE_EMPTY)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_MAGAZINE_EMPTY;
    } else if (!has(mimita_npc_behavior::FIRE_LOS_CLEAR)) {
        request.outReason = mimita_npc_behavior::FIRE_REASON_NO_LOS;
    } else {
        request.outAllow = 1;
        request.outReason = mimita_npc_behavior::FIRE_REASON_ALLOW;
    }
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int decideAim(mimita_npc_behavior::AimPolicyV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    request.outUseModule = 1;

    float aimX = request.predictedTarget[0];
    float aimY = request.predictedTarget[1];
    float aimZ = request.predictedTarget[2];
    if (request.flags & mimita_npc_behavior::AIM_PROJECTILE) {
        const float travel = request.targetDistance /
            std::max(1.0f, request.projectileSpeed);
        const float lead = travel + request.aimLeadSeconds;
        aimX = request.targetPos[0] + request.targetVel[0] * lead;
        aimY = request.targetPos[1] + request.targetVel[1] * lead;
        aimZ = request.targetPos[2] + request.targetVel[2] * lead;
    }
    request.outAimPoint[0] = aimX;
    request.outAimPoint[1] = aimY;
    request.outAimPoint[2] = aimZ;

    if (request.profileAimErrorDeg >= 0.0f) {
        request.outErrorDeg = request.profileAimErrorDeg;
    } else if (request.forceHit ||
               (request.flags & mimita_npc_behavior::AIM_FORCE_HIT)) {
        request.outErrorDeg = 0.0f;
    } else {
        request.outErrorDeg =
            request.baseAimErrorDeg *
            (1.0f - request.difficulty01 * request.difficultyErrorScale);
    }
    if (request.outErrorDeg < 0.0f)
        request.outErrorDeg = 0.0f;
    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int decideWeaponChoice(mimita_npc_behavior::WeaponChoiceV1& request)
{
    using namespace mimita_npc_behavior;
    if (request.version != ABI_VERSION ||
        request.structSize != sizeof(request) ||
        request.candidateCount > MAX_WEAPON_CANDIDATES ||
        request.cycleOrderCount > MAX_WEAPON_CANDIDATES)
        return 0;

    request.outUseModule = 1;
    request.outIndex = -1;
    request.outKeepCurrent = 0;
    request.outReason = WEAPON_CHOICE_NO_CANDIDATE;

    auto valid = [&](std::int32_t index) {
        return index >= 0 && index < (std::int32_t)request.candidateCount;
    };
    auto score = [&](const WeaponCandidateV1& c) {
        const float effRange = std::max(1.0f, c.effectiveRange);
        const float rangeFit = 1.0f - std::clamp(
            std::fabs(effRange - request.targetDistance) /
                std::max(effRange, 1.0f), 0.0f, 1.0f);
        const float burst = c.damage * (float)std::max(1u, c.pelletCount);
        const float damageUtility = std::clamp(burst / 80.0f, 0.0f, 1.0f);
        float safety = std::clamp(effRange / 80.0f, 0.0f, 1.0f);
        if (c.behaviorType == WEAPON_BEHAVIOR_ROCKET_LAUNCHER ||
            c.behaviorType == WEAPON_BEHAVIOR_GRENADE_LAUNCHER ||
            c.behaviorType == WEAPON_BEHAVIOR_PROJECTILE)
            safety *= 0.5f;
        else if (c.behaviorType == WEAPON_BEHAVIOR_MELEE ||
                 c.behaviorType == WEAPON_BEHAVIOR_SWORDSWORD)
            safety = 0.05f;
        return rangeFit * request.rangeBias +
               damageUtility * request.damageBias +
               safety * request.safetyBias;
    };
    auto keep = [&](std::uint32_t reason) {
        request.outKeepCurrent = 1;
        request.outIndex = request.currentIndex;
        request.outReason = reason;
    };
    auto choose = [&](std::int32_t index, std::uint32_t reason) {
        request.outIndex = index;
        request.outKeepCurrent = index == request.currentIndex ? 1u : 0u;
        request.outReason = reason;
    };
    auto idIs = [&](const WeaponCandidateV1& c, const char* id) {
        return std::strcmp(c.id, id) == 0;
    };

    if (request.mode == WEAPON_CHOICE_FORCE) {
        if (valid(request.forceIndex))
            choose(request.forceIndex, WEAPON_CHOICE_REASON_FORCE);
        else
            keep(WEAPON_CHOICE_NO_CANDIDATE);
    } else if (request.mode == WEAPON_CHOICE_SCORED) {
        const float currentScore = valid(request.currentIndex)
            ? score(request.candidates[request.currentIndex]) : -1e30f;
        std::int32_t bestIndex = -1;
        float bestScore = -1e30f;
        for (std::uint32_t i = 0; i < request.candidateCount; ++i) {
            if (!request.candidates[i].usable)
                continue;
            const float s = score(request.candidates[i]);
            if (s > bestScore) {
                bestScore = s;
                bestIndex = (std::int32_t)i;
            }
        }
        if (bestIndex < 0) {
            keep(WEAPON_CHOICE_NO_CANDIDATE);
        } else if (valid(request.currentIndex) &&
                   request.candidates[request.currentIndex].usable &&
                   bestIndex != request.currentIndex &&
                   bestScore <= currentScore + request.switchThreshold) {
            keep(WEAPON_CHOICE_KEEP);
        } else {
            choose(bestIndex, WEAPON_CHOICE_SCORED_BEST);
        }
    } else if (request.mode == WEAPON_CHOICE_LEGACY) {
        std::int32_t chosen = -1;
        std::uint32_t reason = WEAPON_CHOICE_NO_CANDIDATE;
        const bool currentEmpty = valid(request.currentIndex) &&
            request.candidates[request.currentIndex].currentAmmo == 0 &&
            !request.candidates[request.currentIndex].isReloading;
        auto firstWithAmmo = [&](bool shotgun, bool explosive, bool revolver) {
            for (std::uint32_t i = 0; i < request.candidateCount; ++i) {
                const WeaponCandidateV1& c = request.candidates[i];
                if (c.currentAmmo == 0)
                    continue;
                const bool match =
                    (shotgun && idIs(c, "shotgun")) ||
                    (explosive && (idIs(c, "rocket_launcher") ||
                                   idIs(c, "grenade_launcher"))) ||
                    (revolver && idIs(c, "revolver"));
                if (match)
                    return (std::int32_t)i;
            }
            return (std::int32_t)-1;
        };
        if (currentEmpty) {
            for (std::uint32_t i = 0; i < request.candidateCount; ++i) {
                if (request.candidates[i].currentAmmo > 0) {
                    chosen = (std::int32_t)i;
                    reason = WEAPON_CHOICE_LEGACY_EMPTY;
                    break;
                }
            }
        } else if (request.targetDistance < request.closeSwitchDist) {
            chosen = firstWithAmmo(true, false, false);
            reason = WEAPON_CHOICE_LEGACY_CLOSE;
        } else if (request.targetDistance > request.farSwitchDist) {
            chosen = firstWithAmmo(false, true, false);
            reason = WEAPON_CHOICE_LEGACY_FAR;
        } else {
            chosen = firstWithAmmo(false, false, true);
            reason = WEAPON_CHOICE_LEGACY_MID;
        }
        if (chosen >= 0 && chosen != request.currentIndex)
            choose(chosen, reason);
        else
            keep(WEAPON_CHOICE_KEEP);
    } else {
        auto ready = [&](std::int32_t index) {
            return valid(index) && request.candidates[index].attackCapable &&
                   request.candidates[index].ammoReady;
        };
        std::int32_t currentPos = -1;
        for (std::uint32_t i = 0; i < request.cycleOrderCount; ++i) {
            if (!valid(request.cycleOrder[i]))
                continue;
            if (request.cycleOrder[i] == request.currentIndex) {
                currentPos = (std::int32_t)i;
                break;
            }
        }
        std::int32_t next = -1;
        if (request.cycleOrderCount > 0) {
            const std::uint32_t count = request.cycleOrderCount;
            const std::uint32_t start = currentPos >= 0
                ? ((std::uint32_t)currentPos + 1u) % count : 0u;
            for (std::uint32_t step = 0; step < count; ++step) {
                const std::int32_t candidate =
                    request.cycleOrder[(start + step) % count];
                if (candidate == request.currentIndex)
                    continue;
                if (ready(candidate)) {
                    next = candidate;
                    break;
                }
            }
        }
        if (next >= 0)
            choose(next, WEAPON_CHOICE_CYCLE_NEXT);
        else
            keep(WEAPON_CHOICE_NO_CANDIDATE);
    }

    request.outGeneration = MIMITA_MODULE_GENERATION;
    return 1;
}

int decideDash(mimita_npc_behavior::DashDecisionV1& request)
{
    if (request.version != mimita_npc_behavior::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;
    auto has = [&](mimita_npc_behavior::DashFlag flag) {
        return (request.flags & static_cast<std::uint32_t>(flag)) != 0;
    };
    const float chance = std::clamp(request.chanceScale, 0.0f, 1.0f);
    const float roll = std::clamp(request.roll, 0.0f, 1.0f);
    bool dash = false;
    if (has(mimita_npc_behavior::DASH_OBJECTIVE_RUSH) &&
        has(mimita_npc_behavior::DASH_OBJECTIVE_KNOWN) &&
        !has(mimita_npc_behavior::DASH_HAS_TARGET)) {
        dash = roll < chance;
    } else if (request.distance > 10.0f && request.difficulty01 > 0.3f) {
        dash = roll < 0.6f * chance;
    } else if (request.healthFraction < 0.35f && request.distance < 8.0f) {
        dash = roll < 0.7f * chance;
    } else if (has(mimita_npc_behavior::DASH_TARGET_SEES_ME) &&
               request.difficulty01 > 0.2f) {
        dash = roll < (0.15f + request.difficulty01 * 0.25f) * chance;
    } else {
        dash = roll < (0.05f + request.difficulty01 * 0.15f) * chance;
    }
    request.outDash = dash ? 1u : 0u;
    request.outGeneration = MIMITA_MODULE_GENERATION;
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

int decideTargetingPolicy(mimita_npc_behavior::TargetingPolicyV1& p)
{
    if (p.version != mimita_npc_behavior::ABI_VERSION ||
        p.structSize != sizeof(p))
        return 0;
    auto parseMode = [](const char* mode) -> std::uint32_t {
        if (std::strcmp(mode, "player") == 0)
            return 0u;
        if (std::strcmp(mode, "opposite_team") == 0)
            return 2u;
        return 1u; // closest (including invalid/unset)
    };
    if (p.configured) {
        p.outConfigured = 1u;
        p.outMode = parseMode(p.mode);
        p.outIncludePlayers = p.includePlayers ? 1u : 0u;
        p.outIncludeNpcs = p.includeNpcs ? 1u : 0u;
    } else {
        p.outConfigured = 0u;
        p.outMode = std::strcmp(p.difficultyTargetMode, "player") == 0
            ? 0u : 1u;
        p.outIncludePlayers = 1u;
        p.outIncludeNpcs =
            (p.damageOtherNpcs || p.teamMatch) ? 1u : 0u;
    }
    p.outGeneration = MIMITA_MODULE_GENERATION;
    static bool loggedOnce = false;
    if (!loggedOnce && gServices && gServices->log) {
        loggedOnce = true;
        char fields[160];
        std::snprintf(
            fields, sizeof(fields),
            "{\"configured\":%u,\"mode\":%u,\"players\":%u,\"npcs\":%u,"
            "\"generation\":%u}",
            p.outConfigured, p.outMode, p.outIncludePlayers,
            p.outIncludeNpcs, p.outGeneration);
        gServices->log(mimita_module::LOG_CATEGORY_NPC_MOVEMENT,
                       mimita_module::LOG_LEVEL_IMPORTANT,
                       "npc.targeting.policy", "",
                       "npc-behavior targeting policy", 0, fields,
                       __FILE__, __LINE__, __func__);
    }
    return 1;
}

int decideStateTiming(mimita_npc_behavior::StateTimingV1& p)
{
    if (p.version != mimita_npc_behavior::ABI_VERSION ||
        p.structSize != sizeof(p))
        return 0;
    float minTime = 0.2f;
    float maxTime = 1.5f;
    const float base = 1.0f - p.difficulty01 * 0.5f;
    switch (p.state) {
    case 0: minTime = 0.3f; maxTime = 3.0f * base; break;  // Idle
    case 1: minTime = 0.5f; maxTime = 4.0f * base; break;  // RandomWalk
    case 2: minTime = 0.3f; maxTime = 2.0f * base; break;  // Chase
    case 3: minTime = 0.3f; maxTime = 3.0f * base; break;  // Circle
    case 4: minTime = 0.3f; maxTime = 2.5f * base; break;  // Strafe
    case 5: minTime = 0.2f; maxTime = 1.5f * base; break;  // Retreat
    case 6: minTime = 0.08f; maxTime = 0.3f; break;        // Attack
    case 7: minTime = 0.1f; maxTime = 0.5f * base; break;  // Recover
    case 8: minTime = 0.3f; maxTime = 2.0f * base; break;  // Advance
    case 9: minTime = 0.3f; maxTime = 2.5f * base; break;  // HoldPosition
    case 10: minTime = 0.3f; maxTime = 2.0f * base; break; // Peek
    case 11: minTime = 0.15f; maxTime = 1.5f * base; break;// Aim
    case 12: minTime = 0.4f; maxTime = 2.5f * base; break; // ZigZag
    case 13: minTime = 0.5f; maxTime = 3.0f * base; break; // Patrol
    default: break;
    }
    p.outMinTime = minTime;
    p.outMaxTime = maxTime;
    p.outGeneration = MIMITA_MODULE_GENERATION;
    static bool loggedOnce = false;
    if (!loggedOnce && gServices && gServices->log) {
        loggedOnce = true;
        char fields[160];
        std::snprintf(fields, sizeof(fields),
                      "{\"state\":%u,\"min_s\":%.3f,\"max_s\":%.3f,"
                      "\"generation\":%u}",
                      p.state, minTime, maxTime, p.outGeneration);
        gServices->log(mimita_module::LOG_CATEGORY_NPC_MOVEMENT,
                       mimita_module::LOG_LEVEL_IMPORTANT,
                       "npc.state-timing", "",
                       "npc-behavior state timing", 0, fields,
                       __FILE__, __LINE__, __func__);
    }
    return 1;
}

// Flip this to true (module-only edit) to let the hot module own state/move/
// attack decisions. Default false preserves the legacy host brain exactly.
constexpr bool kActorPolicyLive = true;

void decideActorState(mimita_npc_behavior::ActorStateV1& p)
{
    p.outUseModule = kActorPolicyLive ? 1u : 0u;
    std::uint32_t state = p.currentState;
    if (p.targetVisible) {
        if (p.healthFraction < 0.35f)
            state = 5u; // Retreat
        else if (p.targetDistance > (p.preferredRange > 0.0f
                                         ? p.preferredRange * 0.8f
                                         : 8.0f))
            state = 2u; // Chase
        else
            state = 6u; // Attack
    } else if (p.targetRemembered) {
        state = 2u; // Chase last known
    } else {
        state = 13u; // Patrol
    }
    p.outState = state;
    const float base = 1.0f - p.difficulty01 * 0.5f;
    switch (state) {
    case 0: p.outMinTime = 0.3f; p.outMaxTime = 3.0f * base; break;
    case 1: p.outMinTime = 0.5f; p.outMaxTime = 4.0f * base; break;
    case 2: p.outMinTime = 0.3f; p.outMaxTime = 2.0f * base; break;
    case 3: p.outMinTime = 0.3f; p.outMaxTime = 3.0f * base; break;
    case 4: p.outMinTime = 0.3f; p.outMaxTime = 2.5f * base; break;
    case 5: p.outMinTime = 0.2f; p.outMaxTime = 1.5f * base; break;
    case 6: p.outMinTime = 0.08f; p.outMaxTime = 0.3f; break;
    case 7: p.outMinTime = 0.1f; p.outMaxTime = 0.5f * base; break;
    case 8: p.outMinTime = 0.3f; p.outMaxTime = 2.0f * base; break;
    case 9: p.outMinTime = 0.3f; p.outMaxTime = 2.5f * base; break;
    case 10: p.outMinTime = 0.3f; p.outMaxTime = 2.0f * base; break;
    case 11: p.outMinTime = 0.15f; p.outMaxTime = 1.5f * base; break;
    case 12: p.outMinTime = 0.4f; p.outMaxTime = 2.5f * base; break;
    case 13: p.outMinTime = 0.5f; p.outMaxTime = 3.0f * base; break;
    default: p.outMinTime = 0.2f; p.outMaxTime = 1.5f * base; break;
    }
    p.outAttack =
        (p.targetVisible && p.attackCooldownReady) ? 1u : 0u;
    p.outJump = 0u;
    p.outDash = 0u;
    float dx = p.targetPos[0] - p.selfPos[0];
    float dy = p.targetPos[1] - p.selfPos[1];
    const float len = std::sqrt(dx * dx + dy * dy);
    if (p.targetVisible && len > 0.001f) {
        p.outMoveDir[0] = dx / len;
        p.outMoveDir[1] = dy / len;
        p.outMoveDir[2] = 0.0f;
    } else {
        p.outMoveDir[0] = 0.0f;
        p.outMoveDir[1] = 0.0f;
        p.outMoveDir[2] = 0.0f;
    }
    p.outGeneration = MIMITA_MODULE_GENERATION;
    static bool loggedOnce = false;
    if (!loggedOnce && gServices && gServices->log) {
        loggedOnce = true;
        char fields[192];
        std::snprintf(
            fields, sizeof(fields),
            "{\"use_module\":%u,\"state\":%u,\"attack\":%u,"
            "\"min_s\":%.3f,\"max_s\":%.3f,\"generation\":%u}",
            p.outUseModule, p.outState, p.outAttack, p.outMinTime,
            p.outMaxTime, p.outGeneration);
        gServices->log(mimita_module::LOG_CATEGORY_NPC_MOVEMENT,
                       mimita_module::LOG_LEVEL_IMPORTANT,
                       "npc.actor-state", "",
                       "npc-behavior actor-state decision", 0, fields,
                       __FILE__, __LINE__, __func__);
    }
}

int MIMITA_MODULE_CALL onEvent(mimita_module::ModuleEventV1* event)
{
    if (!event || !event->payload) return 0;
    if (event->typeId == mimita_module::EVENT_NPC_PAYLOAD_ROLE &&
        event->payloadSize == sizeof(mimita_npc_behavior::PayloadRoleV1))
        return assignPayloadRole(*static_cast<mimita_npc_behavior::PayloadRoleV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_BEHAVIOR_DECISION &&
        event->payloadSize == sizeof(mimita_npc_behavior::DecisionV1))
        return decide(*static_cast<mimita_npc_behavior::DecisionV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_BEHAVIOR_ACTION &&
        event->payloadSize == sizeof(mimita_npc_behavior::ActionV1))
        return actionForGoalEvent(*static_cast<mimita_npc_behavior::ActionV1*>(
            event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_GRENADE_DECISION &&
        event->payloadSize == sizeof(mimita_npc_behavior::GrenadeDecisionV1))
        return decideGrenade(
            *static_cast<mimita_npc_behavior::GrenadeDecisionV1*>(
                event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_BEHAVIOR_SELFTEST &&
        event->payloadSize == sizeof(mimita_npc_behavior::SelfTestV1))
        return runSelfTest(
            *static_cast<mimita_npc_behavior::SelfTestV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_GOAL_SCORE &&
        event->payloadSize == sizeof(mimita_npc_behavior::GoalScoreV1))
        return scoreGoal(
            *static_cast<mimita_npc_behavior::GoalScoreV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_ACTOR_STATE &&
        event->payloadSize == sizeof(mimita_npc_behavior::ActorStateV1)) {
        auto& state =
            *static_cast<mimita_npc_behavior::ActorStateV1*>(event->payload);
        if (state.version != mimita_npc_behavior::ABI_VERSION ||
            state.structSize != sizeof(state))
            return 0;
        decideActorState(state);
        return 1;
    }
    if (event->typeId == mimita_module::EVENT_NPC_STATE_TIMING &&
        event->payloadSize == sizeof(mimita_npc_behavior::StateTimingV1))
        return decideStateTiming(*static_cast<
            mimita_npc_behavior::StateTimingV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_TARGETING_POLICY &&
        event->payloadSize ==
            sizeof(mimita_npc_behavior::TargetingPolicyV1))
        return decideTargetingPolicy(*static_cast<
            mimita_npc_behavior::TargetingPolicyV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_TARGET_SELECT &&
        event->payloadSize == sizeof(mimita_npc_behavior::TargetSelectV1))
        return selectTarget(
            *static_cast<mimita_npc_behavior::TargetSelectV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_ATTACK_INTENT &&
        event->payloadSize == sizeof(mimita_npc_behavior::AttackIntentV1))
        return decideAttack(
            *static_cast<mimita_npc_behavior::AttackIntentV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_FIRE_GATE &&
        event->payloadSize == sizeof(mimita_npc_behavior::FireGateV1))
        return decideFireGate(
            *static_cast<mimita_npc_behavior::FireGateV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_AIM_POLICY &&
        event->payloadSize == sizeof(mimita_npc_behavior::AimPolicyV1))
        return decideAim(
            *static_cast<mimita_npc_behavior::AimPolicyV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_WEAPON_CHOICE &&
        event->payloadSize == sizeof(mimita_npc_behavior::WeaponChoiceV1))
        return decideWeaponChoice(
            *static_cast<mimita_npc_behavior::WeaponChoiceV1*>(event->payload));
    if (event->typeId == mimita_module::EVENT_NPC_DASH_DECISION &&
        event->payloadSize == sizeof(mimita_npc_behavior::DashDecisionV1))
        return decideDash(
            *static_cast<mimita_npc_behavior::DashDecisionV1*>(event->payload));
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
