// 2026-11-08
/* purpose
* host adapter for the npc-behavior module
* builds bounded plain-data requests from host-owned UtilityContext/UtilityState
* and applies the module result; retains no policy implementation
* this file DOES NOT score goals, map actions, or reason about grenades
*/
#include "npc/npc-utility.h"

#include "hot-reload/module-host.h"
#include "modules/api/npc-behavior-api.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace {

void setFlag(mimita_npc_behavior::DecisionV1& request,
             mimita_npc_behavior::ContextFlag flag, bool enabled)
{
    if (enabled)
        request.contextFlags |= static_cast<std::uint32_t>(flag);
}

void copyVec3(const glm::vec3& from, float to[3])
{
    to[0] = from.x;
    to[1] = from.y;
    to[2] = from.z;
}

void fillContext(mimita_npc_behavior::DecisionV1& request,
                 const UtilityContext& ctx)
{
    request.version = mimita_npc_behavior::ABI_VERSION;
    request.structSize = sizeof(request);
    setFlag(request, mimita_npc_behavior::HAS_VISIBLE_TARGET, ctx.hasVisibleTarget);
    setFlag(request, mimita_npc_behavior::HAS_KNOWN_TARGET, ctx.hasKnownTarget);
    setFlag(request, mimita_npc_behavior::HAZARD_ACTIVE, ctx.hazardActive);
    setFlag(request, mimita_npc_behavior::LOS_BLOCKED, ctx.loSBlocked);
    setFlag(request, mimita_npc_behavior::OBJECTIVE_KNOWN, ctx.objectiveKnown);
    setFlag(request, mimita_npc_behavior::PAYLOAD_OBJECTIVE, ctx.payloadObjective);
    setFlag(request, mimita_npc_behavior::PLANTED, ctx.planted);
    setFlag(request, mimita_npc_behavior::IS_DEFUSER, ctx.isDefuserId);
    setFlag(request, mimita_npc_behavior::CAN_PLANT, ctx.canPlant);
    setFlag(request, mimita_npc_behavior::CAN_DEFUSE, ctx.canDefuse);
    setFlag(request, mimita_npc_behavior::AT_OBJECTIVE, ctx.atObjective);
    setFlag(request, mimita_npc_behavior::ON_DEFENSE, ctx.onDefense);
    setFlag(request, mimita_npc_behavior::ENEMY_AREA_KNOWN, ctx.enemyAreaKnown);
    setFlag(request, mimita_npc_behavior::FOCUS_KNOWN, ctx.focusKnown);
    setFlag(request, mimita_npc_behavior::SQUAD_SLOT_KNOWN, ctx.squadSlotKnown);
    setFlag(request, mimita_npc_behavior::AT_SQUAD_SLOT, ctx.atSquadSlot);
    setFlag(request, mimita_npc_behavior::WEAPON_READY, ctx.weaponReady);
    setFlag(request, mimita_npc_behavior::BOMB_DROPPED, ctx.bombDropped);
    setFlag(request, mimita_npc_behavior::IS_RECOVERER, ctx.isRecoverer);
    setFlag(request, mimita_npc_behavior::IS_CARRIER, ctx.isCarrier);
    setFlag(request, mimita_npc_behavior::IS_TRAVEL_SLOT_KNOWN, ctx.travelSlotKnown);
    setFlag(request, mimita_npc_behavior::HAZARD_LOOK_AWAY_KNOWN, ctx.lookAwayKnown);
    setFlag(request, mimita_npc_behavior::SQUAD_APPROACH_KNOWN, ctx.squadApproachKnown);
    setFlag(request, mimita_npc_behavior::SWARM, ctx.swarm);
    setFlag(request, mimita_npc_behavior::DOWN_DASH_OPPORTUNITY, ctx.downDashOpportunity);
    request.downDashChance = ctx.downDashChance;
    request.enemyCount = ctx.enemyCount;
    request.teamAlive = ctx.teamAlive;
    request.team = ctx.team;
    request.targetDistance = ctx.targetDistance;
    request.targetCartDistance = ctx.targetCartDistance;
    request.targetConfidence = ctx.targetConfidence;
    request.healthFraction = ctx.healthFraction;
    request.timeRemaining = ctx.timeRemaining;
    request.bombDefendRadius = ctx.bombDefendRadius;
    request.travelObjectiveBias = ctx.travelObjectiveBias;
    request.travelHuntBias = ctx.travelHuntBias;
    copyVec3(ctx.enemyAreaPos, request.enemyAreaPosition);
    copyVec3(ctx.plantedBombPos, request.plantedBombPosition);
    copyVec3(ctx.objectivePos, request.objectivePosition);
    copyVec3(ctx.travelSlot, request.travelSlot);
    copyVec3(ctx.hazardPos, request.hazardPosition);
    request.hazardRadius = ctx.hazardRadius;
    request.hazardKind = ctx.hazardKind;
    copyVec3(ctx.lookAwayFrom, request.lookAwayFrom);
    copyVec3(ctx.focusPos, request.focusPosition);
    copyVec3(ctx.squadSlot, request.squadSlot);
    copyVec3(ctx.squadAnchor, request.squadAnchor);
    copyVec3(ctx.squadApproachDir, request.squadApproachDirection);
    request.squadApproachSector = ctx.squadApproachSector;
}

