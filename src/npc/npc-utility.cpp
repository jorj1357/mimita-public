// 10 02 2026
// Utility goal/action scoring with hysteresis. Pure; no movement or firing.
#include "npc/npc-utility.h"

#include <algorithm>
#include <cmath>

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

UtilityGoalScore scoreUtilityGoal(UtilityGoalKind kind, const UtilityContext& ctx)
{
    UtilityGoalScore s;
    s.kind = kind;

    const float dist = std::max(0.0f, ctx.targetDistance);
    const float close01 = 1.0f - std::clamp((dist - 2.0f) / 18.0f, 0.0f, 1.0f);
    const float far01 = std::clamp((dist - 4.0f) / 60.0f, 0.0f, 1.0f);
    const float health = std::clamp(ctx.healthFraction, 0.0f, 1.0f);
    const float lowHealth = 1.0f - health;
    const float confidence = std::clamp(ctx.targetConfidence, 0.0f, 1.0f);
    const float losOpen = ctx.loSBlocked ? 0.0f : 1.0f;
    const float enemyFactor = std::clamp((float)ctx.enemyCount / 5.0f, 0.0f, 1.0f);
    const float teamFactor = std::clamp((float)ctx.teamAlive / 5.0f, 0.0f, 1.0f);
    const float timeLow = ctx.timeRemaining > 0.0f
        ? 1.0f - std::clamp(ctx.timeRemaining / 115.0f, 0.0f, 1.0f) : 0.0f;
    const float ready = ctx.weaponReady ? 1.0f : 0.0f;

    // Shared score terms (each kept separate for inspection).
    s.distance = close01;
    s.lineOfSight = losOpen;
    s.health = health;
    s.enemyCount = enemyFactor;
    s.timeRemaining = timeLow;
    s.weaponReadiness = ready;
    s.confidence = confidence;
    s.teamInformation = teamFactor;

    switch (kind) {
        case UtilityGoalKind::KillTarget:
            s.relevance = (ctx.hasVisibleTarget ? 1.0f : 0.5f) * (0.4f + 0.6f * losOpen);
            s.objectiveState = 0.0f;
            s.total = s.relevance * (0.5f + 0.5f * confidence) * ready
                    * (0.6f + 0.4f * close01);
            break;

        case UtilityGoalKind::Survive:
            s.relevance = lowHealth * (0.5f + 0.5f * enemyFactor);
            s.objectiveState = 0.0f;
            s.total = s.relevance * (0.5f + 0.5f * (1.0f - ready));
            break;

        case UtilityGoalKind::HoldPosition:
            s.relevance = ctx.hasKnownTarget ? 0.4f : 0.15f;
            s.objectiveState = ctx.objectiveKnown ? 0.3f : 0.0f;
            s.total = s.relevance * health * losOpen;
            break;

        case UtilityGoalKind::TakeCover:
            s.relevance = (ctx.loSBlocked ? 0.7f : 0.4f) * (0.5f + 0.5f * lowHealth);
            s.objectiveState = 0.0f;
            s.total = s.relevance * health;
            break;

        case UtilityGoalKind::MoveToObjective:
            s.relevance = ctx.objectiveKnown ? (ctx.atObjective ? 0.1f : 0.7f) : 0.0f;
            s.objectiveState = ctx.objectiveKnown ? 1.0f : 0.0f;
            s.total = s.relevance * (0.5f + 0.5f * far01) * health;
            break;

        case UtilityGoalKind::DefendSite:
            s.relevance = ctx.onDefense ? 0.7f : 0.0f;
            s.objectiveState = ctx.objectiveKnown ? 0.6f : 0.0f;
            s.total = s.relevance * (0.5f + 0.5f * teamFactor);
            break;

        case UtilityGoalKind::RotateToSite:
            s.relevance = (ctx.onDefense && ctx.objectiveKnown) ? 0.5f : 0.0f;
            s.objectiveState = ctx.objectiveKnown ? 0.5f : 0.0f;
            s.total = s.relevance * (0.5f + 0.5f * timeLow);
            break;

        case UtilityGoalKind::PlantObjective:
            s.relevance = ctx.canPlant ? 1.0f : 0.0f;
            s.objectiveState = ctx.objectiveKnown ? 1.0f : 0.0f;
            s.total = s.relevance * health;
            break;

        case UtilityGoalKind::DefuseObjective:
            s.relevance = ctx.canDefuse ? 1.0f : 0.0f;
            s.objectiveState = ctx.objectiveKnown ? 1.0f : 0.0f;
            s.total = s.relevance * health * (0.5f + 0.5f * losOpen);
            break;

        case UtilityGoalKind::RetakeSite:
            s.relevance = (ctx.onDefense && ctx.objectiveKnown) ? 0.6f : 0.0f;
            s.objectiveState = ctx.objectiveKnown ? 0.7f : 0.0f;
            s.total = s.relevance * (0.5f + 0.5f * enemyFactor);
            break;

        case UtilityGoalKind::HuntArea:
            // Travel toward the team's best-known enemy area. Only meaningful
            // with a report and no visible/known target close by.
            s.relevance = ctx.enemyAreaKnown ? 1.0f : 0.0f;
            s.objectiveState = ctx.enemyAreaKnown ? 0.5f : 0.0f;
            s.total = s.relevance * std::max(0.0f, ctx.travelHuntBias) *
                      (0.5f + 0.5f * teamFactor);
            break;

        case UtilityGoalKind::FocusTarget:
            // Group focus: advance to the squad slot around the shared focus.
            // Travels toward a focus actor/area but is NOT an aim permission.
            s.relevance = ctx.focusKnown ? 1.2f : 0.0f;
            s.teamInformation = ctx.focusKnown ? 1.0f : 0.0f;
            s.distance = (ctx.squadSlotKnown && !ctx.atSquadSlot) ? 1.0f : 0.5f;
            s.total = s.relevance * (ctx.atSquadSlot ? 0.2f : 1.0f) *
                      (0.5f + 0.5f * teamFactor);
            break;

        case UtilityGoalKind::Patrol:
            // Only meaningful with no hostile and no objective context.
            s.relevance = (!ctx.hasKnownTarget && !ctx.hasVisibleTarget) ? 1.0f : 0.0f;
            s.total = s.relevance;
            break;

        case UtilityGoalKind::None:
            s.total = 0.0f;
            break;
    }

    // Per-team/mode weighting of objective travel (gamemode npc_travel).
    switch (kind) {
        case UtilityGoalKind::MoveToObjective:
        case UtilityGoalKind::DefendSite:
        case UtilityGoalKind::RotateToSite:
        case UtilityGoalKind::PlantObjective:
        case UtilityGoalKind::DefuseObjective:
        case UtilityGoalKind::RetakeSite:
            s.total *= std::max(0.0f, ctx.travelObjectiveBias);
            break;
        default:
            break;
    }
    return s;
}

