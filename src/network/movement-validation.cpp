// 07 21 2026, 16 45
/* purpose
* Cold bridge for client-trusting movement report validation. The accept/
* correct/reject policy lives in the shared header
* `hot-reload/hot-movement-validation.h` (one source for the cold EXE fallback
* and the hot provider). This file projects the ServerPlayer + report + config
* into the POD request, precomputes the world sweep the policy cannot do, and
* applies the returned decision.
* Provides bounded diagnostic counters and reset bridges for authoritative server movement state.
* Does NOT send packets, poll sockets, render, play audio, or own transport decisions.
* Does NOT simulate full server-derived movement, rewind prediction, or input acknowledgements.
* Does NOT validate or trust damage, health, ammo, projectile hits, or weapon outcomes.
*/

#include "network/movement-validation.h"

#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-movement-validation.h"
#include "network/server.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace MimitaNet {
namespace {

bool hasFlag(uint32_t flags, MovementReportFlags flag)
{
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

float safeSizeScale(float sizeScale)
{
    if (!std::isfinite(sizeScale) || sizeScale <= 0.0f)
        return 1.0f;
    return std::clamp(sizeScale, 0.1f, 10.0f);
}

bool rayTriangle(const glm::vec3& origin,
                 const glm::vec3& direction,
                 const CollisionTriangle& tri,
                 float maxDistance,
                 float& outDistance)
{
    glm::vec3 e1 = tri.b - tri.a;
    glm::vec3 e2 = tri.c - tri.a;
    glm::vec3 p = glm::cross(direction, e2);
    float det = glm::dot(e1, p);
    if (std::abs(det) < 0.000001f)
        return false;

    float inv = 1.0f / det;
    glm::vec3 t = origin - tri.a;
    float u = glm::dot(t, p) * inv;
    if (u < 0.0f || u > 1.0f)
        return false;
    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f)
        return false;
    outDistance = glm::dot(e2, q) * inv;
    return outDistance >= 0.0f && outDistance <= maxDistance;
}

bool crossesBlockingGeometry(const HeadlessWorld* world,
                             glm::vec3 from,
                             glm::vec3 to,
                             float tolerance)
{
    if (!world || world->triangles.empty())
        return false;

    const glm::vec3 delta = to - from;
    const float distance = glm::length(delta);
    if (distance <= 0.0001f)
        return false;

    const glm::vec3 direction = delta / distance;
    const float maxDistance = distance + tolerance;

    for (const CollisionTriangle& tri : world->triangles)
    {
        if (std::abs(tri.normal.z) > 0.85f)
            continue;
        float hitDistance = 0.0f;
        if (rayTriangle(from, direction, tri, maxDistance, hitDistance))
            return true;
    }

    return false;
}

MovementState stateFromAcceptedReport(const ServerPlayer& player,
                                      const ClientMovementReport& report,
                                      const MovementValidationConfig& config)
{
    MovementState state = player.movement;
    state.lifecycle = report.lifecycle;
    state.movementEnabled = true;
    state.position = report.position;
    state.baseVelocity = report.baseVelocity;
    state.externalImpulse = report.externalImpulse;
    state.lastInputMoveAxes = movementClampUnitOrZero(report.moveAxes);
    state.yaw = report.yaw;
    state.sizeScale = safeSizeScale(report.sizeScale);

    state.ground.onGround = hasFlag(report.movementFlags, MOVEMENT_REPORT_ON_GROUND);
    state.ground.stableOnGround =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_STABLE_ON_GROUND);
    state.ground.hasWorldContact =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_HAS_WORLD_CONTACT);
    state.ground.realWorldContactThisFrame =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_REAL_WORLD_CONTACT);
    if (state.ground.onGround || state.ground.stableOnGround)
        state.ground.groundNormal = glm::vec3(0.0f, 0.0f, 1.0f);

    state.jump.airJumpArmed =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_AIR_JUMP_ARMED);
    state.jump.airJumpLocked =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_AIR_JUMP_LOCKED);
    if (state.jump.airJumpArmed && !state.jump.airJumpLocked)
        state.jump.airJumpsLeft = config.maximumAirJumps;
    else if (!state.jump.airJumpArmed)
        state.jump.airJumpsLeft = 0;
    state.jump.jumpHeldPreviously =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_JUMP_HELD);

    state.dash.dashAvailable =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_DASH_AVAILABLE);
    state.dash.didDash = report.dashSerial != 0 &&
        report.dashSerial != player.lastPresentationDashSerial;
    state.dashMomentumProtection.active =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_DASH_PROTECTED);
    state.dashMomentumProtection.protectedMoveAxes =
        movementClampUnitOrZero(report.moveAxes);

    state.downDash.available =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_DOWN_DASH_AVAILABLE);
    state.downDash.didDownDash = report.downDashSerial != 0 &&
        report.downDashSerial != player.lastPresentationDownDashSerial;

    state.freeze.active =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_FREEZE_ACTIVE);
    state.freeze.available =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_FREEZE_AVAILABLE);
    state.freeze.heldPreviously =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_FREEZE_HELD);
    state.freeze.didFreeze = report.freezeSerial != 0 &&
        report.freezeSerial != player.lastPresentationFreezeSerial;

    state.groundReturn.available =
        hasFlag(report.movementFlags, MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE);

    return state;
}

} // namespace