mimita_npc_behavior::DecisionV1 makeDecision(const UtilityContext& ctx,
                                             const UtilityState& state,
                                             float dt, float minGoalSeconds,
                                             float switchMargin)
{
    mimita_npc_behavior::DecisionV1 request{};
    fillContext(request, ctx);
    request.dt = dt;
    request.minGoalSeconds = minGoalSeconds;
    request.switchMargin = switchMargin;
    request.currentGoal = static_cast<std::uint32_t>(state.currentGoal);
    request.currentAction = static_cast<std::uint32_t>(state.currentAction);
    request.goalTimer = state.goalTimer;
    request.actionCooldown = state.actionCooldown;
    request.currentScore.kind = static_cast<std::uint32_t>(state.currentScore.kind);
    request.currentScore.relevance = state.currentScore.relevance;
    request.currentScore.distance = state.currentScore.distance;
    request.currentScore.lineOfSight = state.currentScore.lineOfSight;
    request.currentScore.health = state.currentScore.health;
    request.currentScore.enemyCount = state.currentScore.enemyCount;
    request.currentScore.timeRemaining = state.currentScore.timeRemaining;
    request.currentScore.objectiveState = state.currentScore.objectiveState;
    request.currentScore.weaponReadiness = state.currentScore.weaponReadiness;
    request.currentScore.confidence = state.currentScore.confidence;
    request.currentScore.teamInformation = state.currentScore.teamInformation;
    request.currentScore.total = state.currentScore.total;
    return request;
}

void applyDecision(const mimita_npc_behavior::DecisionV1& request,
                   UtilityState& state)
{
    state.currentGoal = static_cast<UtilityGoalKind>(request.outGoal);
    state.currentAction = static_cast<UtilityActionKind>(request.outAction);
    state.goalTimer = request.outGoalTimer;
    state.actionCooldown = request.outActionCooldown;
    state.currentScore.kind = static_cast<UtilityGoalKind>(request.outCurrentScore.kind);
    state.currentScore.relevance = request.outCurrentScore.relevance;
    state.currentScore.distance = request.outCurrentScore.distance;
    state.currentScore.lineOfSight = request.outCurrentScore.lineOfSight;
    state.currentScore.health = request.outCurrentScore.health;
    state.currentScore.enemyCount = request.outCurrentScore.enemyCount;
    state.currentScore.timeRemaining = request.outCurrentScore.timeRemaining;
    state.currentScore.objectiveState = request.outCurrentScore.objectiveState;
    state.currentScore.weaponReadiness = request.outCurrentScore.weaponReadiness;
    state.currentScore.confidence = request.outCurrentScore.confidence;
    state.currentScore.teamInformation = request.outCurrentScore.teamInformation;
    state.currentScore.total = request.outCurrentScore.total;
    state.shouldDownDash = request.outDownDash != 0;
}