UtilityGoalKind selectUtilityGoal(const UtilityContext& ctx,
                                  UtilityState& state,
                                  float dt,
                                  float minGoalSeconds,
                                  float switchMargin)
{
    // Advance timers.
    state.goalTimer += dt;
    state.actionCooldown = std::max(0.0f, state.actionCooldown - dt);

    // Build a fixed candidate set. Reusable goals only; objective goals score
    // zero when no objective/role context is present.
    const UtilityGoalKind candidates[] = {
        UtilityGoalKind::KillTarget,
        UtilityGoalKind::Survive,
        UtilityGoalKind::HoldPosition,
        UtilityGoalKind::TakeCover,
        UtilityGoalKind::MoveToObjective,
        UtilityGoalKind::DefendSite,
        UtilityGoalKind::RotateToSite,
        UtilityGoalKind::PlantObjective,
        UtilityGoalKind::DefuseObjective,
        UtilityGoalKind::RetakeSite,
        UtilityGoalKind::HuntArea,
        UtilityGoalKind::FocusTarget,
    };

    UtilityGoalKind best = UtilityGoalKind::None;
    UtilityGoalScore bestScore;
    bestScore.total = -1.0f;

    for (UtilityGoalKind kind : candidates) {
        UtilityGoalScore score = scoreUtilityGoal(kind, ctx);
        if (score.total > bestScore.total) {
            bestScore = score;
            best = kind;
        }
    }

    // No hostile and no objective context: the executor walks the map. The raw
    // scorer leaves KillTarget as the target-less fallback, so reclassify that
    // as Patrol for diagnostics. `makeNavGoal` falls through to the legacy state
    // mapping for Patrol, so navigation behavior is unchanged.
    if (best == UtilityGoalKind::KillTarget &&
        !ctx.hasKnownTarget && !ctx.hasVisibleTarget && !ctx.objectiveKnown) {
        best = UtilityGoalKind::Patrol;
        bestScore = scoreUtilityGoal(UtilityGoalKind::Patrol, ctx);
    }

    // Hysteresis: keep the current goal unless the challenger beats it by the
    // switch margin, or the minimum goal duration has not elapsed.
    const UtilityGoalScore currentScore = scoreUtilityGoal(state.currentGoal, ctx);
    const bool hasCurrent = state.currentGoal != UtilityGoalKind::None;
    const bool minDurationLocked = state.goalTimer < minGoalSeconds;

    bool switchGoal;
    if (!hasCurrent) {
        switchGoal = best != UtilityGoalKind::None;
    } else if (minDurationLocked) {
        switchGoal = false;
    } else {
        switchGoal = bestScore.total > currentScore.total + switchMargin;
    }

    if (switchGoal && best != state.currentGoal) {
        // Cooldown only blocks re-selecting the *same* action immediately;
        // a genuinely new goal is allowed to switch.
        state.currentGoal = best;
        state.currentScore = bestScore;
        state.goalTimer = 0.0f;
        state.currentAction = actionForGoal(best, ctx);
        state.actionCooldown = 0.2f;
    } else if (hasCurrent) {
        state.currentScore = currentScore;
    }

    return state.currentGoal;
}