MovementValidationConfig makeMovementValidationConfig(
    const MovementConfig& movementConfig,
    const HeadlessWorld* world)
{
    MovementValidationConfig config;
    config.maximumBaseHorizontalSpeed =
        std::max(180.0f, movementConfig.groundSpeed +
                            movementConfig.groundDashImpulse +
                            movementConfig.maximumExternalImpulseSpeed +
                            movementConfig.bunnyHopSpeedCap);
    config.maximumBaseUpwardSpeed =
        std::max(180.0f, movementConfig.jumpVerticalSpeed + 120.0f);
    config.maximumBaseDownwardSpeed =
        std::max(450.0f, movementConfig.maximumFallSpeed + 50.0f);
    config.maximumExternalHorizontalImpulse =
        std::max(260.0f, movementConfig.maximumExternalImpulseSpeed + 140.0f);
    config.maximumExternalVerticalImpulse =
        std::max(260.0f, movementConfig.maximumExternalImpulseSpeed + 140.0f);
    config.dashDisplacementAllowance =
        std::max(12.0f, movementConfig.groundDashImpulse * 0.12f);
    config.downDashDisplacementAllowance =
        std::max(18.0f, std::abs(movementConfig.downDashVerticalSpeed) * 0.18f);
    config.maximumAirJumps = movementConfig.maximumAirJumps;
    if (world && movementIsFinite(world->boundsMin) && movementIsFinite(world->boundsMax) &&
        glm::length(world->boundsMax - world->boundsMin) > 0.001f)
    {
        // Keep the map's XY bounds for anti-glitch protection, but leave the
        // Z floor far below any void-death threshold so players falling off a
        // map can reach the void instead of being clamped back above it.
        config.worldBoundsMin = glm::vec3(world->boundsMin.x,
                                          world->boundsMin.y,
                                          -100000.0f);
        config.worldBoundsMax = world->boundsMax;
    }
    return config;
}

bool movementReportSequenceIsNewer(uint32_t incoming, uint32_t previous)
{
    if (incoming == previous)
        return false;
    return static_cast<int32_t>(incoming - previous) > 0;
}

MovementCorrectionClass classifyMovementCorrection(
    float errorDistance,
    const MovementValidationConfig& config)
{
    if (!std::isfinite(errorDistance) || errorDistance <= 0.0f)
        return MovementCorrectionClass::None;
    if (errorDistance <= config.smallCorrectionDistance)
        return MovementCorrectionClass::Small;
    if (errorDistance < config.majorCorrectionDistance)
        return MovementCorrectionClass::Medium;
    return MovementCorrectionClass::Major;
}

const char* movementValidationDecisionName(MovementValidationDecision decision)
{
    switch (decision)
    {
    case MovementValidationDecision::Accept: return "accept";
    case MovementValidationDecision::Correct: return "correct";
    case MovementValidationDecision::Reject: return "reject";
    default: return "unknown";
    }
}

