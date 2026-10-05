// 08 09 2026, 14 30
/* purpose
* Main NPC system: per-frame update loop, sensing, and input-state building.
* Builds the two facing modes (aim-at-target vs face-movement) with config-driven
* turn speed, and drives NPC physics through the shared player movement code.
* Does NOT own NPC state selection, per-state movement, combat/firing, or config parsing.
* Does NOT render NPCs or manage NPC spawning.
*/

#include "npc.h"
#include "npc/npc-internal.h"
#include "npc/npc-difficulty-config.h"
#include "gamemode/match-roles.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>

#include "combat/weapon-hit.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "perf/perf.h"
#include "physics/config.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-subgrid.h"
#include "render/render-player.h"
#include "world/world.h"
#include "audio/audio.h"
#include "effects/effect-part.h"
#include "devtools/dev-npc-selection.h"
#include "npc/npc-navigation.h"
#include "npc/npc-navigation-settings.h"
#include "npc/npc-combat.h"
#include "combat/weapon-registry.h"
#include "perf/perf.h"
#include "npc/npc-state-machine.h"
#include "npc/npc-movement-context.h"
#include "actor/actor-intent.h"

float targetCanSeeNpc(const Npc& npc, const World& world)
{
    glm::vec3 fromTarget = npc.sensors.targetPos;
    fromTarget.z += 0.8f;
    glm::vec3 toNpc = npc.body.pos - fromTarget;
    float dist = glm::length(toNpc);
    if (dist < 0.5f) return 0.0f;
    toNpc /= dist;

    AABB rayBounds;
    rayBounds.min = glm::min(fromTarget, npc.body.pos);
    rayBounds.max = glm::max(fromTarget, npc.body.pos);
    static thread_local std::vector<int> candidates;
    candidates.clear();
        appendChunkTrianglesForAABB(world, rayBounds, 0.1f, candidates, "targetCanSeeNpc");

    for (int ti : candidates)
    {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
        const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
        glm::vec3 e1 = tri.b - tri.a;
        glm::vec3 e2 = tri.c - tri.a;
        glm::vec3 pVec = glm::cross(toNpc, e2);
        float det = glm::dot(e1, pVec);
        if (std::fabs(det) < 0.0001f) continue;
        float invDet = 1.0f / det;
        glm::vec3 tVec = fromTarget - tri.a;
        float u = glm::dot(tVec, pVec) * invDet;
        if (u < 0.0f || u > 1.0f) continue;
        glm::vec3 qVec = glm::cross(tVec, e1);
        float v = glm::dot(toNpc, qVec) * invDet;
        if (v < 0.0f || u + v > 1.0f) continue;
        float t = glm::dot(e2, qVec) * invDet;
        if (t > 0.1f && t < dist - 0.5f)
            return 0.0f;
    }
    return 1.0f;
}

const NpcMovementPolicy* activeMovementPolicy(const Npc& npc)
{
    if (npc.actorPresetId.empty())
        return nullptr;
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(npc.actorPresetId);
    if (!preset || !preset->movementPolicy.configured)
        return nullptr;
    return &preset->movementPolicy;
}

const NpcNavigationSettings* activeNavigationSettings(const Npc& npc)
{
    if (npc.actorPresetId.empty())
        return nullptr;
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(npc.actorPresetId);
    if (!preset || !preset->navigationSettings.configured)
        return nullptr;
    return &preset->navigationSettings;
}

// The generic movement executor this actor's preset selected. Independent of
// whether the npc_behavior policy is configured, so a preset can pick an
// executor without opting into the full movement policy. Default: the shared
// Sandbox executor (also used by every actor with no preset).
NpcMovementExecutor activeMovementExecutor(const Npc& npc)
{
    if (npc.actorPresetId.empty())
        return NpcMovementExecutor::SandboxShared;
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(npc.actorPresetId);
    if (!preset)
        return NpcMovementExecutor::SandboxShared;
    NpcMovementExecutor executor = NpcMovementExecutor::SandboxShared;
    npcMovementExecutorFromString(preset->movementPolicy.movementExecutor, executor);
    return executor;
}

bool shouldDash(Npc& npc, float d01, float distance, const WeaponDefinition* def, bool targetCanSeeMe)
{
    const auto& cfg = NpcDifficultyConfig::instance().settings();
    if (npc.dashCooldown > 0.0f)
        return false;

    float healthFraction = (float)npc.body.currentHp / (float)npc.body.maxHp;
    float chanceScale = cfg.dashChance;

    if (distance > 10.0f && d01 > 0.3f)
        return random01(npc.rngState) < (0.6f * chanceScale);

    if (healthFraction < 0.35f && distance < 8.0f)
        return random01(npc.rngState) < (0.7f * chanceScale);

    if (targetCanSeeMe && d01 > 0.2f)
        return random01(npc.rngState) < ((0.15f + d01 * 0.25f) * chanceScale);

    return random01(npc.rngState) < ((0.05f + d01 * 0.15f) * chanceScale);
}

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
                                const glm::vec3& currentVel, float delaySeconds,
                                int rememberedPathPoints,
                                glm::vec3* outVelocity = nullptr) {
    if (delaySeconds <= 0.001f || npc.posRingCount == 0) {
        if (outVelocity) *outVelocity = currentVel;
        return currentPos;
    }

    int tail = (npc.posRingHead - 1 + Npc::MAX_HISTORY_SAMPLES) % Npc::MAX_HISTORY_SAMPLES;
    float targetTime = npc.posRing[tail].time - delaySeconds;
    glm::vec3 bestPos = currentPos;
    glm::vec3 bestVel = currentVel;

    int count = std::clamp(rememberedPathPoints, 1, Npc::MAX_HISTORY_SAMPLES);
    count = std::min(npc.posRingCount, count);
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
    if (outVelocity) *outVelocity = bestVel;
    return bestPos + bestVel * predictTime;
}

void senseWorld(Npc& npc, const World& world, const Player& player, float dt)
{
    NpcSensorContext sensors;
    sensors.selfVel = npc.body.vel + npc.body.externalImpulse;
    sensors.touchFloor = npc.body.ground.hasWorldContact;

    // The authoritative server mirrors either a human player or an NPC into
    // `player`, so this one perception path supports both NPC-vs-player and
    // NPC-vs-NPC radar. A target switch must never inherit the old target's
    // trail or last-known position.
    if (npc.perceptionTargetId != npc.serverTargetId) {
        npc.perceptionTargetId = npc.serverTargetId;
        npc.posRingHead = 0;
        npc.posRingCount = 0;
        npc.targetMemory = MemoryRecord{};
        npc.belief = BeliefState{};
        npc.stateMachine.lastKnownTarget = npc.body.pos;
        npc.stateMachine.lastKnownAge = 0.0f;
    }

    {
        int i = npc.posRingHead;
        npc.posRing[i] = {player.pos, player.vel, npc.sensors.time + dt};
        npc.posRingHead = (i + 1) % Npc::MAX_HISTORY_SAMPLES;
        if (npc.posRingCount < Npc::MAX_HISTORY_SAMPLES)
            npc.posRingCount++;
    }

    const auto& cfg = NpcDifficultyConfig::instance().settings();
    PerceptionTuning tuning;
    tuning.horizontalFovDegrees = cfg.perceptionFovDegrees;
    tuning.sightRangeMeters = cfg.perceptionSightRange;
    tuning.hearingRangeMeters = cfg.perceptionHearingRange;
    tuning.reactionDelayTicks = cfg.perceptionReactionTicks;
    tuning.memoryTicks = cfg.perceptionMemoryTicks;
    tuning.predictionSeconds = cfg.perceptionPredictionSeconds;
    tuning.predictionErrorMeters = cfg.perceptionPredictionErrorMeters;

    // Candidate is a live hostile actor. FOV/range/LOS are applied by the
    // perception module so acquisition can never skip those gates.
    const bool candidateValid = !player.dead && player.currentHp > 0;

    // Line of sight for perception uses the same shared ray trace as combat.
    // Rate-limited to every 5 ticks (matching the prior cache cadence) so the
    // chunk query does not run every tick for every NPC. Between checks the
    // last result is retained, which is fine at 60 Hz for acquisition.
    bool losBlocked = npc.cachedLoSBlocked;
    if (candidateValid) {
        static const int LOS_INTERVAL = 5;
        if (++npc.losTickCounter >= LOS_INTERVAL)
            npc.losTickCounter = 0;
        if (npc.losTickCounter == 0) {
            glm::vec3 fromPos = npc.body.pos + NpcCombat::npcMuzzleOffset();
            glm::vec3 toPos = player.pos + NpcCombat::npcMuzzleOffset();
            glm::vec3 losDir = toPos - fromPos;
            const float losDist = glm::length(losDir);
            losBlocked = false;
            if (losDist > 0.5f) {
                losDir /= losDist;
                float hitDist = losDist;
                if (rayTraverseGridCells(world, fromPos, losDir, losDist, hitDist, nullptr))
                    losBlocked = hitDist > 0.1f && hitDist < losDist - 0.5f;
            }
            npc.cachedLoSBlocked = losBlocked;
        }
    }

    const glm::vec3 facing = glm::length(npc.currentFacing) > 0.0001f
        ? npc.currentFacing : glm::vec3(1.0f, 0.0f, 0.0f);
    PerceptionSnapshot snap = perceive(
        npc, candidateValid, player.pos, player.vel, facing, losBlocked, tuning);
    npc.perception = snap;

    const NpcBehaviorTuning& behavior = npc.behavior;
    const bool radarMode = behavior.active &&
        (behavior.informationMode == "radar" ||
         behavior.informationMode == "perfect_radar");
    const bool perfectRadar = behavior.informationMode == "perfect_radar";
    glm::vec3 knownPos = player.pos;
    glm::vec3 knownVel = player.vel;
    if (radarMode && candidateValid) {
        const float delaySeconds = perfectRadar
            ? 0.0f
            : (float)behavior.radarDelayTicks / 60.0f;
        knownPos = delayedTarget(npc, player.pos, player.vel, delaySeconds,
                                 behavior.rememberedPathPoints, &knownVel);
        if (behavior.radarErrorMeters > 0.0f)
            knownPos = predictTargetPosition(knownPos, glm::vec3(0.0f), 0.0f,
                                             behavior.radarErrorMeters, npc.id);
        snap.radarKnown = !snap.visible;
    }

    // Radar is knowledge, not vision. It can update pursuit memory, but
    // snap.visible remains false behind a wall so combat cannot shoot through
    // cover. `never` is cleared when the authoritative target becomes invalid.
    if (snap.visible) {
        updateMemory(npc.targetMemory, snap, player.pos, player.vel, dt, tuning);
    } else if (radarMode && candidateValid) {
        PerceptionSnapshot radarSnapshot = snap;
        radarSnapshot.radarKnown = true;
        const int radarMemoryTicks = behavior.radarMemoryMode == "long"
            ? behavior.radarMemoryTicks : tuning.memoryTicks;
        PerceptionTuning radarTuning = tuning;
        radarTuning.memoryTicks = radarMemoryTicks;
        updateMemory(npc.targetMemory, radarSnapshot, knownPos, knownVel, dt, radarTuning);
    } else if (radarMode && behavior.radarMemoryMode == "never") {
        npc.targetMemory = MemoryRecord{};
    } else if (behavior.active && behavior.informationMode == "none") {
        npc.targetMemory = MemoryRecord{};
    } else {
        updateMemory(npc.targetMemory, snap, player.pos, player.vel, dt, tuning);
    }

    // Aim error grows with uncertainty: a remembered, unseen target is aimed
    // at less precisely than a currently visible one.
    const float errorMeters = tuning.predictionErrorMeters;
    npc.belief = buildBelief(snap, npc.targetMemory, /*targetActorId=*/0,
                             errorMeters, tuning);

    npc.previousPosition = npc.body.pos;

    // Target acquisition now requires visibility (FOV + range + LOS + alive).
    sensors.hasTarget = npc.belief.hasVisibleTarget;
    if (sensors.hasTarget) {
        sensors.targetPos = player.pos;
        sensors.targetVel = player.vel;
        sensors.predictedTarget = npc.belief.aimPosition;
    } else if (npc.belief.hasTarget) {
        // Search phase: aim at the uncertain last-known position.
        sensors.targetPos = npc.belief.lastKnownPosition;
        sensors.targetVel = glm::vec3(0.0f);
        sensors.predictedTarget = npc.belief.aimPosition;
    } else {
        sensors.targetPos = player.pos;
        sensors.targetVel = player.vel;
        sensors.predictedTarget = player.pos;
    }
    sensors.toTarget = sensors.targetPos - npc.body.pos;
    sensors.targetDistance = glm::length(sensors.toTarget);

    // Keep the legacy lastKnown fields for the search state machine.
    if (snap.visible || (radarMode && candidateValid)) {
        npc.stateMachine.lastKnownTarget = player.pos;
        npc.stateMachine.lastKnownAge = 0.0f;
        npc.stateMachine.pursuitSearchActive = false;
        npc.stateMachine.pursuitSearchTimer = 0.0f;
    } else if (npc.belief.hasTarget) {
        npc.stateMachine.lastKnownAge = npc.targetMemory.ageSeconds;
    } else {
        npc.stateMachine.lastKnownAge += dt;
        npc.stateMachine.pursuitSearchActive = false;
        npc.stateMachine.pursuitSearchTimer = 0.0f;
    }

    npc.sensors = sensors;

    // ── Utility context + goal selection ───────────────────────────
    // Objective fields are filled by objective-aware callers (TeamBrain in a
    // later checkpoint); until then they stay false and only the combat/
    // survival goals are competitive.
    UtilityContext uctx = npc.utilityContext;
    uctx.hasVisibleTarget = npc.belief.hasVisibleTarget;
    uctx.hasKnownTarget = npc.belief.hasTarget;
    uctx.targetDistance = sensors.targetDistance;
    uctx.targetConfidence = npc.belief.confidence;
    uctx.loSBlocked = !npc.perception.hasLineOfSight && npc.perception.candidateValid;
    uctx.healthFraction = npc.body.maxHp > 0
        ? (float)npc.body.currentHp / (float)npc.body.maxHp : 1.0f;
    {
        const auto& rt = npc.body.weaponRuntimes.find(npc.body.equippedWeaponId);
        uctx.weaponReady = rt == npc.body.weaponRuntimes.end()
            || (!rt->second.isReloading && rt->second.currentAmmo > 0);
    }
    npc.utilityContext = uctx;
    selectUtilityGoal(npc.utilityContext, npc.utility, dt);

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
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcMovement, StructuredLevel::Important,
        "npc.state-changed", std::to_string(npc.id), "state_changed",
        (uint32_t)(npc.sensors.time * 60.0f),
        nlohmann::json{
            {"actor", npc.id},
            {"team", npc.body.matchTeam},
            {"profile", npc.behaviorProfileId},
            {"preset", npc.actorPresetId},
            {"state", npcStateName(newState)},
            {"previous_state", npcStateName(oldState)},
            {"target_id", npc.serverTargetId},
            {"target_visible", npc.sensors.hasTarget},
            {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}}},
        __FILE__, __LINE__, __FUNCTION__);
}