UtilityActionKind actionForGoal(UtilityGoalKind goal, const UtilityContext& ctx)
{
    switch (goal) {
        case UtilityGoalKind::KillTarget:
            if (!ctx.weaponReady) return UtilityActionKind::Reload;
            return (ctx.targetDistance > 12.0f) ? UtilityActionKind::Approach
                                                : UtilityActionKind::Shoot;
        case UtilityGoalKind::Survive:
            return ctx.loSBlocked ? UtilityActionKind::Retreat
                                  : UtilityActionKind::TakeCover;
        case UtilityGoalKind::HoldPosition:
            return UtilityActionKind::HoldAngle;
        case UtilityGoalKind::TakeCover:
            return UtilityActionKind::TakeCover;
        case UtilityGoalKind::MoveToObjective:
            return UtilityActionKind::Approach;
        case UtilityGoalKind::DefendSite:
            return UtilityActionKind::HoldAngle;
        case UtilityGoalKind::RotateToSite:
            return UtilityActionKind::Reposition;
        case UtilityGoalKind::PlantObjective:
            return ctx.atObjective ? UtilityActionKind::Interact
                                   : UtilityActionKind::Approach;
        case UtilityGoalKind::DefuseObjective:
            return ctx.atObjective ? UtilityActionKind::Interact
                                   : UtilityActionKind::Approach;
        case UtilityGoalKind::RetakeSite:
            return UtilityActionKind::Flank;
        case UtilityGoalKind::HuntArea:
            return UtilityActionKind::Approach;
        case UtilityGoalKind::FocusTarget:
            return ctx.atSquadSlot ? UtilityActionKind::HoldAngle
                                   : UtilityActionKind::Approach;
        case UtilityGoalKind::Patrol:
            return UtilityActionKind::Reposition;
        case UtilityGoalKind::None:
            return UtilityActionKind::None;
    }
    return UtilityActionKind::None;
}