mimita_npc_behavior::GrenadeDecisionV1 makeGrenadeRequest(
    const GrenadeThrowContext& ctx, float threshold)
{
    mimita_npc_behavior::GrenadeDecisionV1 request{};
    request.version = mimita_npc_behavior::ABI_VERSION;
    request.structSize = sizeof(request);
    auto flag = [&](mimita_npc_behavior::GrenadeFlag value, bool enabled) {
        if (enabled)
            request.flags |= static_cast<std::uint32_t>(value);
    };
    flag(mimita_npc_behavior::GRENADE_HAS_TARGET, ctx.hasTarget);
    flag(mimita_npc_behavior::GRENADE_TARGET_BEHIND_COVER, ctx.targetBehindCover);
    flag(mimita_npc_behavior::GRENADE_SELF_IN_BLAST, ctx.selfInBlast);
    flag(mimita_npc_behavior::GRENADE_TRAJECTORY_BLOCKED, ctx.trajectoryBlocked);
    flag(mimita_npc_behavior::GRENADE_DUPLICATE_THROW, ctx.duplicateThrow);
    flag(mimita_npc_behavior::GRENADE_HAVE_GRENADE, ctx.haveGrenade);
    flag(mimita_npc_behavior::GRENADE_SITE_DEFENSE, ctx.siteDefense);
    flag(mimita_npc_behavior::GRENADE_SITE_RETAKE, ctx.siteRetake);
    request.enemyGroupDensity = ctx.enemyGroupDensity;
    request.friendlyNearImpact = ctx.friendlyNearImpact;
    request.threshold = threshold;
    return request;
}

} // namespace

const char* utilityGoalName(UtilityGoalKind kind)
{
    switch (kind) {
        case UtilityGoalKind::None:            return "None";
        case UtilityGoalKind::KillTarget:      return "KillTarget";
        case UtilityGoalKind::Survive:         return "Survive";
        case UtilityGoalKind::HoldPosition:    return "HoldPosition";
        case UtilityGoalKind::TakeCover:       return "TakeCover";
        case UtilityGoalKind::MoveToObjective: return "MoveToObjective";
        case UtilityGoalKind::DefendSite:      return "DefendSite";
        case UtilityGoalKind::RotateToSite:    return "RotateToSite";
        case UtilityGoalKind::PlantObjective:  return "PlantObjective";
        case UtilityGoalKind::DefuseObjective: return "DefuseObjective";
        case UtilityGoalKind::RetakeSite:      return "RetakeSite";
        case UtilityGoalKind::HuntArea:        return "HuntArea";
        case UtilityGoalKind::FocusTarget:     return "FocusTarget";
        case UtilityGoalKind::Patrol:          return "Patrol";
    }
    return "Unknown";
}

const char* utilityActionName(UtilityActionKind kind)
{
    switch (kind) {
        case UtilityActionKind::None:             return "None";
        case UtilityActionKind::Approach:         return "Approach";
        case UtilityActionKind::Retreat:          return "Retreat";
        case UtilityActionKind::HoldAngle:        return "HoldAngle";
        case UtilityActionKind::Peek:             return "Peek";
        case UtilityActionKind::Flank:            return "Flank";
        case UtilityActionKind::TakeCover:        return "TakeCover";
        case UtilityActionKind::Reposition:       return "Reposition";
        case UtilityActionKind::Shoot:            return "Shoot";
        case UtilityActionKind::Reload:           return "Reload";
        case UtilityActionKind::SwitchWeapon:     return "SwitchWeapon";
        case UtilityActionKind::ThrowAreaEffect:  return "ThrowAreaEffect";
        case UtilityActionKind::Interact:         return "Interact";
    }
    return "Unknown";
}

UtilityGoalKind selectUtilityGoal(const UtilityContext& ctx,
                                  UtilityState& state,
                                  float dt,
                                  float minGoalSeconds,
                                  float switchMargin,
                                  std::uint32_t actorId,
                                  std::uint32_t tick)
{
    auto& moduleHost = ModuleHost::instance();
    if (actorId != 0 && moduleHost.npcBehaviorLoaded()) {
        mimita_npc_behavior::DecisionV1 request =
            makeDecision(ctx, state, dt, minGoalSeconds, switchMargin);
        request.actorId = actorId;
        request.tick = tick;
        if (moduleHost.npcBehaviorDecision(&request)) {
            applyDecision(request, state);
            return state.currentGoal;
        }
    }
    // No active module generation: keep the last validated goal and advance
    // only its timers. The host never invents a second policy path here.
    state.goalTimer += dt;
    state.actionCooldown = std::max(0.0f, state.actionCooldown - dt);
    return state.currentGoal;
}