// Translate the current brain state into an abstract navigation goal.
// Combat decisions stay here; navigation only sees positions and distances.
// The utility goal (when it covers the situation) takes precedence; otherwise
// the legacy state mapping is used so the state machine remains the executor.
NpcGoal makeNavGoal(const Npc& npc)
{
    NpcGoal goal;
    switch (npc.utility.currentGoal) {
        case UtilityGoalKind::KillTarget:
            if (npc.sensors.hasTarget) {
                goal.kind = NpcGoalKind::FollowActor;
                return goal;
            }
            break;
        case UtilityGoalKind::Survive:
        case UtilityGoalKind::TakeCover:
            if (npc.sensors.hasTarget) {
                goal.kind = NpcGoalKind::FleeActor;
                goal.desiredDistance = 8.0f;
                return goal;
            }
            break;
        case UtilityGoalKind::MoveToObjective:
        case UtilityGoalKind::RetakeSite:
        case UtilityGoalKind::RotateToSite:
            if (glm::length(npc.utilityContext.objectivePos) > 0.001f) {
                goal.kind = NpcGoalKind::ReachPosition;
                goal.targetPos = npc.utilityContext.objectivePos;
                return goal;
            }
            break;
        case UtilityGoalKind::PlantObjective:
        case UtilityGoalKind::DefuseObjective:
            if (glm::length(npc.utilityContext.objectivePos) > 0.001f) {
                goal.kind = NpcGoalKind::ReachPosition;
                goal.targetPos = npc.utilityContext.objectivePos;
                goal.tolerance = 1.0f;
                return goal;
            }
            break;
        default:
            break;
    }

    if (!npc.sensors.hasTarget) {
        if (npc.stateMachine.currentState == NpcState::Chase) {
            goal.kind = NpcGoalKind::ReachPosition;
            // Perception memory is the canonical visual last-known position.
            // Do not let a later hearing report redirect a continuing pursuit.
            goal.targetPos = npc.targetMemory.hasMemory
                ? npc.targetMemory.lastKnownPosition
                : npc.stateMachine.lastKnownTarget;
        } else if (npc.stateMachine.currentState == NpcState::Patrol) {
            // Project a small rolling forward waypoint along the navigator's
            // committed forward direction (the single direction owner). Once
            // reached, the next update creates another waypoint from the new
            // position, so the NPC advances forever without a fake target.
            const glm::vec3 dir = npc.navigator.commitmentActive &&
                    glm::length(glm::vec3(npc.navigator.committedDirection.x,
                                          npc.navigator.committedDirection.y, 0.0f)) > 0.001f
                ? glm::normalize(glm::vec3(npc.navigator.committedDirection.x,
                                           npc.navigator.committedDirection.y, 0.0f))
                : (glm::length(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f)) > 0.001f
                    ? glm::normalize(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f))
                    : glm::vec3(1.0f, 0.0f, 0.0f));
            const NpcMovementPolicy* policy = activeMovementPolicy(npc);
            const float distance = policy ? policy->forwardPatrolDistance : 12.0f;
            goal.kind = NpcGoalKind::ReachPosition;
            goal.targetPos = npc.body.pos + dir * distance;
            goal.tolerance = 1.5f;
        } else if (npc.stateMachine.currentState == NpcState::RandomWalk) {
            goal.kind = NpcGoalKind::ReachPosition;
            goal.targetPos = npc.stateMachine.wanderTarget;
        }
        return goal;
    }

    switch (npc.stateMachine.currentState) {
        case NpcState::Chase:
        case NpcState::Advance:
            goal.kind = NpcGoalKind::FollowActor;
            break;
        case NpcState::Circle:
        case NpcState::Strafe:
        case NpcState::HoldPosition:
        case NpcState::Peek:
        case NpcState::Aim:
            goal.kind = NpcGoalKind::MaintainDistance;
            goal.desiredDistance =
                (npc.behavior.active && npc.behavior.preferredRange > 0.0f)
                    ? npc.behavior.preferredRange
                    : std::clamp(weaponEffectiveRange(npc) * 0.6f, 5.0f, 25.0f);
            break;
        case NpcState::Retreat:
        case NpcState::Recover:
            goal.kind = NpcGoalKind::FleeActor;
            goal.desiredDistance = 8.0f;
            break;
        default:
            goal.kind = NpcGoalKind::FollowActor;
            break;
    }
    return goal;
}

InputState buildInputState(Npc& npc, glm::vec3 moveDir, bool jump, bool dash, bool attack, bool downDash, float dt)
{
    // Route the brain's tactical output through the shared ActorIntent
    // boundary so NPC, human, script, and replay all share one execution
    // translation. Facing smoothing below stays the NPC brain's responsibility.
    ActorIntent intent;
    intent.move = glm::vec2(moveDir.x, moveDir.y);
    intent.jump = jump;
    intent.dash = dash;
    intent.attack = attack;
    intent.reload = false;
    intent.interact = false;
    intent.lookDirection = npc.currentFacing;

    InputState input = ActorIntentAdapter::toInputState(
        intent, glm::length(moveDir) > 0.001f, npc.currentFacing);
    input.downDashPressed = downDash;

    glm::vec3 desiredFwd;

    // Facing-mode timing: switch between "aim at target" (dominant, long
    // stretches) and "face movement" (brief). Being airborne or grounded is
    // irrelevant — a jumping/dashing NPC aims exactly like a standing one.
    const auto& facingCfg = NpcDifficultyConfig::instance().settings();
    if (facingCfg.aimAtTargetMax <= 0.0f)
    {
        npc.facingTargetMode = true;  // no move-mode configured; always aim
    }
    else if (npc.facingModeTimer <= 0.0f)
    {
        npc.facingTargetMode = !npc.facingTargetMode;
        if (npc.facingTargetMode)
        {
            float minT = facingCfg.aimAtTargetMin;
            float maxT = std::max(minT, facingCfg.aimAtTargetMax);
            npc.facingModeTimer = minT + random01(npc.rngState) * (maxT - minT);
        }
        else
        {
            float minT = facingCfg.faceMovementMin;
            float maxT = std::max(minT, facingCfg.faceMovementMax);
            npc.facingModeTimer = minT + random01(npc.rngState) * (maxT - minT);
        }
    }
    else
    {
        npc.facingModeTimer -= dt;
    }

    const bool targetVisible = npc.sensors.hasTarget && !npc.cachedLoSBlocked;
    if (npc.facingTargetMode && targetVisible)
    {
        // Aim mode: face the target so the model turns smoothly at the player
        // no matter how it is moving. The arcade aim error is applied to the
        // shot, not to the model facing.
        glm::vec3 npcEye = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.8f);
        glm::vec3 toTarget = npc.sensors.targetPos + glm::vec3(0.0f, 0.0f, 0.8f) - npcEye;
        glm::vec3 aimDir = glm::length(toTarget) > 0.001f ? glm::normalize(toTarget)
                                                          : glm::vec3(1.0f, 0.0f, 0.0f);
        desiredFwd = safePlanarNormal(aimDir, {1.0f, 0.0f, 0.0f});
    }
    else if (input.movementPressed)
    {
        // Move mode (or no target while moving): face travel direction.
        desiredFwd = safePlanarNormal(moveDir, {1.0f, 0.0f, 0.0f});
    }
    else if (targetVisible)
    {
        // Not moving: look at the target rather than snapping to +X.
        glm::vec3 npcEye = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.8f);
        glm::vec3 toTarget = npc.sensors.targetPos + glm::vec3(0.0f, 0.0f, 0.8f) - npcEye;
        glm::vec3 aimDir = glm::length(toTarget) > 0.001f ? glm::normalize(toTarget)
                                                          : glm::vec3(1.0f, 0.0f, 0.0f);
        desiredFwd = safePlanarNormal(aimDir, {1.0f, 0.0f, 0.0f});
    }
    else
    {
        // Idle with no target: hold the current facing instead of snapping.
        desiredFwd = npc.currentFacing;
    }

    // Apply turn speed limiting: smoothly rotate currentFacing toward desiredFwd
    // The config turnSpeed overrides the per-difficulty tuning value when > 0.
    float turnSpeed = facingCfg.turnSpeed > 0.0f ? facingCfg.turnSpeed : npc.tuning.turnSpeed;
    float maxTurnAngle = turnSpeed * dt;  // degrees this frame
    float angleDiff = glm::degrees(std::acos(
        std::clamp(glm::dot(npc.currentFacing, desiredFwd), -1.0f, 1.0f)));
    if (angleDiff > maxTurnAngle && maxTurnAngle > 0.0f) {
        float t = maxTurnAngle / angleDiff;
        npc.currentFacing = glm::normalize(
            glm::mix(npc.currentFacing, desiredFwd, t));
    } else {
        npc.currentFacing = desiredFwd;
    }
    // The rendered model yaw follows the smoothly-turned facing (this is the
    // ONLY place body.yaw updates — no snapping on fire).
    npc.body.yaw = glm::degrees(std::atan2(npc.currentFacing.y, npc.currentFacing.x));
    input.camForward = npc.currentFacing;

    return input;
}

} // anonymous namespace