void FightMemory::recordDodgeLeft()  { if (total < MAX) { ++dodgedLeft;  ++total; } }
void FightMemory::recordDodgeRight() { if (total < MAX) { ++dodgedRight; ++total; } }
void FightMemory::recordJump()       { if (total < MAX) { ++jumped;      ++total; } }
void FightMemory::recordHeld()       { if (total < MAX) { ++heldPosition;++total; } }
void FightMemory::decay()
{
    // Halve all counts so the memory forgets older fights.
    dodgedLeft /= 2; dodgedRight /= 2; jumped /= 2; heldPosition /= 2;
    total = dodgedLeft + dodgedRight + jumped + heldPosition;
}

float scoreGrenadeThrow(const GrenadeThrowContext& ctx)
{
    // Hard rejections first.
    if (!ctx.haveGrenade) return -1.0f;
    if (ctx.trajectoryBlocked) return -1.0f;   // direct wall collision
    if (ctx.selfInBlast) return -1.0f;         // self-damage risk
    if (ctx.friendlyNearImpact > 0) return -1.0f;  // friendly fire risk
    if (ctx.duplicateThrow) return -1.0f;      // duplicate throw, no added value

    float score = 0.0f;
    if (ctx.hasTarget) score += 0.3f;
    if (ctx.targetBehindCover) score += 0.4f;  // utility breaks cover
    score += std::min(0.3f, ctx.enemyGroupDensity * 0.1f);
    if (ctx.siteDefense) score += 0.15f;       // doorway denial
    if (ctx.siteRetake) score += 0.2f;

    // No tactical benefit: no target, no cover, no group, no site context.
    if (!ctx.hasTarget && !ctx.targetBehindCover &&
        ctx.enemyGroupDensity <= 1 && !ctx.siteDefense && !ctx.siteRetake)
        return -1.0f;

    return std::clamp(score, -1.0f, 1.0f);
}

bool grenadeThrowAllowed(const GrenadeThrowContext& ctx, float threshold)
{
    return scoreGrenadeThrow(ctx) > threshold;
}

