// 07 21 2026, 16 45
/* purpose
* Implements Stage 3A client-trusting movement report validation.
* Applies shared movement vocabulary to server ownership, lifecycle, sequence, and bounds checks.
* Provides bounded diagnostic counters and reset bridges for authoritative server movement state.
* Does NOT send packets, poll sockets, render, play audio, or own transport decisions.
* Does NOT simulate full server-derived movement, rewind prediction, or input acknowledgements.
* Does NOT validate or trust damage, health, ammo, projectile hits, or weapon outcomes.
*/

#include "network/movement-validation.h"

#include "network/server.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace MimitaNet {
namespace {

constexpr float kTickSeconds = 1.0f / 60.0f;
constexpr float kFinitePositionLimit = 100000.0f;
constexpr float kFiniteVelocityLimit = 5000.0f;

bool hasFlag(uint32_t flags, MovementReportFlags flag)
{
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

float horizontalLength(glm::vec3 v)
{
    return glm::length(glm::vec2(v.x, v.y));
}

bool componentMagnitudeAllowed(glm::vec3 v, float maxAbs)
{
    return std::abs(v.x) <= maxAbs &&
           std::abs(v.y) <= maxAbs &&
           std::abs(v.z) <= maxAbs;
}

float safeSizeScale(float sizeScale)
{
    if (!std::isfinite(sizeScale) || sizeScale <= 0.0f)
        return 1.0f;
    return std::clamp(sizeScale, 0.1f, 10.0f);
}

float clientElapsedSeconds(const ServerPlayer& player,
                           const ClientMovementReport& report,
                           const MovementValidationContext& context)
{
    if (player.movementValidation.lastAcceptedClientTick != 0 &&
        report.clientSimulationTick > player.movementValidation.lastAcceptedClientTick)
    {
        const uint64_t deltaTicks =
            report.clientSimulationTick -
            player.movementValidation.lastAcceptedClientTick;
        return std::clamp(static_cast<float>(deltaTicks) * kTickSeconds,
                          kTickSeconds,
                          0.5f);
    }

    if (player.lastAcceptedClientTransformMs != 0 && context.nowMs != 0 &&
        context.nowMs >= player.lastAcceptedClientTransformMs)
    {
        return std::clamp(
            static_cast<float>(context.nowMs - player.lastAcceptedClientTransformMs) /
                1000.0f,
            1.0f / 240.0f,
            0.5f);
    }

    return kTickSeconds;
}

bool tickTooOld(const ServerPlayer& player,
                const ClientMovementReport& report,
                const MovementValidationContext& context,
                const MovementValidationConfig& config)
{
    if (context.serverTick == 0 || report.clientSimulationTick == 0)
        return false;

    const uint64_t serverTick = context.serverTick;
    const uint64_t clientTick = report.clientSimulationTick;
    return serverTick > clientTick &&
           serverTick - clientTick > config.maximumReportAgeTicks;
}

bool tickTooFuture(const ClientMovementReport& report,
                   const MovementValidationContext& context,
                   const MovementValidationConfig& config)
{
    if (context.serverTick == 0 || report.clientSimulationTick == 0)
        return false;
    return report.clientSimulationTick > context.serverTick &&
           report.clientSimulationTick - context.serverTick >
               config.maximumFutureTickLead;
}

glm::vec3 validationBoundsMin(const MovementValidationConfig& config)
{
    return config.worldBoundsMin - glm::vec3(config.worldBoundsPadding);
}

glm::vec3 validationBoundsMax(const MovementValidationConfig& config)
{
    return config.worldBoundsMax + glm::vec3(config.worldBoundsPadding);
}

bool insideBounds(glm::vec3 position, const MovementValidationConfig& config)
{
    const glm::vec3 minB = validationBoundsMin(config);
    const glm::vec3 maxB = validationBoundsMax(config);
    return position.x >= minB.x && position.y >= minB.y && position.z >= minB.z &&
           position.x <= maxB.x && position.y <= maxB.y && position.z <= maxB.z;
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

MovementValidationResult reject(MovementValidationReason reason)
{
    MovementValidationResult result;
    result.decision = MovementValidationDecision::Reject;
    result.reason = reason;
    return result;
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

bool hasContactResetEvidence(const ClientMovementReport& report)
{
    return hasFlag(report.movementFlags, MOVEMENT_REPORT_ON_GROUND) ||
           hasFlag(report.movementFlags, MOVEMENT_REPORT_STABLE_ON_GROUND) ||
           hasFlag(report.movementFlags, MOVEMENT_REPORT_HAS_WORLD_CONTACT) ||
           hasFlag(report.movementFlags, MOVEMENT_REPORT_REAL_WORLD_CONTACT);
}

bool invalidAbilityTransition(const ServerPlayer& player,
                              const ClientMovementReport& report)
{
    const MovementState& previous = player.movement;
    const bool contactReset = hasContactResetEvidence(report);

    const bool freshDash = report.dashSerial != 0 &&
        report.dashSerial != player.lastPresentationDashSerial;
    if (freshDash && !previous.dash.dashAvailable && !contactReset)
        return true;

    const bool freshDownDash = report.downDashSerial != 0 &&
        report.downDashSerial != player.lastPresentationDownDashSerial;
    if (freshDownDash && !previous.downDash.available && !contactReset)
        return true;

    const bool freshFreeze = report.freezeSerial != 0 &&
        report.freezeSerial != player.lastPresentationFreezeSerial;
    if (freshFreeze && !previous.freeze.available && !contactReset)
        return true;

    if (hasFlag(report.movementFlags, MOVEMENT_REPORT_DASH_AVAILABLE) &&
        !previous.dash.dashAvailable &&
        !contactReset)
        return true;
    if (hasFlag(report.movementFlags, MOVEMENT_REPORT_DOWN_DASH_AVAILABLE) &&
        !previous.downDash.available &&
        !contactReset)
        return true;
    if (hasFlag(report.movementFlags, MOVEMENT_REPORT_FREEZE_AVAILABLE) &&
        !previous.freeze.available &&
        !contactReset)
        return true;
    if (hasFlag(report.movementFlags, MOVEMENT_REPORT_AIR_JUMP_ARMED) &&
        !previous.jump.airJumpArmed &&
        !contactReset)
        return true;

    return false;
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
                            movementConfig.maximumExternalImpulseSpeed);
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
        config.worldBoundsMin = world->boundsMin;
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
    if (errorDistance <= config.mediumCorrectionDistance)
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

bool movementSnapshotIsFresh(uint32_t incomingServerTick,
                             uint32_t incomingSpawnGeneration,
                             uint32_t incomingTransformEpoch,
                             uint32_t lastServerTick,
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
    if (!context.playerExists)
        return reject(MovementValidationReason::UnknownPlayer);
    if (!context.connectionActive)
        return reject(MovementValidationReason::InactiveConnection);
    if (!context.connectionOwnsPlayer)
        return reject(MovementValidationReason::WrongOwner);
    if (!player.spawned)
        return reject(MovementValidationReason::NotSpawned);
    if (player.spawnState != ServerPlayer::Active)
        return reject(MovementValidationReason::NotActive);
    if (player.dead)
        return reject(MovementValidationReason::Dead);
    if (!player.movement.movementEnabled)
        return reject(MovementValidationReason::MovementDisabled);
    if (report.lifecycle.spawnGeneration != player.spawnGeneration)
        return reject(MovementValidationReason::SpawnGenerationMismatch);
    if (report.lifecycle.transformEpoch != player.transformEpoch)
        return reject(MovementValidationReason::TransformEpochMismatch);
    if (player.hasMovementSequence)
    {
        if (report.movementSequence == player.lastMovementSequence)
            return reject(MovementValidationReason::DuplicateSequence);
        if (!movementReportSequenceIsNewer(report.movementSequence,
                                           player.lastMovementSequence))
            return reject(MovementValidationReason::OldSequence);
    }
    if (tickTooOld(player, report, context, config))
        return reject(MovementValidationReason::StaleClientTick);
    if (tickTooFuture(report, context, config))
        return reject(MovementValidationReason::FutureClientTick);

    MovementValidationResult result;
    result.acceptedState = stateFromAcceptedReport(player, report, config);

    const bool finiteState =
        movementIsFinite(report.moveAxes) &&
        movementIsFinite(report.horizontalCameraForward) &&
        movementIsFinite(report.position) &&
        movementIsFinite(report.baseVelocity) &&
        movementIsFinite(report.externalImpulse) &&
        movementIsFinite(report.yaw) &&
        movementIsFinite(report.lookPitch) &&
        movementIsFinite(report.sizeScale) &&
        movementIsFinite(result.acceptedState);
    if (!finiteState)
        return reject(MovementValidationReason::NonFinite);
    if (!componentMagnitudeAllowed(report.position, kFinitePositionLimit) ||
        !componentMagnitudeAllowed(report.baseVelocity, kFiniteVelocityLimit) ||
        !componentMagnitudeAllowed(report.externalImpulse, kFiniteVelocityLimit) ||
        report.sizeScale <= 0.0f)
    {
        return reject(MovementValidationReason::MalformedState);
    }

    if (player.awaitingAuthoritativeTransformAck)
    {
        const float distanceFromAuthoritative =
            glm::length(report.position - player.authoritativeTransformPosition);
        if (report.lifecycle.transformEpoch != player.authoritativeTransformEpoch)
            return reject(MovementValidationReason::TransformEpochMismatch);
        if (distanceFromAuthoritative > config.authoritativeAckDistance)
        {
            MovementValidationResult ackReject =
                reject(MovementValidationReason::TooFarFromAuthoritative);
            ackReject.metrics.positionError = distanceFromAuthoritative;
            return ackReject;
        }
        result.clearsAuthoritativeTransformAck = true;
    }

    const float baseHorizontal = horizontalLength(report.baseVelocity);
    const float baseUp = std::max(0.0f, report.baseVelocity.z);
    const float baseDown = std::max(0.0f, -report.baseVelocity.z);
    const float externalHorizontal = horizontalLength(report.externalImpulse);
    const float externalVertical = std::abs(report.externalImpulse.z);
    const glm::vec3 combinedVelocity =
        report.baseVelocity + report.externalImpulse;
    const float combinedSpeed = glm::length(combinedVelocity);

    result.metrics.horizontalBaseSpeed = baseHorizontal;
    result.metrics.upwardBaseSpeed = baseUp;
    result.metrics.downwardBaseSpeed = baseDown;
    result.metrics.horizontalExternalImpulse = externalHorizontal;
    result.metrics.verticalExternalImpulse = externalVertical;
    result.metrics.combinedSpeed = combinedSpeed;

    if (baseHorizontal > config.maximumBaseHorizontalSpeed ||
        baseUp > config.maximumBaseUpwardSpeed ||
        baseDown > config.maximumBaseDownwardSpeed ||
        externalHorizontal > config.maximumExternalHorizontalImpulse ||
        externalVertical > config.maximumExternalVerticalImpulse ||
        combinedSpeed > config.maximumCombinedEmergencySpeed)
    {
        return reject(MovementValidationReason::ImpossibleVelocity);
    }

    if (invalidAbilityTransition(player, report))
        return reject(MovementValidationReason::AbilityTransition);

    const glm::vec3 previousPosition = player.hasAcceptedClientTransform
        ? player.lastAcceptedClientPosition
        : player.pos;
    const glm::vec3 previousVelocity = player.hasAcceptedClientTransform
        ? player.lastAcceptedClientVelocity
        : player.vel;
    const glm::vec3 delta = report.position - previousPosition;
    result.metrics.positionError = glm::length(report.position - player.pos);
    result.metrics.velocityError = glm::length(combinedVelocity - previousVelocity);
    result.metrics.horizontalDelta = glm::length(glm::vec2(delta.x, delta.y));
    result.metrics.verticalDelta = std::abs(delta.z);

    const float elapsedSeconds = clientElapsedSeconds(player, report, context);
    const float latencySeconds =
        std::clamp(static_cast<float>(std::max(0, report.clientPingMs)) / 1000.0f,
                   0.0f,
                   0.5f);
    const bool freshDash = report.dashSerial != 0 &&
        report.dashSerial != player.lastPresentationDashSerial;
    const bool freshDownDash = report.downDashSerial != 0 &&
        report.downDashSerial != player.lastPresentationDownDashSerial;

    result.metrics.allowedHorizontalDelta =
        config.ordinaryDisplacementTolerance +
        (std::max(baseHorizontal, horizontalLength(previousVelocity)) *
         elapsedSeconds) +
        externalHorizontal * config.externalImpulseDisplacementSeconds +
        config.latencyDisplacementAllowance * latencySeconds +
        config.collisionDepenetrationAllowance +
        (freshDash ? config.dashDisplacementAllowance : 0.0f);
    result.metrics.allowedVerticalDelta =
        config.ordinaryDisplacementTolerance +
        std::max(std::max(baseUp, baseDown), std::abs(previousVelocity.z)) *
            elapsedSeconds +
        externalVertical * config.externalImpulseDisplacementSeconds +
        config.latencyDisplacementAllowance * latencySeconds +
        config.collisionDepenetrationAllowance +
        (freshDownDash ? config.downDashDisplacementAllowance : 0.0f);

    if (result.metrics.horizontalDelta > result.metrics.allowedHorizontalDelta ||
        result.metrics.verticalDelta > result.metrics.allowedVerticalDelta)
    {
        return reject(MovementValidationReason::ImpossibleDisplacement);
    }

    if (!insideBounds(report.position, config))
    {
        result.decision = MovementValidationDecision::Correct;
        result.reason = MovementValidationReason::OutOfBounds;
        result.acceptedState.position = glm::clamp(
            report.position,
            validationBoundsMin(config),
            validationBoundsMax(config));
        return result;
    }

    if (crossesBlockingGeometry(context.world,
                                previousPosition,
                                report.position,
                                config.wallSweepTolerance))
    {
        result.decision = MovementValidationDecision::Correct;
        result.reason = MovementValidationReason::BlockingGeometry;
        result.acceptedState.position = previousPosition;
        result.acceptedState.baseVelocity = glm::vec3(0.0f);
        result.acceptedState.externalImpulse = glm::vec3(0.0f);
        result.metrics.positionError = glm::length(report.position - previousPosition);
        return result;
    }

    result.decision = MovementValidationDecision::Accept;
    result.reason = MovementValidationReason::None;
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
    player.movementValidation.lastAcceptedSequence = 0;
    player.movementValidation.lastAcceptedClientTick = 0;
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