void NpcSystem::notifyCombatSound(glm::vec3 position, float intensity)
{
    int i = heardSoundHead;
    heardSounds[i] = {position, currentTime, intensity};
    heardSoundHead = (i + 1) % MAX_HEARD_SOUNDS;
    if (heardSoundCount < MAX_HEARD_SOUNDS)
        heardSoundCount++;
}

bool NpcSystem::recentCombatSoundNear(glm::vec3 pos, float maxAge, float maxDist, glm::vec3& outSource) const
{
    int count = std::min(heardSoundCount, MAX_HEARD_SOUNDS);
    int tail = (heardSoundHead - 1 + MAX_HEARD_SOUNDS) % MAX_HEARD_SOUNDS;
    for (int j = 0; j < count; ++j)
    {
        int i = (tail - j + MAX_HEARD_SOUNDS) % MAX_HEARD_SOUNDS;
        const auto& s = heardSounds[i];
        if (currentTime - s.time > maxAge)
            continue;
        float d = glm::length(s.position - pos);
        if (d < maxDist)
        {
            outSource = s.position;
            return true;
        }
    }
    return false;
}

bool NpcSystem::isNpcNear(glm::vec3 pos, float radius, uint32_t excludeId) const
{
    for (const Npc& n : npcs)
    {
        if (n.id == excludeId) continue;
        if (glm::length(n.body.pos - pos) < radius)
            return true;
    }
    return false;
}

float NpcSystem::nearestOtherNpc(glm::vec3 pos, uint32_t excludeId, glm::vec3& outPos) const
{
    float bestDist = 1e9f;
    for (const Npc& n : npcs)
    {
        if (n.id == excludeId) continue;
        float d = glm::length(n.body.pos - pos);
        if (d < bestDist)
        {
            bestDist = d;
            outPos = n.body.pos;
        }
    }
    return bestDist;
}

int NpcSystem::npcCountNear(glm::vec3 pos, float radius) const
{
    int count = 0;
    for (const Npc& n : npcs)
    {
        if (glm::length(n.body.pos - pos) < radius)
            count++;
    }
    return count;
}

void NpcSystem::recordPlayerInput(const InputFrame& frame, float time)
{
    PlayerMovementEvent evt;
    evt.time = time;
    evt.moveDir = {frame.moveX, frame.moveY};
    evt.jumped = frame.jumpPressed;
    evt.dashed = frame.dashPressed;
    evt.downDashed = frame.downDashPressed;
    evt.froze = frame.freezePressed;

    int i = playerHistoryHead;
    playerHistory[i] = evt;
    playerHistoryHead = (i + 1) % MAX_PLAYER_HISTORY;
    if (playerHistoryCount < MAX_PLAYER_HISTORY)
        playerHistoryCount++;
}

const PlayerMovementEvent* NpcSystem::findPlayerEvent(float targetTime) const
{
    if (playerHistoryCount == 0) return nullptr;

    int bestIdx = -1;
    float bestDist = 1e10f;

    int count = std::min(playerHistoryCount, MAX_PLAYER_HISTORY);
    int tail = (playerHistoryHead - 1 + MAX_PLAYER_HISTORY) % MAX_PLAYER_HISTORY;

    for (int j = 0; j < count; ++j)
    {
        int i = (tail - j + MAX_PLAYER_HISTORY) % MAX_PLAYER_HISTORY;
        float dist = std::abs(playerHistory[i].time - targetTime);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestIdx = i;
        }
    }

    return bestIdx >= 0 ? &playerHistory[bestIdx] : nullptr;
}

void NpcSystem::update(const World& world, Player& player, float dt, const InputFrame& frame)
{
    Perf::ScopedTimer _updateTimer("NpcUpdate");
    Perf::state().current.npcCount = (int)npcs.size();
    currentTime += dt;

    // Record player movement for mirror mode
    recordPlayerInput(frame, currentTime);

    for (Npc& nc : npcs)
    {
        // Register NPC weapon fire for other NPCs' hearing
        if (nc.attackCooldown > 0.0f && nc.sensors.hasTarget)
            notifyCombatSound(nc.body.pos, 0.5f);

        // Offline/Sandbox path: the single local player is the target. The
        // shared executor owns all movement; this only supplies the context.
        NpcMovementContext context;
        context.target = &player;
        context.hasTarget = !player.dead && player.currentHp > 0;
        auto tNpcStart = std::chrono::steady_clock::now();
        updateOneNpc(nc, world, context, dt);
        auto tNpcEnd = std::chrono::steady_clock::now();
        double npcMs = std::chrono::duration<double, std::milli>(tNpcEnd - tNpcStart).count();
        Perf::collectNpcProfile(nc.id, "total", npcMs);
    }
    Perf::flushNpcProfiles();
}

void NpcSystem::updateOneWithTarget(uint32_t npcId, const World& world, Player& player, float dt)
{
    for (Npc& nc : npcs)
    {
        if (nc.id != npcId)
            continue;
        // Register NPC weapon fire for other NPCs' hearing (same as update()).
        if (nc.attackCooldown > 0.0f && nc.sensors.hasTarget)
            notifyCombatSound(nc.body.pos, 0.5f);
        // Server path: `player` is the mode-selected target mirror. The shared
        // executor owns all movement; the mode only chose who the target is.
        NpcMovementContext context;
        context.target = &player;
        context.hasTarget = !player.dead && player.currentHp > 0;
        context.targetActorId = nc.serverTargetId;
        updateOneNpc(nc, world, context, dt);
        break;
    }
}

// ── Bounded structured diagnostics (change-edge only, never per tick) ─────
static void emitCommitmentEvents(const Npc& npc, const NpcCommitmentUpdate& cu,
                                 const MovementCommitmentSettings& s)
{
    if (!cu.created && !cu.replaced && !cu.blocked && !cu.progressFailed)
        return;
    auto& log = StructuredLogger::instance();
    const uint32_t tick = (uint32_t)(npc.sensors.time * 60.0f);
    nlohmann::json fields{
        {"actor", npc.id},
        {"team", npc.body.matchTeam},
        {"profile", npc.behaviorProfileId},
        {"preset", npc.actorPresetId},
        {"executor", npcMovementExecutorName(npc.lastMovementExecutor)},
        {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}},
        {"committed_dir", {cu.direction.x, cu.direction.y}},
        {"previous_dir", {cu.previousDirection.x, cu.previousDirection.y}},
        {"progress_distance", cu.progressDistance},
        {"commit_seconds", s.directionCommitSeconds}};
    auto emit = [&](const char* name, const char* reason) {
        log.writeEvent(StructuredCategory::NpcMovement, StructuredLevel::Important,
                       name, std::to_string(npc.id), reason, tick, fields,
                       __FILE__, __LINE__, __FUNCTION__);
    };
    if (cu.created) emit("npc.movement-commitment-created", "created");
    if (cu.replaced) emit("npc.movement-commitment-replaced", "replaced");
    if (cu.blocked) emit("npc.movement-commitment-blocked", "blocked");
    if (cu.progressFailed) emit("npc.movement-progress-failed", "no_progress");
}

static void emitNavPlanEvents(const Npc& npc, const NpcNavResult& nav,
                              const NpcNavigationSettings* settings)
{
    if (!nav.planCreated && !nav.planFailed)
        return;
    auto& log = StructuredLogger::instance();
    const uint32_t tick = (uint32_t)(npc.sensors.time * 60.0f);
    nlohmann::json fields{
        {"actor", npc.id},
        {"team", npc.body.matchTeam},
        {"profile", npc.behaviorProfileId},
        {"preset", npc.actorPresetId},
        {"executor", npcMovementExecutorName(npc.lastMovementExecutor)},
        {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}},
        {"destination", {nav.destination.x, nav.destination.y, nav.destination.z}},
        {"goal_kind", npc.lastNavGoalKind},
        {"path_nodes", nav.pathNodeCount},
        {"replan_reason", nav.replanReason ? nav.replanReason : ""},
        {"search_radius", settings ? settings->searchRadius : 0.0f}};
    if (nav.planFailed) {
        log.writeEvent(StructuredCategory::NpcMovement, StructuredLevel::Important,
            "npc.nav-plan-failed", std::to_string(npc.id),
            nav.replanReason ? nav.replanReason : "failed", tick, fields,
            __FILE__, __LINE__, __FUNCTION__);
    } else {
        log.writeEvent(StructuredCategory::NpcMovement, StructuredLevel::Important,
            nav.replan ? "npc.nav-replan" : "npc.nav-plan-created",
            std::to_string(npc.id), nav.replanReason ? nav.replanReason : "planned",
            tick, fields, __FILE__, __LINE__, __FUNCTION__);
    }
}