const char* movementValidationReasonName(MovementValidationReason reason)
{
    switch (reason)
    {
    case MovementValidationReason::None: return "none";
    case MovementValidationReason::UnknownPlayer: return "unknown-player";
    case MovementValidationReason::WrongOwner: return "wrong-owner";
    case MovementValidationReason::InactiveConnection: return "inactive-connection";
    case MovementValidationReason::NotSpawned: return "not-spawned";
    case MovementValidationReason::NotActive: return "not-active";
    case MovementValidationReason::Dead: return "dead";
    case MovementValidationReason::MovementDisabled: return "movement-disabled";
    case MovementValidationReason::SpawnGenerationMismatch: return "spawn-generation";
    case MovementValidationReason::TransformEpochMismatch: return "transform-epoch";
    case MovementValidationReason::DuplicateSequence: return "duplicate-sequence";
    case MovementValidationReason::OldSequence: return "old-sequence";
    case MovementValidationReason::StaleClientTick: return "stale-client-tick";
    case MovementValidationReason::FutureClientTick: return "future-client-tick";
    case MovementValidationReason::NonFinite: return "non-finite";
    case MovementValidationReason::MalformedState: return "malformed-state";
    case MovementValidationReason::ImpossibleDisplacement: return "impossible-displacement";
    case MovementValidationReason::ImpossibleVelocity: return "impossible-velocity";
    case MovementValidationReason::OutOfBounds: return "out-of-bounds";
    case MovementValidationReason::BlockingGeometry: return "blocking-geometry";
    case MovementValidationReason::AbilityTransition: return "ability-transition";
    case MovementValidationReason::AwaitingAuthoritativeTransformAck:
        return "awaiting-authoritative-transform-ack";
    case MovementValidationReason::TooFarFromAuthoritative:
        return "too-far-from-authoritative";
    default: return "unknown";
    }
}

const char* movementCorrectionClassName(MovementCorrectionClass correctionClass)
{
    switch (correctionClass)
    {
    case MovementCorrectionClass::None: return "none";
    case MovementCorrectionClass::Small: return "small";
    case MovementCorrectionClass::Medium: return "medium";
    case MovementCorrectionClass::Major: return "major";
    default: return "unknown";
    }
}

bool movementSnapshotLifecycleFresh(uint32_t incomingSpawnGeneration,
                                    uint32_t incomingTransformEpoch,
                                    uint32_t lastSpawnGeneration,
                                    uint32_t lastTransformEpoch)
{
    if (incomingSpawnGeneration != 0 && lastSpawnGeneration != 0)
    {
        if (incomingSpawnGeneration < lastSpawnGeneration)
            return false;
        if (incomingSpawnGeneration > lastSpawnGeneration)
            return true;
    }

    if (incomingTransformEpoch != 0 && lastTransformEpoch != 0)
    {
        if (incomingTransformEpoch < lastTransformEpoch)
            return false;
        if (incomingTransformEpoch > lastTransformEpoch)
            return true;
    }

    return true;
}

bool movementSnapshotIsFresh(uint32_t incomingServerTick,
                             uint32_t incomingSpawnGeneration,
                             uint32_t incomingTransformEpoch,
                             uint32_t lastServerTick,
                             uint32_t lastSpawnGeneration,
                             uint32_t lastTransformEpoch)
{
    if (!movementSnapshotLifecycleFresh(incomingSpawnGeneration,
                                        incomingTransformEpoch,
                                        lastSpawnGeneration,
                                        lastTransformEpoch))
        return false;

    if (incomingServerTick <= lastServerTick)
        return false;
    return true;
}