UtilityActionKind actionForGoal(UtilityGoalKind goal, const UtilityContext& ctx)
{
    mimita_npc_behavior::ActionV1 request{};
    request.version = mimita_npc_behavior::ABI_VERSION;
    request.structSize = sizeof(request);
    request.goal = static_cast<std::uint32_t>(goal);
    request.targetDistance = ctx.targetDistance;
    if (ctx.weaponReady)
        request.contextFlags |= mimita_npc_behavior::WEAPON_READY;
    if (ctx.loSBlocked)
        request.contextFlags |= mimita_npc_behavior::LOS_BLOCKED;
    if (ctx.atObjective)
        request.contextFlags |= mimita_npc_behavior::AT_OBJECTIVE;
    if (ctx.atSquadSlot)
        request.contextFlags |= mimita_npc_behavior::AT_SQUAD_SLOT;
    if (ModuleHost::instance().npcBehaviorAction(&request))
        return static_cast<UtilityActionKind>(request.outAction);
    return UtilityActionKind::None;
}

UtilityGoalScore scoreUtilityGoal(UtilityGoalKind kind, const UtilityContext& ctx)
{
    mimita_npc_behavior::GoalScoreV1 request{};
    request.version = mimita_npc_behavior::ABI_VERSION;
    request.structSize = sizeof(request);
    request.goal = static_cast<std::uint32_t>(kind);
    request.context = makeDecision(ctx, UtilityState{}, 0.0f, 0.0f, 0.0f);
    if (!ModuleHost::instance().npcGoalScore(&request))
        return UtilityGoalScore{};
    UtilityGoalScore out;
    out.kind = static_cast<UtilityGoalKind>(request.outScore.kind);
    out.relevance = request.outScore.relevance;
    out.distance = request.outScore.distance;
    out.lineOfSight = request.outScore.lineOfSight;
    out.health = request.outScore.health;
    out.enemyCount = request.outScore.enemyCount;
    out.timeRemaining = request.outScore.timeRemaining;
    out.objectiveState = request.outScore.objectiveState;
    out.weaponReadiness = request.outScore.weaponReadiness;
    out.confidence = request.outScore.confidence;
    out.teamInformation = request.outScore.teamInformation;
    out.total = request.outScore.total;
    return out;
}

float scoreGrenadeThrow(const GrenadeThrowContext& ctx)
{
    mimita_npc_behavior::GrenadeDecisionV1 request = makeGrenadeRequest(ctx, 0.05f);
    if (ModuleHost::instance().npcGrenadeDecision(&request))
        return request.outScore;
    return -1.0f;
}

bool grenadeThrowAllowed(const GrenadeThrowContext& ctx, float threshold)
{
    mimita_npc_behavior::GrenadeDecisionV1 request =
        makeGrenadeRequest(ctx, threshold);
    if (ModuleHost::instance().npcGrenadeDecision(&request))
        return request.outAllowed != 0;
    return false;
}

bool npcGrenadeReasoningSelfTest(std::string& report)
{
    mimita_npc_behavior::SelfTestV1 request{};
    request.version = mimita_npc_behavior::ABI_VERSION;
    request.structSize = sizeof(request);
    request.which = 1;
    if (!ModuleHost::instance().npcBehaviorSelfTest(&request)) {
        report += "FAIL: npc-behavior module unavailable\n";
        return false;
    }
    report += request.outReport;
    return request.outOk != 0;
}

bool npcUtilitySelfTest(std::string& report)
{
    mimita_npc_behavior::SelfTestV1 request{};
    request.version = mimita_npc_behavior::ABI_VERSION;
    request.structSize = sizeof(request);
    request.which = 0;
    if (!ModuleHost::instance().npcBehaviorSelfTest(&request)) {
        report += "FAIL: npc-behavior module unavailable\n";
        return false;
    }
    report += request.outReport;
    return request.outOk != 0;
}