static void emitGoalChangedEvent(Npc& npc, const NpcGoal& goal)
{
    const int kind = (int)goal.kind;
    if (kind == npc.lastNavGoalKind && goal.targetActorId == npc.lastNavGoalActor)
        return;
    npc.lastNavGoalKind = kind;
    npc.lastNavGoalActor = goal.targetActorId;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcMovement, StructuredLevel::Important,
        "npc.goal-changed", std::to_string(npc.id), "goal_changed",
        (uint32_t)(npc.sensors.time * 60.0f),
        nlohmann::json{
            {"actor", npc.id},
            {"team", npc.body.matchTeam},
            {"profile", npc.behaviorProfileId},
            {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}},
            {"goal_kind", kind},
            {"target_actor", goal.targetActorId},
            {"state", (int)npc.stateMachine.currentState}},
        __FILE__, __LINE__, __FUNCTION__);
}

static const char* jumpReasonName(NpcJumpReason reason)
{
    switch (reason) {
        case NpcJumpReason::Obstacle:  return "obstacle";
        case NpcJumpReason::Navigation: return "navigation";
        case NpcJumpReason::Gap:       return "gap";
        case NpcJumpReason::Climbable: return "climbable";
        case NpcJumpReason::None:
        default:                       return "none";
    }
}

// Emit the once-per-second movement-decision snapshot after all overrides and
// physics have run. This is the one complete record that explains what the
// actor wanted, what it finally did, and how far it actually moved.
static void emitMovementDecision(Npc& npc, const char* replanReason,
                                 NpcJumpReason jumpReason, float dt)
{
    npc.movementDecisionTimer -= dt;
    if (npc.movementDecisionTimer > 0.0f)
        return;
    npc.movementDecisionTimer = 1.0f;

    const glm::vec3 moved = npc.body.pos - npc.previousPosition;
    const glm::vec2 finalDir = npc.lastMoveInput;
    const glm::vec3 committed = npc.navigator.commitmentActive
        ? npc.navigator.committedDirection : glm::vec3(0.0f);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcMovement, StructuredLevel::Important,
        "npc.movement-decision", std::to_string(npc.id), "snapshot",
        (uint32_t)(npc.sensors.time * 60.0f),
        nlohmann::json{
            {"actor", npc.id},
            {"team", npc.body.matchTeam},
            {"profile", npc.behaviorProfileId},
            {"preset", npc.actorPresetId},
            {"executor", npcMovementExecutorName(npc.lastMovementExecutor)},
            {"position", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}},
            {"state", npcStateName(npc.stateMachine.currentState)},
            {"target_id", npc.serverTargetId},
            {"target_visible", npc.sensors.hasTarget},
            {"target_remembered", npc.belief.hasTarget && !npc.belief.hasVisibleTarget},
            {"committed_direction", {committed.x, committed.y}},
            {"final_direction", {finalDir.x, finalDir.y}},
            {"replan_reason", replanReason ? replanReason : ""},
            {"jump_reason", jumpReasonName(jumpReason)},
            {"distance_moved", glm::length(moved)},
            {"velocity", {npc.body.vel.x, npc.body.vel.y, npc.body.vel.z}},
            {"on_ground", npc.sensors.touchFloor}},
        __FILE__, __LINE__, __FUNCTION__);
}