bool npcGrenadeReasoningSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Valid throw: enemy behind cover.
    {
        GrenadeThrowContext ctx;
        ctx.hasTarget = true;
        ctx.targetBehindCover = true;
        if (!grenadeThrowAllowed(ctx)) fail("enemy behind cover should allow a throw");
    }
    // Reject: wall in the throw path.
    {
        GrenadeThrowContext ctx;
        ctx.hasTarget = true; ctx.trajectoryBlocked = true;
        if (grenadeThrowAllowed(ctx)) fail("blocked trajectory must be rejected");
    }
    // Reject: self in blast.
    {
        GrenadeThrowContext ctx;
        ctx.hasTarget = true; ctx.selfInBlast = true;
        if (grenadeThrowAllowed(ctx)) fail("self-damage must be rejected");
    }
    // Reject: friendly fire.
    {
        GrenadeThrowContext ctx;
        ctx.hasTarget = true; ctx.friendlyNearImpact = 2;
        if (grenadeThrowAllowed(ctx)) fail("friendly fire must be rejected");
    }
    // Reject: duplicate.
    {
        GrenadeThrowContext ctx;
        ctx.hasTarget = true; ctx.duplicateThrow = true;
        if (grenadeThrowAllowed(ctx)) fail("duplicate throw must be rejected");
    }
    // Reject: no tactical benefit.
    {
        GrenadeThrowContext ctx;
        if (grenadeThrowAllowed(ctx)) fail("no benefit should be rejected");
    }
    report += "grenade_reasoning=ok\n";

    // Fight memory is bounded and decays.
    {
        FightMemory mem;
        for (int i = 0; i < 20; ++i) mem.recordDodgeLeft();
        if (mem.total > FightMemory::MAX) fail("fight memory must be bounded");
        mem.recordDodgeRight();
        mem.decay();
        if (mem.dodgedLeft >= 8) fail("memory should decay");
        report += "fight_memory=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}

bool npcUtilitySelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // A visible close enemy should make KillTarget the top raw score.
    {
        UtilityContext ctx;
        ctx.hasVisibleTarget = true;
        ctx.hasKnownTarget = true;
        ctx.targetDistance = 6.0f;
        ctx.targetConfidence = 1.0f;
        ctx.weaponReady = true;
        ctx.healthFraction = 1.0f;
        const float kill = scoreUtilityGoal(UtilityGoalKind::KillTarget, ctx).total;
        const float hold = scoreUtilityGoal(UtilityGoalKind::HoldPosition, ctx).total;
        if (!(kill > hold)) fail("visible close enemy should favor KillTarget");
    }

    // Low health should favor Survive over KillTarget at range.
    {
        UtilityContext ctx;
        ctx.hasVisibleTarget = true;
        ctx.hasKnownTarget = true;
        ctx.targetDistance = 40.0f;
        ctx.targetConfidence = 0.5f;
        ctx.weaponReady = false;
        ctx.healthFraction = 0.15f;
        ctx.enemyCount = 4;
        const float survive = scoreUtilityGoal(UtilityGoalKind::Survive, ctx).total;
        const float kill = scoreUtilityGoal(UtilityGoalKind::KillTarget, ctx).total;
        if (!(survive > kill)) fail("low health at range should favor Survive");
    }

    // Objective context gates objective goals.
    {
        UtilityContext ctx;
        const float plantOff = scoreUtilityGoal(UtilityGoalKind::PlantObjective, ctx).total;
        ctx.objectiveKnown = true;
        ctx.canPlant = true;
        const float plantOn = scoreUtilityGoal(UtilityGoalKind::PlantObjective, ctx).total;
        if (!(plantOn > plantOff && plantOn > 0.9f)) fail("canPlant should enable PlantObjective");
    }

    // Hysteresis: a marginally better challenger does not switch during the
    // minimum duration.
    {
        UtilityContext ctx;
        ctx.hasVisibleTarget = true;
        ctx.hasKnownTarget = true;
        ctx.targetDistance = 30.0f;
        ctx.targetConfidence = 1.0f;
        ctx.healthFraction = 0.9f;
        UtilityState state;
        state.currentGoal = UtilityGoalKind::KillTarget;
        const UtilityGoalKind g1 = selectUtilityGoal(ctx, state, 0.1f, 0.6f, 0.08f);
        if (g1 != UtilityGoalKind::KillTarget) fail("should keep current goal during min duration");
        report += "hysteresis=ok\n";
    }

    // Hysteresis: a clearly better goal switches after the min duration.
    {
        UtilityContext ctx;
        ctx.hasVisibleTarget = false;
        ctx.hasKnownTarget = false;
        ctx.healthFraction = 0.05f;   // survive dominates
        ctx.enemyCount = 1;
        UtilityState state;
        state.currentGoal = UtilityGoalKind::HoldPosition;
        state.goalTimer = 2.0f;       // min duration elapsed
        const UtilityGoalKind g = selectUtilityGoal(ctx, state, 0.1f, 0.6f, 0.08f);
        if (g != UtilityGoalKind::Survive) fail("very low health should switch to Survive");
        report += "switch=ok\n";
    }

    // Action mapping: not weapon-ready while engaging -> Reload.
    {
        UtilityContext ctx;
        ctx.weaponReady = false;
        ctx.targetDistance = 5.0f;
        if (actionForGoal(UtilityGoalKind::KillTarget, ctx) != UtilityActionKind::Reload)
            fail("KillTarget without ammo should Reload");
        ctx.weaponReady = true;
        if (actionForGoal(UtilityGoalKind::KillTarget, ctx) != UtilityActionKind::Shoot)
            fail("KillTarget in range with ammo should Shoot");
        report += "action_map=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