MovementValidationResult validateClientMovementReport(
    const ServerPlayer& player,
    const ClientMovementReport& report,
    const MovementValidationContext& context,
    const MovementValidationConfig& config)
{
    // The accepted state projection stays cold; the policy only decides whether
    // to accept it, correct it, or reject it.
    const MovementState acceptedState =
        stateFromAcceptedReport(player, report, config);

    GameMovementValidateV1 request{};
    request.structSize = sizeof(GameMovementValidateV1);
    request.playerExists = context.playerExists ? 1u : 0u;
    request.connectionActive = context.connectionActive ? 1u : 0u;
    request.connectionOwnsPlayer = context.connectionOwnsPlayer ? 1u : 0u;
    request.serverTick = context.serverTick;
    request.nowMs = context.nowMs;
    request.spawned = player.spawned ? 1u : 0u;
    request.spawnStateActive = player.spawnState == ServerPlayer::Active ? 1u : 0u;
    request.dead = player.dead ? 1u : 0u;
    request.movementEnabled = player.movement.movementEnabled ? 1u : 0u;
    request.spawnGeneration = player.spawnGeneration;
    request.transformEpoch = static_cast<std::uint32_t>(player.transformEpoch);
    request.hasMovementSequence = player.hasMovementSequence ? 1u : 0u;
    request.lastMovementSequence = player.lastMovementSequence;
    request.awaitingAuthoritativeTransformAck =
        player.awaitingAuthoritativeTransformAck ? 1u : 0u;
    request.authoritativeTransformEpoch =
        static_cast<std::uint32_t>(player.authoritativeTransformEpoch);
    request.hasAcceptedClientTransform = player.hasAcceptedClientTransform ? 1u : 0u;
    for (int i = 0; i < 3; ++i)
    {
        request.playerPos[i] = player.pos[i];
        request.lastAcceptedClientPosition[i] = player.lastAcceptedClientPosition[i];
        request.reportPosition[i] = report.position[i];
        request.reportBaseVelocity[i] = report.baseVelocity[i];
        request.reportExternalImpulse[i] = report.externalImpulse[i];
        request.reportCameraForward[i] = report.horizontalCameraForward[i];
        request.worldBoundsMin[i] = config.worldBoundsMin[i];
        request.worldBoundsMax[i] = config.worldBoundsMax[i];
    }
    request.lastAcceptedClientTick = player.movementValidation.lastAcceptedClientTick;
    request.reportSpawnGeneration = report.lifecycle.spawnGeneration;
    request.reportTransformEpoch = report.lifecycle.transformEpoch;
    request.reportMovementSequence = report.movementSequence;
    request.reportClientSimulationTick = report.clientSimulationTick;
    request.reportMoveAxes[0] = report.moveAxes.x;
    request.reportMoveAxes[1] = report.moveAxes.y;
    request.reportYaw = report.yaw;
    request.reportLookPitch = report.lookPitch;
    request.reportSizeScale = report.sizeScale;
    request.reportMovementFlags = report.movementFlags;
    request.worldBoundsPadding = config.worldBoundsPadding;
    request.postGapCorrectionMinTicks = config.postGapCorrectionMinTicks;
    request.postGapCorrectionDistance = config.postGapCorrectionDistance;
    request.acceptedStateFinite = movementIsFinite(acceptedState) ? 1u : 0u;

    const glm::vec3 previousPosition = player.hasAcceptedClientTransform
        ? player.lastAcceptedClientPosition
        : player.pos;
    request.crossesBlockingGeometry =
        crossesBlockingGeometry(context.world, previousPosition, report.position,
                                config.wallSweepTolerance) ? 1u : 0u;
    request.belowVoidFloor =
        (context.world && report.position.z < context.world->boundsMin.z) ? 1u : 0u;

    auto validateFn = reinterpret_cast<GameMovementValidateFn>(
        MimitaRuntime::GenericRuntime::instance().capability(
            GAME_CAP_MOVEMENT_VALIDATE));
    if (validateFn)
        validateFn(nullptr, &request);
    else
        HotMovementValidationImpl::validate(&request);

    MovementValidationResult result;
    if (!request.result)
        return result;

    result.decision = static_cast<MovementValidationDecision>(request.decision);
    result.reason = static_cast<MovementValidationReason>(request.reason);
    if (result.decision == MovementValidationDecision::Reject)
    {
        // Matches the previous reject() shape: no accepted state is applied.
        return result;
    }

    result.acceptedState = acceptedState;
    result.acceptedState.position = glm::vec3(request.acceptedPosition[0],
                                              request.acceptedPosition[1],
                                              request.acceptedPosition[2]);
    result.acceptedState.baseVelocity = glm::vec3(request.acceptedBaseVelocity[0],
                                                  request.acceptedBaseVelocity[1],
                                                  request.acceptedBaseVelocity[2]);
    result.acceptedState.externalImpulse =
        glm::vec3(request.acceptedExternalImpulse[0],
                  request.acceptedExternalImpulse[1],
                  request.acceptedExternalImpulse[2]);
    result.clearsAuthoritativeTransformAck =
        request.clearsAuthoritativeTransformAck != 0;
    result.metrics.positionError = request.positionError;
    return result;
}