void NpcSystem::updateOneNpc(Npc& npc, const World& world,
                             const NpcMovementContext& context, float dt)
{
    if (npc.body.dead || npc.body.currentHp <= 0 || !context.valid()) {
        npc.body.updateModelWorldTransforms();
        return;
    }
    Player& player = *context.target;

    // Mode-selected objective flows into the shared utility context. Sandbox
    // and the current server path leave `hasObjective` false because TeamBrain
    // already writes npc.utilityContext directly; future mode code may fill it.
    if (context.hasObjective) {
        npc.utilityContext.objectiveKnown = context.objective.objectiveKnown;
        npc.utilityContext.objectivePos = context.objective.objectivePos;
        npc.utilityContext.atObjective = context.objective.atObjective;
        npc.utilityContext.canPlant = context.objective.canPlant;
        npc.utilityContext.canDefuse = context.objective.canDefuse;
        npc.utilityContext.onDefense = context.objective.onDefense;
        npc.utilityContext.timeRemaining = context.objective.timeRemaining;
        npc.utilityContext.enemyCount = context.objective.enemyCount;
        npc.utilityContext.teamAlive = context.objective.teamAlive;
    }

    float safeDt = std::max(dt, 0.0001f);

    // Behavior profiles are hot-reloaded. Re-resolve the living NPC when the
    // registry revision changes so pursuit settings take effect without a
    // respawn or server restart.
    {
        auto& profiles = BehaviorProfileRegistry::instance();
        const uint64_t revision = profiles.revision();
        if (!npc.behaviorProfileId.empty() && npc.behaviorRevision != revision) {
            npc.behavior = resolveNpcBehavior(npc.behaviorProfileId);
            npc.behaviorRevision = revision;
            if (npc.behavior.active && npc.behavior.aggression >= 0.0f)
                npc.tuning.aggression = npc.behavior.aggression;
        }
    }

    if (npc.wakeupTimer > 0.0f) {
        npc.wakeupTimer -= safeDt;
        npc.body.updateModelWorldTransforms();
        return;
    }

    npc.dashCooldown = std::max(0.0f, npc.dashCooldown - safeDt);
    npc.downDashCooldown = std::max(0.0f, npc.downDashCooldown - safeDt);
    npc.attackCooldown = std::max(0.0f, npc.attackCooldown - safeDt);
    npc.weaponSwitchCooldown = std::max(0.0f, npc.weaponSwitchCooldown - safeDt);
    npc.hitReactionTimer = std::max(0.0f, npc.hitReactionTimer - safeDt);
    npc.aimTimer = std::max(0.0f, npc.aimTimer);
    npc.stateMachine.stateTimer += safeDt;
    npc.stateMachine.nextDecisionTime -= safeDt;
    npc.stateMachine.retreatTimer += safeDt;

    npc.timeSinceLastShot += safeDt;
    npc.fireRhythmOffset = std::sin(npc.sensors.time * 0.4f + npc.id * 2.1f);

    // Advance mirror movement cycle timer
    {
        const auto& mc = NpcDifficultyConfig::instance().settings();
        if (mc.mirrorMovementEnabled)
            npc.mirrorCycleTimer += safeDt;
        else
            npc.mirrorCycleTimer = 0.0f;
    }

    // Update model-world transforms once per simulation tick
    // (collision code reads these without recomputing them)
    npc.body.updateModelWorldTransforms();

    // Cache weapon definition once per frame (avoids 3+ string-keyed map lookups)
    const WeaponDefinition* cachedWeaponDef = WeaponRegistry::instance().get(npc.body.equippedWeaponId);

    // Background reload for ALL loadout weapons (not just the equipped one).
    // This enables the revolver→shotgun→revolver combo: when the NPC switches
    // away from a weapon, it starts reloading in the background.
    {
        const auto& cfg = NpcDifficultyConfig::instance().settings();
        const std::vector<std::string>& loadout =
            npc.loadoutOverride.empty() ? cfg.weaponLoadout : npc.loadoutOverride;
        for (const auto& wid : loadout) {
            auto it = npc.body.weaponRuntimes.find(wid);
            if (it == npc.body.weaponRuntimes.end()) continue;
            auto& rt = it->second;
            if (!rt.isReloading) continue;
            const WeaponDefinition* wdef = WeaponRegistry::instance().get(wid);
            if (!wdef) continue;
            rt.reloadTimer -= safeDt;
            if (rt.reloadTimer <= 0.0f) {
                int toLoad = wdef->magazineSize - rt.currentAmmo;
                int available = std::min(toLoad, rt.reserveAmmo);
                rt.currentAmmo += available;
                rt.reserveAmmo -= available;
                rt.isReloading = false;
                Debug::log(Debug::Category::NpcCombat,
                    "[NPC RELOAD] npc=%u weapon=%s complete ammo=%d reserve=%d",
                    npc.id, wid.c_str(), rt.currentAmmo, rt.reserveAmmo);
            }
        }
    }

    senseWorld(npc, world, player, safeDt);

    // Hearing: if no target, react to nearby combat sounds
    if (!npc.sensors.hasTarget && npc.stateMachine.lastKnownAge > 2.0f)
    {
        glm::vec3 soundSource;
        float hearRange = 20.0f + npc.tuning.awarenessRange * 0.5f;
        if (recentCombatSoundNear(npc.body.pos, 3.0f, hearRange, soundSource))
        {
            npc.stateMachine.lastKnownTarget = soundSource;
            npc.stateMachine.lastKnownAge = 0.0f;
        }
    }

    if (npc.stateMachine.pursuitSearchActive) {
        npc.stateMachine.pursuitSearchTimer = std::max(
            0.0f, npc.stateMachine.pursuitSearchTimer - safeDt);
        if (npc.stateMachine.pursuitSearchTimer <= 0.0f)
            npc.stateMachine.nextDecisionTime = 0.0f;
    }

    // Weapon switching: pick best weapon for current distance
    if (npc.sensors.hasTarget && npc.weaponSwitchCooldown <= 0.0f)
    {
        const auto& cfg = NpcDifficultyConfig::instance().settings();
        const std::vector<std::string>& loadout =
            npc.loadoutOverride.empty() ? cfg.weaponLoadout : npc.loadoutOverride;
        float dist = npc.sensors.targetDistance;
        std::string bestWeapon = npc.body.equippedWeaponId;

        // Force weapon mode: always use the specified weapon. A role loadout
        // override ignores the global forceWeapon so roles stay authoritative.
        if (npc.loadoutOverride.empty() && !cfg.forceWeapon.empty()) {
            bestWeapon = cfg.forceWeapon;
        } else if (npc.behavior.active) {
            // Scored weapon selection from existing weapon metadata plus the
            // behavior profile weights. Ammo/legality remain authoritative.
            const NpcBehaviorTuning& b = npc.behavior;
            auto weaponScore = [&](const WeaponDefinition& def) {
                const float effRange = weaponSelectionRangeOf(def);
                const float rangeFit = 1.0f - glm::clamp(
                    std::fabs(effRange - dist) / std::max(effRange, 1.0f), 0.0f, 1.0f);
                const float burst = def.damage * (float)std::max(1, def.pelletCount);
                const float damageUtility = glm::clamp(burst / 80.0f, 0.0f, 1.0f);
                float safety = glm::clamp(effRange / 80.0f, 0.0f, 1.0f);
                if (def.behaviorType == WeaponBehaviorType::RocketLauncher ||
                    def.behaviorType == WeaponBehaviorType::GrenadeLauncher ||
                    def.behaviorType == WeaponBehaviorType::Projectile)
                    safety *= 0.5f;
                else if (def.behaviorType == WeaponBehaviorType::Melee ||
                         def.behaviorType == WeaponBehaviorType::Swordsword)
                    safety = 0.05f;
                return rangeFit * b.weaponRangeBias
                     + damageUtility * b.weaponDamageBias
                     + safety * b.weaponSafetyBias;
            };
            auto usable = [&](const std::string& wid) {
                auto it = npc.body.weaponRuntimes.find(wid);
                if (it == npc.body.weaponRuntimes.end()) return false;
                const auto& rt = it->second;
                return rt.currentAmmo > 0 || rt.reserveAmmo > 0 || rt.isReloading;
            };

            const WeaponDefinition* curDef =
                WeaponRegistry::instance().get(npc.body.equippedWeaponId);
            const float currentScore = curDef ? weaponScore(*curDef) : -1e30f;

            float bestScore = -1e30f;
            for (const auto& wid : loadout) {
                if (!usable(wid)) continue;
                const WeaponDefinition* d = WeaponRegistry::instance().get(wid);
                if (!d) continue;
                const float s = weaponScore(*d);
                if (s > bestScore) { bestScore = s; bestWeapon = wid; }
            }
            // Keep the current weapon unless a candidate is clearly better.
            if (curDef && usable(npc.body.equippedWeaponId) &&
                bestWeapon != npc.body.equippedWeaponId &&
                bestScore <= currentScore + b.weaponSwitchThreshold)
                bestWeapon = npc.body.equippedWeaponId;
        } else {
            // Legacy distance-based switching (unchanged without a profile).
            // Check ammo: if current weapon is empty, force a switch
            auto curIt = npc.body.weaponRuntimes.find(npc.body.equippedWeaponId);
            bool currentEmpty = curIt != npc.body.weaponRuntimes.end()
                && curIt->second.currentAmmo <= 0 && !curIt->second.isReloading;

            if (currentEmpty) {
                for (const auto& wid : loadout) {
                    auto wit = npc.body.weaponRuntimes.find(wid);
                    if (wit != npc.body.weaponRuntimes.end() && wit->second.currentAmmo > 0) {
                        bestWeapon = wid;
                        break;
                    }
                }
            } else if (dist < cfg.closeSwitchDist) {
                for (const auto& wid : loadout) {
                    if (wid == "shotgun") {
                        auto it = npc.body.weaponRuntimes.find(wid);
                        if (it != npc.body.weaponRuntimes.end() && it->second.currentAmmo > 0)
                            bestWeapon = wid;
                        break;
                    }
                }
            } else if (dist > cfg.farSwitchDist) {
                for (const auto& wid : loadout) {
                    if (wid == "rocket_launcher" || wid == "grenade_launcher") {
                        auto it = npc.body.weaponRuntimes.find(wid);
                        if (it != npc.body.weaponRuntimes.end() && it->second.currentAmmo > 0) {
                            bestWeapon = wid;
                            break;
                        }
                    }
                }
            } else {
                for (const auto& wid : loadout) {
                    if (wid == "revolver") {
                        auto it = npc.body.weaponRuntimes.find(wid);
                        if (it != npc.body.weaponRuntimes.end() && it->second.currentAmmo > 0)
                            bestWeapon = wid;
                        break;
                    }
                }
            }
        }

        if (bestWeapon != npc.body.equippedWeaponId) {
            const std::string previousWeapon = npc.body.equippedWeaponId;
            const std::string switched = bestWeapon;
            npcSwitchWeapon(npc, bestWeapon);
            npc.weaponSwitchCooldown = cfg.switchCooldown;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcCombat, StructuredLevel::Important,
                "npc.weapon-switched", std::to_string(npc.id), "weapon_switch",
                (uint32_t)(npc.sensors.time * 60.0f),
                nlohmann::json{
                    {"actor", npc.id},
                    {"team", npc.body.matchTeam},
                    {"profile", npc.behaviorProfileId},
                    {"weapon", switched},
                    {"previous_weapon", previousWeapon},
                    {"target_distance", dist},
                    {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }

    bool wantDownDash = false;
    if (npc.sensors.hasTarget && !npc.sensors.touchFloor && npc.downDashCooldown <= 0.0f)
    {
        const auto& cfg = NpcDifficultyConfig::instance().settings();
        float heightAbove = npc.body.pos.z - npc.sensors.targetPos.z;
        if (heightAbove > 3.0f && random01(npc.rngState) < (0.8f * cfg.downDashChance))
            wantDownDash = true;
    }

    // Panic/freeze on hit: gated by hitReactionEnabled config
    if (npc.hitReactionTimer > 0.0f)
    {
        const auto& cfg = NpcDifficultyConfig::instance().settings();
        if (cfg.hitReactionEnabled) {
            npc.stateMachine.currentState = NpcState::Recover;
            npc.stateMachine.recoverTimer = npc.hitReactionTimer * cfg.hitReactionDurationScale;
            npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime,
                npc.hitReactionTimer * cfg.hitReactionDurationScale + 0.1f);
        }
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
            Perf::ScopedTimer _decTimer("NpcSecondDecision");
            NpcState oldState = npc.stateMachine.currentState;
            NpcState newState = pickNextState(npc);

            if (newState == NpcState::Retreat && oldState != NpcState::Retreat)
                npc.stateMachine.retreatTimer = 0.0f;

            if (newState == NpcState::Recover)
                npc.stateMachine.recoverTimer = 0.2f + random01(npc.rngState) * 0.3f;

            if (newState == NpcState::Circle && oldState != NpcState::Circle)
            {
                npc.stateMachine.orbitSwapTimer = 0.1f + random01(npc.rngState) * 1.5f;
                glm::vec3 otherPos;
                float nearest;
                { Perf::ScopedTimer _t("NpcNearestOther"); nearest = nearestOtherNpc(npc.body.pos, npc.id, otherPos); }
                if (nearest < 8.0f)
                {
                    glm::vec2 toOther(otherPos.x - npc.body.pos.x, otherPos.y - npc.body.pos.y);
                    glm::vec2 toTarget(npc.sensors.targetPos.x - npc.body.pos.x, npc.sensors.targetPos.y - npc.body.pos.y);
                    float cross = toTarget.x * toOther.y - toTarget.y * toOther.x;
                    npc.stateMachine.orbitDirection = cross > 0.0f ? 1.0f : -1.0f;
                }
                else
                {
                    npc.stateMachine.orbitDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                }
                npc.stateMachine.orbitDistance = 1.0f + random01(npc.rngState) * 9.0f;
            }

            if (newState == NpcState::Strafe && oldState != NpcState::Strafe)
            {
                glm::vec3 otherPos;
                float nearest;
                { Perf::ScopedTimer _t("NpcNearestOther"); nearest = nearestOtherNpc(npc.body.pos, npc.id, otherPos); }
                if (nearest < 8.0f)
                {
                    glm::vec2 toOther(otherPos.x - npc.body.pos.x, otherPos.y - npc.body.pos.y);
                    glm::vec2 toTarget(npc.sensors.targetPos.x - npc.body.pos.x, npc.sensors.targetPos.y - npc.body.pos.y);
                    float cross = toTarget.x * toOther.y - toTarget.y * toOther.x;
                    npc.stateMachine.strafeDirection = cross > 0.0f ? -1.0f : 1.0f;
                }
                else
                {
                    npc.stateMachine.strafeDirection = random01(npc.rngState) < 0.5f ? 1.0f : -1.0f;
                }
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

    const auto& npcDifficulty = NpcDifficultyConfig::instance().settings();
    // Pre-gather collision triangles once for local navigation checks.
    // The radius follows the configurable wall search distance.
    glm::vec3 gatherCenter = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.5f);
    // The gather radius must cover the search lookahead so the planner can see
    // open space ahead, not just the immediate wall-avoid range.
    const float navigationGatherDistance = std::max({
        3.0f, npcDifficulty.wallSearchDistance + 0.5f,
        npcDifficulty.searchLookahead * 2.0f + 1.5f });
    AABB localBounds{gatherCenter - glm::vec3(navigationGatherDistance), gatherCenter + glm::vec3(navigationGatherDistance)};
    static thread_local std::vector<int> nearCandidates;
    nearCandidates.clear();
        appendChunkTrianglesForAABB(world, localBounds, 0.0f, nearCandidates, "npcNearCandidates");

    // ── Movement commitment (single forward/pursuit direction owner) ────
    // Resolve the behavior-profile tuning once, then evaluate the commitment
    // before the state machine builds its direction, so Patrol and pursuit both
    // steer along one committed direction instead of re-deciding every tick.
    MovementCommitmentSettings commitmentSettings;
    {
        const NpcBehaviorTuning& b = npc.behavior;
        commitmentSettings.repathIntervalSeconds = b.repathIntervalSeconds;
        commitmentSettings.goalMoveThresholdMeters = b.goalMoveThresholdMeters;
        commitmentSettings.enabled = b.commitmentEnabled;
        commitmentSettings.directionCommitSeconds = b.commitmentDirectionSeconds;
        commitmentSettings.progressCheckSeconds = b.commitmentProgressCheckSeconds;
        commitmentSettings.minimumProgressMeters = b.commitmentMinimumProgressMeters;
        commitmentSettings.candidateDistanceMeters = b.commitmentCandidateDistanceMeters;
        commitmentSettings.allowReverse = b.commitmentAllowReverse;
        commitmentSettings.avoidRecentPath = b.commitmentAvoidRecentPath;
        commitmentSettings.recentPathAvoidRadius = b.commitmentRecentPathAvoidRadius;
        commitmentSettings.visibleEnemyAllowsCombatMovement =
            b.commitmentVisibleEnemyAllowsCombatMovement;
        commitmentSettings.forwardBias = b.commitmentForwardBias;
        commitmentSettings.targetProgressBias = b.commitmentTargetProgressBias;
        commitmentSettings.openDistanceBias = b.commitmentOpenDistanceBias;
        commitmentSettings.reversePenalty = b.commitmentReversePenalty;
    }
    {
        const glm::vec3 committedPlanar(npc.navigator.committedDirection.x,
                                        npc.navigator.committedDirection.y, 0.0f);
        const glm::vec3 facingPlanar(npc.currentFacing.x, npc.currentFacing.y, 0.0f);
        const glm::vec3 forwardDir = (npc.navigator.commitmentActive &&
                                      glm::length(committedPlanar) > 0.001f)
            ? glm::normalize(committedPlanar)
            : (glm::length(facingPlanar) > 0.001f
                ? glm::normalize(facingPlanar) : glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 targetDir{0.0f};
        if (!npc.sensors.hasTarget &&
            (npc.stateMachine.currentState == NpcState::Chase ||
             npc.stateMachine.pursuitSearchActive ||
             npc.stateMachine.currentState == NpcState::Recover)) {
            const glm::vec3 known = npc.targetMemory.hasMemory
                ? npc.targetMemory.lastKnownPosition : npc.stateMachine.lastKnownTarget;
            targetDir = glm::vec3(known.x - npc.body.pos.x,
                                  known.y - npc.body.pos.y, 0.0f);
        }
        const NpcCommitmentUpdate cu = npc.navigator.updateCommitment(
            npc, forwardDir, targetDir, npc.sensors.hasTarget, world, nearCandidates,
            commitmentSettings, safeDt, npc.sensors.time, npcDifficulty.searchMemorySeconds);
        emitCommitmentEvents(npc, cu, commitmentSettings);
    }

    glm::vec3 moveDir;
    bool jump, dash, attack;
    bool inMirrorPhase = false;

    // Mirror movement cycle: alternate between normal AI and replaying player inputs
    const auto& mirrorCfg = NpcDifficultyConfig::instance().settings();
    if (mirrorCfg.mirrorMovementEnabled && npc.sensors.hasTarget)
    {
        float cycleDuration = mirrorCfg.mirrorNormalDuration + mirrorCfg.mirrorReplayDuration;
        float phaseTime = std::fmod(npc.mirrorCycleTimer, cycleDuration);
        inMirrorPhase = phaseTime >= mirrorCfg.mirrorNormalDuration;
        npc.mirrorPhaseActive = inMirrorPhase;

        if (inMirrorPhase)
        {
            float mirrorPhaseTime = phaseTime - mirrorCfg.mirrorNormalDuration;
            float targetTime = currentTime - mirrorCfg.mirrorHistorySeconds + mirrorPhaseTime;
            const PlayerMovementEvent* evt = findPlayerEvent(targetTime);

            if (evt)
            {
                moveDir = glm::vec3(evt->moveDir, 0.0f);
                jump = mirrorCfg.mirrorJumpEnabled && evt->jumped;
                dash = mirrorCfg.mirrorDashEnabled && evt->dashed;
                attack = false; // don't force attack from mirror
                if (mirrorCfg.mirrorKeepAimingAtTarget)
                    npc.facingTargetMode = true;
            }
            else
            {
                inMirrorPhase = false;
            }
        }
    }

    // Search-memory sampling + generic no-progress watchdog. The recent-path
    // rings and the committed direction live in the navigator now; this only
    // feeds them and forces a recommit when progress stops. Both run in every
    // state so a chasing NPC also breaks out of a local minimum.
    {
        auto& sm = npc.stateMachine;
        const float now = npc.sensors.time;

        sm.patrolSnapshotTimer -= safeDt;
        if (sm.patrolSnapshotTimer <= 0.0f)
        {
            sm.patrolSnapshotTimer = npcDifficulty.searchSnapshotSeconds;
            bool record = npc.navigator.recentVisitedCount == 0;
            if (!record) {
                const int last = (npc.navigator.recentVisitedHead - 1 + NpcNavigator::kRecentMax)
                                 % NpcNavigator::kRecentMax;
                record = glm::length(glm::vec2(
                    npc.body.pos.x - npc.navigator.recentVisited[last].pos.x,
                    npc.body.pos.y - npc.navigator.recentVisited[last].pos.y)) > 0.5f;
            }
            if (record)
                npc.navigator.pushVisited(npc.body.pos, now);
        }

        const float progress = glm::length(glm::vec2(
            npc.body.pos.x - sm.patrolLastProgressPos.x,
            npc.body.pos.y - sm.patrolLastProgressPos.y));
        if (progress > 0.5f) {
            sm.patrolLastProgressPos = npc.body.pos;
            sm.patrolNoProgressTimer = 0.0f;
        } else if (npc.body.ground.onGround && glm::length(npc.lastMoveInput) > 0.1f) {
            sm.patrolNoProgressTimer += safeDt;
        }

        if (sm.patrolNoProgressTimer > npcDifficulty.searchNoProgressSeconds)
        {
            npc.navigator.pushBlocked(npc.body.pos, now);
            if (sm.currentState != NpcState::Patrol && !inMirrorPhase)
            {
                // Break the local ram with a temporary lateral correction. The
                // long-range commitment remains intact; this is not a new goal.
                glm::vec3 base = glm::length(npc.lastMoveInput) > 0.1f
                    ? glm::normalize(glm::vec3(npc.lastMoveInput.x, npc.lastMoveInput.y, 0.0f))
                    : (glm::length(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f)) > 0.001f
                        ? glm::normalize(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f))
                        : glm::vec3(1.0f, 0.0f, 0.0f));
                const glm::vec3 side{-base.y, base.x, 0.0f};
                const glm::vec3 detour = npc.navigator.chooseBestOpenDirection(
                    npc, side, glm::vec3(0.0f), world, nearCandidates,
                    commitmentSettings, now, npcDifficulty.searchMemorySeconds);
                if (glm::length(detour) > 0.001f) {
                    npc.navigator.startLocalCorrection(detour, 1.0f);
                }
            }
            sm.patrolNoProgressTimer = 0.0f;
            sm.patrolLastProgressPos = npc.body.pos;
        }
    }

    if (!inMirrorPhase)
    {
        computeStateMovement(npc, moveDir, jump, dash, attack, safeDt);
    }

    // ── Goal -> navigation -> movement ──────────────────────────────
    // The navigator turns the brain's abstract goal into a cached route and a
    // steering direction. It only overrides the tactical direction while a
    // multi-node route is active, so local strafing/circling is preserved when
    // no detour is needed. Movement execution still flows through the same
    // situational checks, buildInputState, and shared kernel below.
    const MovementConfig* navMovement =
        RoleMovementCache::instance().get(npc.movementProfileId);
    if (!navMovement)
        navMovement = NpcDifficultyConfig::instance().npcMovementConfig();

    // Actor-preset policy (null = legacy brain). It gates the special movement
    // decisions below without adding any Counter-Strike-specific branch.
    const NpcMovementPolicy* policy = activeMovementPolicy(npc);
    NpcJumpReason jumpReason = NpcJumpReason::None;

    // Generic executor selection. Every actor uses the same shared Sandbox
    // movement executor by default. If the actor preset has a navigation block,
    // the shared executor must honor it; this changes route-planning settings,
    // not the movement implementation. The direct executor still disables
    // surface planning explicitly.
    const NpcMovementExecutor executor = activeMovementExecutor(npc);
    npc.lastMovementExecutor = executor;
    const NpcNavigationSettings* navSettings = nullptr;
    NpcNavigationSettings directSettings;
    if (executor == NpcMovementExecutor::SandboxShared ||
        executor == NpcMovementExecutor::SurfaceNavigation) {
        navSettings = activeNavigationSettings(npc);
    } else if (executor == NpcMovementExecutor::Direct) {
        directSettings.configured = true;
        directSettings.mode = "direct";
        navSettings = &directSettings;
    }

    const char* replanReasonThisTick = nullptr;
    if (!inMirrorPhase)
    {
        // The mode may own the goal (objective/plant/defuse); otherwise the
        // shared goal mapping decides. Movement execution is identical either way.
        const NpcGoal navGoal = context.hasGoalOverride ? context.goalOverride
                                                        : makeNavGoal(npc);
        emitGoalChangedEvent(npc, navGoal);
        const NpcNavResult nav =
            npc.navigator.update(npc, navGoal, world, navMovement, safeDt,
                                 navSettings, policy, &commitmentSettings);
        emitNavPlanEvents(npc, nav, navSettings);
        replanReasonThisTick = nav.replanReason;
        const NpcTraversalStep trav =
            npc.traversal.update(npc, nav, navMovement, safeDt);
        // Traversal steers when following a detour route or performing a
        // non-walk traversal; otherwise the tactical direction is preserved.
        const bool traversalSteers = nav.detour ||
            (trav.active && trav.type != TraversalType::Walk);
        if (trav.active && traversalSteers &&
            glm::length(trav.direction) > 0.01f)
            moveDir = trav.direction;
        // A blocked route owns the steering: turn/hold from the navigator, and
        // never let traversal launch a jump/dash into the confirmed obstacle.
        if (nav.blocked)
        {
            moveDir = nav.dir;
        }
        else
        {
            if (trav.jump && npc.sensors.touchFloor)
            {
                jump = true;
                jumpReason = NpcJumpReason::Navigation;
            }
            if (trav.dash && navMovement && navMovement->dashEnabled)
                dash = true;
            if (trav.downDash && navMovement && navMovement->downDashEnabled)
                wantDownDash = true;
        }
    }

    // Situational jump: only a real obstacle ahead justifies it. A stuck actor
    // may also hop to escape a local pocket. This recovery BYPASSES the actor
    // policy on purpose: a policy actor (e.g. Counter-Strike) that spawns facing
    // a wall must still be able to leave it. The policy is still enforced for
    // the jump reason at the bottom of this function via
    // npcPolicyAllowsJump(), so a preset that forbids obstacle jumps never
    // receives one.
    if (npc.sensors.touchFloor && !jump && glm::length(moveDir) > 0.1f)
    {
        if (NpcNavigation::obstacleInDirection(npc, moveDir, 1.8f, world, nearCandidates))
        {
            jump = true;
            jumpReason = NpcJumpReason::Obstacle;
        }
        else if (NpcNavigation::isStuck(npc))
        {
            jump = true;
            jumpReason = NpcJumpReason::Obstacle;
        }
    }

    // Wall climb
    if (npc.sensors.touchFloor && !jump && glm::length(moveDir) > 0.1f)
    {
        glm::vec3 wallNormal;
        if (NpcNavigation::isClimbableWall(npc, moveDir, world, wallNormal, nearCandidates))
        {
            jump = true;
            jumpReason = NpcJumpReason::Climbable;
        }
    }

    // LOS is owned by perception (senseWorld), which uses the same shared ray
    // trace; cachedLoSBlocked is kept in sync there. Dash and combat decisions
    // reuse it instead of tracing a second time.

    // Situational dash (skip LOS gather when dash is on cooldown). A policy
    // only permits a dash for an explicit reason: attacking or escaping (the
    // navigator owns the navigation-gap dash via the traversal step above).
    if (!dash && npc.sensors.hasTarget && npc.dashCooldown <= 0.0f)
    {
        bool dashAllowed = true;
        if (policy)
        {
            const NpcState st = npc.stateMachine.currentState;
            NpcDashReason reason = NpcDashReason::None;
            if (st == NpcState::Advance || st == NpcState::Attack || st == NpcState::Chase)
                reason = NpcDashReason::Attack;
            else if (st == NpcState::Retreat)
                reason = NpcDashReason::Escape;
            dashAllowed = npcPolicyAllowsDash(*policy, reason);
        }
        if (dashAllowed)
        {
            bool targetCanSee = !npc.cachedLoSBlocked;
            dash = shouldDash(npc, difficulty01(npc.difficulty), npc.sensors.targetDistance, cachedWeaponDef, targetCanSee);
        }
    }

    // Cover seeking
    if (npc.sensors.hasTarget && glm::length(moveDir) > 0.001f)
    {
        bool wantsCover = npc.stateMachine.currentState == NpcState::Recover;
        if (!wantsCover)
        {
            const float coverThreshold = policy ? policy->retreatHealthFraction : 0.4f;
            wantsCover = npcHealthFraction(npc.body.currentHp, npc.body.maxHp) < coverThreshold;
        }
        if (!wantsCover)
        {
            const auto& rt = npc.body.weaponRuntimes.find(npc.body.equippedWeaponId);
            wantsCover = rt != npc.body.weaponRuntimes.end() && rt->second.isReloading;
        }

        if (wantsCover)
        {
            glm::vec3 coverDir = NpcNavigation::findCoverDirection(npc, npc.sensors.targetPos, world);
            if (glm::length(coverDir) > 0.001f)
            {
                float coverBlend = 0.5f;
                moveDir = glm::normalize(moveDir + coverDir * coverBlend);
            }
        }

        // A local correction temporarily steers around an obstacle while the
        // navigator keeps the same long-range commitment. Return to the main
        // direction as soon as it is open again; never turn the correction into
        // a new patrol goal.
        if (npc.navigator.localCorrectionActive)
        {
            const glm::vec3 correction = npc.navigator.localCorrectionDirection;
            const bool correctionBlocked = NpcNavigation::obstacleInDirection(
                npc, correction, npcDifficulty.wallCastDistance, world, nearCandidates);
            const bool commitmentOpen = npc.navigator.commitmentActive &&
                glm::length(npc.navigator.committedDirection) > 0.001f &&
                !NpcNavigation::obstacleInDirection(
                    npc, npc.navigator.committedDirection,
                    npcDifficulty.wallCastDistance, world, nearCandidates);
            if (commitmentOpen || correctionBlocked)
            {
                npc.navigator.clearLocalCorrection();
            }
            else
            {
                moveDir = correction;
            }
        }
    }

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

    // No-progress recovery can override the tactical direction for a short
    // window so the actor physically leaves the spot it is stuck on.
    if (!inMirrorPhase && npc.stateMachine.patrolForcedDetourTicks > 0)
    {
        moveDir = npc.stateMachine.patrolForcedDetourDir;
        --npc.stateMachine.patrolForcedDetourTicks;
    }

    {
        Perf::ScopedTimer _pathTimer("NpcPathfinding");
        if (glm::length(moveDir) > 0.001f)
        {
            const auto& navCfg = NpcDifficultyConfig::instance().settings();
            const glm::vec3 requestedDir = moveDir;
            const bool requestedBlocked = navCfg.wallAvoidanceEnabled &&
                NpcNavigation::obstacleInDirection(
                    npc, requestedDir, navCfg.wallCastDistance, world, nearCandidates);
            const glm::vec3 steeringBefore = moveDir;
            moveDir = NpcNavigation::wallAvoidDirection(npc, moveDir, world, nearCandidates);

            if (requestedBlocked)
            {
                // Log wall avoidance only when it actually changed the requested
                // direction and only when the adjusted direction is new, so a
                // sustained wall does not produce a record every tick.
                const glm::vec3 adjusted(moveDir.x, moveDir.y, 0.0f);
                const bool changedByAvoid =
                    glm::length(glm::vec3(moveDir.x - steeringBefore.x,
                                          moveDir.y - steeringBefore.y, 0.0f)) > 0.05f;
                if (changedByAvoid && glm::length(adjusted) > 0.001f) {
                    const glm::vec3 adjustedN = glm::normalize(adjusted);
                    const bool newAvoidDir = glm::length(npc.lastWallAvoidDir) < 0.001f ||
                        glm::dot(adjustedN, npc.lastWallAvoidDir) < 0.99f;
                    npc.lastWallAvoidDir = adjustedN;
                    npc.navigator.startLocalCorrection(adjustedN, 1.0f);
                    if (newAvoidDir) {
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::NpcMovement, StructuredLevel::Important,
                            "npc.wall-avoid", std::to_string(npc.id), "wall_ahead",
                            (uint32_t)(npc.sensors.time * 60.0f),
                            nlohmann::json{
                                {"actor", npc.id},
                                {"team", npc.body.matchTeam},
                                {"profile", npc.behaviorProfileId},
                                {"preset", npc.actorPresetId},
                                {"state", npcStateName(npc.stateMachine.currentState)},
                                {"requested_dir", {requestedDir.x, requestedDir.y}},
                                {"adjusted_dir", {adjustedN.x, adjustedN.y}},
                                {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}}},
                            __FILE__, __LINE__, __FUNCTION__);
                    }
                }

                const bool stillBlocked = NpcNavigation::obstacleInDirection(
                    npc, moveDir, navCfg.wallCastDistance, world, nearCandidates);
                // Even after the local turn the goal direction is still walled
                // off. Back away and repath. This escape BYPASSES the actor
                // policy on purpose: a policy actor (e.g. Counter-Strike) that
                // spawns in a wall pocket must still be able to leave it. The
                // policy's blocked_behavior still shapes the steering above
                // (turn vs hold); only this physical escape is unconditional.
                if (stillBlocked && navCfg.wallBacktrackEnabled)
                {
                    // Emit the diagnostic once per backtrack episode, not once
                    // per tick: while the actor cannot make progress it re-asks
                    // for the same escape every tick, and an unthrottled event
                    // would flood events.jsonl. A new episode begins only after
                    // the previous backtrack finished.
                    const bool newBacktrackEpisode = !npc.navigator.backtrackActive;
                    npc.navigator.startBacktrack(
                        requestedDir,
                        navCfg.wallBacktrackDistance,
                        navCfg.wallBacktrackDuration);
                    moveDir = -glm::normalize(glm::vec3(
                        requestedDir.x, requestedDir.y, 0.0f));
                    // Canonical append-only diagnostic: read
                    // logs/<date>/<run>/events.jsonl while the exe runs.
                    if (newBacktrackEpisode)
                    {
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::NpcMovement, StructuredLevel::Important,
                            "npc.wall-escape", std::to_string(npc.id), "backtrack",
                            (uint32_t)(npc.sensors.time * 60.0f),
                            nlohmann::json{
                                {"actor", npc.id},
                                {"preset", npc.actorPresetId},
                                {"team", npc.body.matchTeam},
                                {"policy", policy ? policy->blockedBehavior : "none"},
                                {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}},
                                {"blocked_dir", {requestedDir.x, requestedDir.y}}},
                            __FILE__, __LINE__, __FUNCTION__);
                    }
                }
            }
            else
            {
                npc.lastWallAvoidDir = glm::vec3(0.0f);
            }
        }

        const bool stuckNow = NpcNavigation::isStuck(npc);
        if (stuckNow)
        {
            if (!npc.wasStuck)
            {
                // Rising edge: the actor is grounded, trying to move, and not
                // progressing. One record per stuck episode.
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::NpcMovement, StructuredLevel::Important,
                    "npc.stuck", std::to_string(npc.id), "stuck_detected",
                    (uint32_t)(npc.sensors.time * 60.0f),
                    nlohmann::json{
                        {"actor", npc.id},
                        {"team", npc.body.matchTeam},
                        {"profile", npc.behaviorProfileId},
                        {"preset", npc.actorPresetId},
                        {"state", npcStateName(npc.stateMachine.currentState)},
                        {"move_input", {npc.lastMoveInput.x, npc.lastMoveInput.y}},
                        {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            npc.stateMachine.stuckTimer += safeDt;
            if (npc.stateMachine.stuckTimer > 0.3f &&
                !npc.navigator.localCorrectionActive)
            {
                // One shared recovery for every actor (policy or legacy): choose
                // the most open local direction first, then optionally jump
                // (still gated by the policy below), then request a fresh route.
                // The actor never keeps pushing the blocked direction.
                const glm::vec3 openDir = NpcNavigation::unstuckDirection(
                    npc, npc.rngState, world, nearCandidates);
                if (glm::length(openDir) > 0.1f)
                {
                    moveDir = openDir;
                    npc.navigator.startLocalCorrection(openDir, 1.0f);
                }
                if (npc.sensors.touchFloor && glm::length(moveDir) > 0.1f) {
                    jump = true;
                    jumpReason = NpcJumpReason::Obstacle;
                }
                if (!policy)
                    dash = npc.dashCooldown <= 0.0f;
                npc.stateMachine.nextDecisionTime = std::min(npc.stateMachine.nextDecisionTime, 0.3f);
                std::string key = "npc-nav-stuck-" + std::to_string(npc.id);
                Debug::logThrottled(Debug::Category::NpcMovement, key.c_str(), 1.0f,
                    "[NPC NAV] actor=%u stuck=1 recovery=open_turn_repath\n", npc.id);
                // Canonical append-only diagnostic, emitted once per stuck
                // episode (the first tick the timer crosses the recovery
                // threshold). Read logs/<date>/<run>/events.jsonl live.
                if (npc.stateMachine.stuckTimer - safeDt <= 0.3f)
                {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::NpcMovement, StructuredLevel::Important,
                        "npc.stuck-recovery", std::to_string(npc.id), "open_turn_repath",
                        (uint32_t)(npc.sensors.time * 60.0f),
                        nlohmann::json{
                            {"actor", npc.id},
                            {"preset", npc.actorPresetId},
                            {"team", npc.body.matchTeam},
                            {"policy", policy ? policy->blockedBehavior : "none"},
                            {"open_dir", {openDir.x, openDir.y}},
                            {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}},
                            {"stuck_seconds", npc.stateMachine.stuckTimer}},
                        __FILE__, __LINE__, __FUNCTION__);
                }
            }
        }
        else
        {
            npc.stateMachine.stuckTimer = 0.0f;
        }
        npc.wasStuck = stuckNow;
    }

    // A policy jump always needs an explicit, allowed reason. This also catches
    // any jump set outside the sources above (e.g. mirror/bomb-tag) so a policy
    // actor cannot jump "just because".
    if (jump && policy && !npcPolicyAllowsJump(*policy, jumpReason))
        jump = false;

    // Jump decision edge: log when the resolved (and permitted) jump reason
    // changes; jumpReason resets to None each tick, so a later identical jump
    // re-emits. Never per tick while the same reason persists.
    {
        const int reasonNow = (int)jumpReason;
        if (reasonNow != npc.lastJumpReason) {
            if (jump && reasonNow != (int)NpcJumpReason::None) {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::NpcMovement, StructuredLevel::Important,
                    "npc.jump", std::to_string(npc.id), jumpReasonName(jumpReason),
                    (uint32_t)(npc.sensors.time * 60.0f),
                    nlohmann::json{
                        {"actor", npc.id},
                        {"team", npc.body.matchTeam},
                        {"profile", npc.behaviorProfileId},
                        {"jump_reason", jumpReasonName(jumpReason)},
                        {"state", npcStateName(npc.stateMachine.currentState)},
                        {"on_ground", npc.sensors.touchFloor},
                        {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            npc.lastJumpReason = reasonNow;
        }
    }

    // Freeze: occasionally freeze to dodge shots / break prediction
    // (skipped when mirror phase provides its own freeze input)
    bool freeze = false;
    if (!inMirrorPhase)
    {
        if (npc.sensors.hasTarget && npc.sensors.touchFloor && npc.dashCooldown <= 0.0f
            && npc.body.freeze.freezeAvailable && npc.body.freeze.freezeTimer <= 0.0f)
        {
            const auto& cfg = NpcDifficultyConfig::instance().settings();
            freeze = random01(npc.rngState) < (0.003f * cfg.freezeChance);
        }
    }
    else if (mirrorCfg.mirrorFreezeEnabled)
    {
        // Mirror phase: check if the historical event had a freeze
        float cycleDuration = mirrorCfg.mirrorNormalDuration + mirrorCfg.mirrorReplayDuration;
        float phaseTime = std::fmod(npc.mirrorCycleTimer, cycleDuration);
        float mirrorPhaseTime = phaseTime - mirrorCfg.mirrorNormalDuration;
        float targetTime = currentTime - mirrorCfg.mirrorHistorySeconds + mirrorPhaseTime;
        const PlayerMovementEvent* evt = findPlayerEvent(targetTime);
        if (evt)
            freeze = evt->froze;
    }

    // Mirror phase: override wantDownDash from player history
    if (inMirrorPhase)
    {
        float cycleDuration = mirrorCfg.mirrorNormalDuration + mirrorCfg.mirrorReplayDuration;
        float phaseTime = std::fmod(npc.mirrorCycleTimer, cycleDuration);
        float mirrorPhaseTime = phaseTime - mirrorCfg.mirrorNormalDuration;
        float targetTime = currentTime - mirrorCfg.mirrorHistorySeconds + mirrorPhaseTime;
        const PlayerMovementEvent* evt = findPlayerEvent(targetTime);
        if (evt && mirrorCfg.mirrorDownDashEnabled)
            wantDownDash = evt->downDashed;
        else
            wantDownDash = false;
    }

    InputState input = buildInputState(npc, moveDir, jump, dash, attack, wantDownDash, safeDt);
    input.freezeHeld = freeze;
    input.groundReturnPressed = false;
    if (input.dashPressed)
        npc.dashCommandConsumed = true;

    bool downDashAvailableBefore = npc.body.dash.downDashAvailable;
    {
        Perf::ScopedTimer _npcCollision("NpcCollision");
        char entityLabel[32];
        std::snprintf(entityLabel, sizeof(entityLabel), "NPC_%u", npc.id);
        setCollisionEntityContext(entityLabel, npc.id, true);

        glm::vec3 velocityBefore = npc.body.vel;
        float planarSpeedBefore = glm::length(glm::vec2(velocityBefore.x, velocityBefore.y));

        // navMovement (resolved above) carries the actor's role movement config
        // through the same shared kernel as the human actor.
        physicsMainUpdate(npc.body, world, input, safeDt, 2, navMovement);

        clearCollisionEntityContext();

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
                "[NPC] id=%u state=%s jump=%d dash=%d freeze=%d move=(%.2f %.2f)\n",
                npc.id, npcStateName(npc.stateMachine.currentState).c_str(),
                (int)input.jumpHeld, (int)input.dashPressed, (int)input.freezeHeld,
                input.wishMoveXY.x, input.wishMoveXY.y);

            std::string physKey = "npc-phys-" + std::to_string(npc.id);
            Debug::logThrottled(Debug::Category::General, physKey.c_str(), DebugConfig::PRINT_INTERVAL,
            "[NPC PHYS] id=%u floor=%d vel=(%.2f %.2f %.2f) finalSpeed=%.2f\n",
            npc.id, (int)npc.sensors.touchFloor,
            npc.body.vel.x, npc.body.vel.y, npc.body.vel.z,
            npc.lastFinalSpeed);

            if (NpcDifficultyConfig::instance().settings().mirrorMovementEnabled)
            {
                std::string mirrorKey = "npc-mirror-" + std::to_string(npc.id);
                Debug::logThrottled(Debug::Category::NpcMovement, mirrorKey.c_str(), DebugConfig::PRINT_INTERVAL,
                    "[NPC MIRROR] id=%u phase=%s timer=%.2f history=%d move=(%.2f %.2f) jump=%d dash=%d freeze=%d\n",
                    npc.id,
                    npc.mirrorPhaseActive ? "MIRROR" : "NORMAL",
                    npc.mirrorCycleTimer,
                    playerHistoryCount,
                    input.wishMoveXY.x, input.wishMoveXY.y,
                    (int)input.jumpHeld, (int)input.dashPressed, (int)input.freezeHeld);
            }
        }
    }

    if (input.dashPressed && npc.body.dash.didDash)
    {
        npc.dashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.62f;
        EffectPartSystem::instance().spawnDash(npc.body.pos);
        playWorldSound("entity/player/dash", npc.body.pos, 1.0f, 1.0f, 36.0f);
    }

    if (wantDownDash && downDashAvailableBefore && !npc.body.dash.downDashAvailable)
    {
        npc.downDashCooldown = 0.80f - difficulty01(npc.difficulty) * 0.50f;
    }

    // Reaction delay: after acquiring a target, wait before the first shot.
    // Uses the behavior profile when it sets one; otherwise falls back to the
    // perception reaction delay (default 8 ticks) so every NPC reacts like a
    // human rather than firing the instant a target becomes visible.
    if (npc.sensors.hasTarget && !npc.prevHadTarget)
    {
        const float perceptionDelay = (float)NpcDifficultyConfig::instance()
            .settings().perceptionReactionTicks / 60.0f;
        npc.reactionTimer = npc.behavior.reactionDelay > 0.0f
            ? npc.behavior.reactionDelay : perceptionDelay;
        if (npc.reactionTimer > 0.0f)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcCombat, StructuredLevel::Important,
                "npc.reaction", std::to_string(npc.id), "reaction_delay",
                (uint32_t)(npc.sensors.time * 60.0f),
                nlohmann::json{
                    {"actor", npc.id},
                    {"team", npc.body.matchTeam},
                    {"profile", npc.behaviorProfileId},
                    {"delay_seconds", npc.reactionTimer},
                    {"target_id", npc.serverTargetId},
                    {"target_distance", npc.sensors.targetDistance},
                    {"pos", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}}},
                __FILE__, __LINE__, __FUNCTION__);
            std::string reactKey = "npc-react-" + std::to_string(npc.id);
            Debug::logThrottled(Debug::Category::NpcCombat, reactKey.c_str(),
                DebugConfig::PRINT_INTERVAL,
                "[NPC REACTION] npc=%u delay=%.2f\n", npc.id, npc.reactionTimer);
        }
    }
    else if (!npc.sensors.hasTarget)
    {
        npc.reactionTimer = 0.0f;
    }
    else
    {
        npc.reactionTimer = std::max(0.0f, npc.reactionTimer - safeDt);
    }
    npc.prevHadTarget = npc.sensors.hasTarget;

    // Defensive team gate: never fire at a same-team target, even if target
    // selection upstream ever misfires. Friendly splash damage is unaffected
    // (this only gates direct fire). Teams < 0 mean "no team" (sandbox/FFA).
    const bool sameTeamTarget =
        npc.body.matchTeam >= 0 && player.matchTeam >= 0 &&
        npc.body.matchTeam == player.matchTeam;

    if (attack && !sameTeamTarget &&
        npc.attackCooldown <= 0.0f && npc.reactionTimer <= 0.0f)
    {
        Debug::log(Debug::Category::NpcCombat,
            "[NPC FIRE] npc=%u timeSinceLastShot=%.3f\n",
            npc.id, npc.timeSinceLastShot);
        Perf::ScopedTimer _combatTimer("NpcCombat");
        bool fired = NpcCombat::tryFire(npc, world, player, safeDt);
        npc.justFired = fired;
        if (fired)
        {
            npc.timeSinceLastShot = 0.0f;
            // Do NOT reset nextDecisionTime — let the state machine keep its
            // current state for its minimum duration to prevent jitter.
        }
    }

    // Final movement snapshot after all overrides, physics, and combat: one
    // complete record per NPC per second (never per tick). This is the record
    // that explains wanted vs final direction and actual distance moved.
    emitMovementDecision(npc, replanReasonThisTick, jumpReason, safeDt);
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

    // Toggled from config/npc-difficulty.json (npcDebugVisuals), hot-reloaded.
    if (!DebugVis::masterEnabled() ||
        !NpcDifficultyConfig::instance().settings().npcDebugVisuals)
        return;

    for (const Npc& npc : npcs)
    {
        glm::vec3 eye = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.8f);

        // Line of sight to player target
        glm::vec3 playerEye = npc.sensors.targetPos + glm::vec3(0.0f, 0.0f, 0.8f);
        DebugVis::drawLine(camera, eye, playerEye, glm::vec4(0.0f, 1.0f, 0.0f, 0.3f));

        // Perfect aim direction (yellow)
        glm::vec3 toTarget = playerEye - eye;
        float tLen = glm::length(toTarget);
        if (tLen > 0.1f)
        {
            glm::vec3 idealDir = toTarget / tLen;
            DebugVis::drawLine(camera, eye, eye + idealDir * 10.0f, glm::vec4(1.0f, 1.0f, 0.0f, 0.6f));
        }

        // Where the gun/model is actually pointing (white) — compare to the
        // yellow aim line to see any "shoots to the side" mismatch.
        glm::vec3 facingDir = glm::length(npc.currentFacing) > 0.001f
            ? glm::normalize(npc.currentFacing) : glm::vec3(1.0f, 0.0f, 0.0f);
        DebugVis::drawLine(camera, eye, eye + facingDir * 6.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.7f));

        // Target point (red sphere)
        DebugVis::drawWireSphere(camera, playerEye, 0.1f, glm::vec4(1.0f, 0.0f, 0.0f, 0.9f));

        // Accuracy info label
        float maxErr = NpcCombat::aimErrorDegrees(npc.difficulty);
        float dist = glm::length(npc.sensors.toTarget);
        glm::vec3 planarFacing = glm::normalize(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f));
        glm::vec3 planarAim = tLen > 0.1f
            ? glm::normalize(glm::vec3(toTarget.x, toTarget.y, 0.0f)) : glm::vec3(0.0f);
        float facingAim = (glm::length(planarFacing) > 0.001f && glm::length(planarAim) > 0.001f)
            ? glm::degrees(std::acos(std::clamp(glm::dot(planarFacing, planarAim), -1.0f, 1.0f)))
            : 0.0f;
        char label[256];
        int n = snprintf(label, sizeof(label), "NPC %u maxErr=%.1fdeg dist=%.1fm facingAim=%.1fdeg",
            npc.id, maxErr, dist, facingAim);
        const WeaponDefinition* wDef = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
        if (wDef)
        {
            auto it = npc.body.weaponRuntimes.find(wDef->id);
            if (it != npc.body.weaponRuntimes.end())
            {
                snprintf(label + n, sizeof(label) - n,
                    " ammo=%d%s cd=%.3f",
                    it->second.currentAmmo,
                    it->second.isReloading ? " RELOAD" : "",
                    npc.attackCooldown);
            }
        }
        DebugVis::drawWorldLabel(eye + glm::vec3(0.0f, 0.0f, 0.5f), label,
            glm::vec4(0.0f, 1.0f, 1.0f, 0.9f));
    }
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
        info.onFloor = npc.sensors.touchFloor;
        info.hasTarget = npc.sensors.hasTarget;
        out.push_back(info);
    }

    return out;
}