void applyMovementValidationCounters(MovementValidationCounters& counters,
                                     const MovementValidationResult& result,
                                     const ClientMovementReport& report)
{
    counters.lastReason = result.reason;
    counters.maximumPositionError = std::max(
        counters.maximumPositionError, result.metrics.positionError);
    counters.maximumVelocityError = std::max(
        counters.maximumVelocityError, result.metrics.velocityError);

    if (result.decision == MovementValidationDecision::Accept)
        ++counters.acceptedReports;
    else if (result.decision == MovementValidationDecision::Correct)
        ++counters.correctedReports;
    else
        ++counters.rejectedReports;

    if (result.reason == MovementValidationReason::DuplicateSequence ||
        result.reason == MovementValidationReason::OldSequence)
        ++counters.duplicateReports;
    if (result.reason == MovementValidationReason::SpawnGenerationMismatch)
        ++counters.oldLifeReports;
    if (result.reason == MovementValidationReason::TransformEpochMismatch)
        ++counters.wrongEpochReports;
    if (result.reason == MovementValidationReason::NonFinite ||
        result.reason == MovementValidationReason::MalformedState)
        ++counters.nonFiniteReports;
    if (result.reason == MovementValidationReason::OutOfBounds)
        ++counters.boundsCorrections;
    if (result.reason == MovementValidationReason::BlockingGeometry)
        ++counters.wallCorrections;

    if (result.decision != MovementValidationDecision::Reject)
    {
        counters.lastAcceptedSequence = report.movementSequence;
        counters.lastAcceptedClientTick = report.clientSimulationTick;
    }
}

void resetServerMovementForAuthoritativeLifecycle(
    ServerPlayer& player,
    const MovementConfig& movementConfig)
{
    MovementState state;
    state.lifecycle = MovementLifecycleIdentity{
        player.spawnGeneration,
        static_cast<uint32_t>(player.transformEpoch)};
    state.movementEnabled =
        !player.dead && player.spawnState == ServerPlayer::Active;
    state.position = player.pos;
    state.baseVelocity = player.vel;
    state.externalImpulse = glm::vec3(0.0f);
    state.lastInputMoveAxes = movementClampUnitOrZero(player.input.wish);
    state.yaw = player.yaw;
    state.sizeScale = safeSizeScale(player.sizeScale);
    state.ground.onGround = player.onGround;
    state.ground.stableOnGround = player.onGround;
    state.ground.hasWorldContact = player.onGround;
    state.jump.airJumpsLeft = movementConfig.maximumAirJumps;
    state.jump.airJumpArmed = true;
    state.jump.airJumpLocked = false;
    state.dash.dashAvailable = true;
    state.dash.frictionOverride = 1.0f;
    state.downDash.available = true;
    state.freeze.available = true;
    state.groundReturn.available = true;
    state.downDash = MovementDownDashState{};
    state.freeze = MovementFreezeState{};
    state.groundReturn = MovementGroundReturnState{};
    state.dashMomentumProtection = MovementDashMomentumProtectionState{};
    state.jump.airJumpsLeft = movementConfig.maximumAirJumps;
    state.jump.airJumpArmed = true;
    state.dash.dashAvailable = true;
    state.downDash.available = true;
    state.freeze.available = true;
    state.groundReturn.available = true;
    state.contactHistory.resetForLifecycle(state.lifecycle);

    player.movement = state;
    player.hasMovementSequence = false;
    player.lastMovementSequence = 0;
    player.hasAcceptedClientTransform = false;
    player.movementValidation.lastAcceptedSequence = 0;
    player.movementValidation.lastAcceptedClientTick = 0;
    // Reset input command buffer (spec: no stale inputs across lives)
    player.lastInputCommandSequence = 0;
    player.lastProcessedInputCommandSequence = 0;
    for (auto& entry : player.inputCommandBuffer)
        entry.valid = false;
}

void recordServerMovementExternalImpulse(ServerPlayer& player,
                                         const glm::vec3& impulse)
{
    if (!movementIsFinite(impulse))
        return;
    player.movement.lifecycle = MovementLifecycleIdentity{
        player.spawnGeneration,
        static_cast<uint32_t>(player.transformEpoch)};
    player.movement.position = player.pos;
    if (!movementIsFinite(player.movement.externalImpulse))
        player.movement.externalImpulse = glm::vec3(0.0f);
    player.movement.externalImpulse += impulse;
    player.movement.baseVelocity = player.vel - player.movement.externalImpulse;
    player.movement.movementEnabled =
        !player.dead && player.spawnState == ServerPlayer::Active;
    player.movement.yaw = player.yaw;
    player.movement.sizeScale = safeSizeScale(player.sizeScale);
}

} // namespace MimitaNet
