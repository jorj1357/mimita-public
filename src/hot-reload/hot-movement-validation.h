// 09 23 2026
/* purpose
* Define the generic, hot-replaceable server movement-report validation policy
* and the ONE implementation shared by the cold EXE fallback and the hot
* provider. The EXE owns the world sweep, the ServerPlayer projection, and the
* authoritative apply; a hot module owns the accept/correct/reject policy.
* POD only: no STL, sockets, ServerPlayer, or engine objects cross the boundary.
* Does NOT own the world, transport, or authoritative movement state.
*/
#pragma once

#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"
#include "network/movement-validation.h"

namespace MimitaNet {

// POD policy request. Cold fills the projections and precomputed world facts;
// the policy writes decision/reason and the accepted transform.
struct GameMovementValidateV1 {
    std::uint32_t structSize;

    // context
    std::uint32_t playerExists;
    std::uint32_t connectionActive;
    std::uint32_t connectionOwnsPlayer;
    std::uint32_t serverTick;
    std::uint64_t nowMs;

    // player projection
    std::uint32_t spawned;
    std::uint32_t spawnStateActive;
    std::uint32_t dead;
    std::uint32_t movementEnabled;
    std::uint32_t spawnGeneration;
    std::uint32_t transformEpoch;
    std::uint32_t hasMovementSequence;
    std::uint32_t lastMovementSequence;
    std::uint32_t awaitingAuthoritativeTransformAck;
    std::uint32_t authoritativeTransformEpoch;
    std::uint32_t hasAcceptedClientTransform;
    float playerPos[3];
    float lastAcceptedClientPosition[3];
    std::uint64_t lastAcceptedClientTick;

    // report projection
    std::uint32_t reportSpawnGeneration;
    std::uint32_t reportTransformEpoch;
    std::uint32_t reportMovementSequence;
    std::uint64_t reportClientSimulationTick;
    float reportPosition[3];
    float reportBaseVelocity[3];
    float reportExternalImpulse[3];
    float reportMoveAxes[2];
    float reportCameraForward[3];
    float reportYaw;
    float reportLookPitch;
    float reportSizeScale;
    std::uint32_t reportMovementFlags;

    // config projection
    float worldBoundsMin[3];
    float worldBoundsMax[3];
    float worldBoundsPadding;
    std::uint32_t postGapCorrectionMinTicks;
    float postGapCorrectionDistance;

    // cold-precomputed world facts (the policy never sees the world)
    std::uint32_t acceptedStateFinite;
    std::uint32_t crossesBlockingGeometry;
    std::uint32_t belowVoidFloor;

    // out
    std::uint32_t decision;   // MovementValidationDecision numeric
    std::uint32_t reason;     // MovementValidationReason numeric
    float acceptedPosition[3];
    float acceptedBaseVelocity[3];
    float acceptedExternalImpulse[3];
    std::uint32_t clearsAuthoritativeTransformAck;
    float positionError;
    std::uint32_t result;     // 1 = the policy produced a decision
    std::uint32_t reserved;
};

using GameMovementValidateFn = void (MIMITA_GAME_CALL *)(
    void* host, GameMovementValidateV1* request);

static constexpr std::uint64_t GAME_CAP_MOVEMENT_VALIDATE =
    gameHash("net.movement.validate");
static constexpr std::uint64_t GAME_SIG_MOVEMENT_VALIDATE =
    gameHash("sig.net.movement.validate.v1");

// ── The single shared implementation ────────────────────────────────
// Header-only so the SAME code serves the cold EXE fallback and the hot game
// DLL. This is exactly the previous cold validateClientMovementReport policy.
namespace HotMovementValidationImpl {

inline bool hasFlag(std::uint32_t flags, MovementReportFlags flag)
{
    return (flags & static_cast<std::uint32_t>(flag)) != 0;
}

inline bool finite3(const float v[3])
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

inline bool magnitudeAllowed3(const float v[3], float maxAbs)
{
    return std::abs(v[0]) <= maxAbs && std::abs(v[1]) <= maxAbs &&
           std::abs(v[2]) <= maxAbs;
}

inline bool sequenceIsNewer(std::uint32_t incoming, std::uint32_t previous)
{
    if (incoming == previous)
        return false;
    return static_cast<std::int32_t>(incoming - previous) > 0;
}

inline void copy3(float dst[3], const float src[3])
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

inline float distance3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline void reject(GameMovementValidateV1& r, MovementValidationReason reason)
{
    r.decision = static_cast<std::uint32_t>(MovementValidationDecision::Reject);
    r.reason = static_cast<std::uint32_t>(reason);
    r.result = 1u;
}

inline void validate(GameMovementValidateV1* request)
{
    if (!request)
        return;
    GameMovementValidateV1& r = *request;
    r.result = 0u;
    r.clearsAuthoritativeTransformAck = 0u;
    r.positionError = 0.0f;
    copy3(r.acceptedPosition, r.reportPosition);
    copy3(r.acceptedBaseVelocity, r.reportBaseVelocity);
    copy3(r.acceptedExternalImpulse, r.reportExternalImpulse);

    if (!r.playerExists) { reject(r, MovementValidationReason::UnknownPlayer); return; }
    if (!r.connectionActive) { reject(r, MovementValidationReason::InactiveConnection); return; }
    if (!r.connectionOwnsPlayer) { reject(r, MovementValidationReason::WrongOwner); return; }
    if (!r.spawned) { reject(r, MovementValidationReason::NotSpawned); return; }

    const bool reportIsCurrentLife =
        r.reportSpawnGeneration == r.spawnGeneration &&
        r.reportTransformEpoch == r.transformEpoch;

    if (!r.spawnStateActive && !reportIsCurrentLife) {
        reject(r, MovementValidationReason::NotActive);
        return;
    }
    if (r.dead) { reject(r, MovementValidationReason::Dead); return; }
    if (!r.movementEnabled && !reportIsCurrentLife) {
        reject(r, MovementValidationReason::MovementDisabled);
        return;
    }
    if (r.reportSpawnGeneration != r.spawnGeneration) {
        reject(r, MovementValidationReason::SpawnGenerationMismatch);
        return;
    }
    if (r.reportTransformEpoch != r.transformEpoch) {
        reject(r, MovementValidationReason::TransformEpochMismatch);
        return;
    }
    if (r.hasMovementSequence) {
        if (r.reportMovementSequence == r.lastMovementSequence) {
            reject(r, MovementValidationReason::DuplicateSequence);
            return;
        }
        if (!sequenceIsNewer(r.reportMovementSequence, r.lastMovementSequence)) {
            reject(r, MovementValidationReason::OldSequence);
            return;
        }
    }

    const bool finiteState =
        std::isfinite(r.reportMoveAxes[0]) && std::isfinite(r.reportMoveAxes[1]) &&
        finite3(r.reportCameraForward) && finite3(r.reportPosition) &&
        finite3(r.reportBaseVelocity) && finite3(r.reportExternalImpulse) &&
        std::isfinite(r.reportYaw) && std::isfinite(r.reportLookPitch) &&
        std::isfinite(r.reportSizeScale) && r.acceptedStateFinite != 0;
    if (!finiteState) { reject(r, MovementValidationReason::NonFinite); return; }

    if (!magnitudeAllowed3(r.reportPosition, 100000.0f) ||
        !magnitudeAllowed3(r.reportBaseVelocity, 5000.0f) ||
        !magnitudeAllowed3(r.reportExternalImpulse, 5000.0f) ||
        r.reportSizeScale <= 0.0f)
    {
        reject(r, MovementValidationReason::MalformedState);
        return;
    }

    if (r.awaitingAuthoritativeTransformAck) {
        // The epoch is the acknowledgement. Do not require a radius after apply.
        if (r.reportTransformEpoch != r.authoritativeTransformEpoch) {
            reject(r, MovementValidationReason::TransformEpochMismatch);
            return;
        }
        r.clearsAuthoritativeTransformAck = 1u;
    }

    const float* previousPosition = r.hasAcceptedClientTransform
        ? r.lastAcceptedClientPosition
        : r.playerPos;

    // Post-blackout drift correction.
    const std::uint64_t clientGapTicks =
        (r.lastAcceptedClientTick != 0 &&
         r.reportClientSimulationTick > r.lastAcceptedClientTick)
            ? r.reportClientSimulationTick - r.lastAcceptedClientTick
            : 0u;
    if (clientGapTicks > r.postGapCorrectionMinTicks) {
        const float drift = distance3(r.reportPosition, r.playerPos);
        if (drift > r.postGapCorrectionDistance) {
            r.decision = static_cast<std::uint32_t>(MovementValidationDecision::Correct);
            r.reason = static_cast<std::uint32_t>(
                MovementValidationReason::TooFarFromAuthoritative);
            copy3(r.acceptedPosition, r.playerPos);
            r.acceptedBaseVelocity[0] = r.acceptedBaseVelocity[1] = r.acceptedBaseVelocity[2] = 0.0f;
            r.acceptedExternalImpulse[0] = r.acceptedExternalImpulse[1] = r.acceptedExternalImpulse[2] = 0.0f;
            r.positionError = drift;
            r.result = 1u;
            return;
        }
    }

    // World bounds.
    bool inside = true;
    for (int i = 0; i < 3; ++i) {
        const float minB = r.worldBoundsMin[i] - r.worldBoundsPadding;
        const float maxB = r.worldBoundsMax[i] + r.worldBoundsPadding;
        if (r.reportPosition[i] < minB || r.reportPosition[i] > maxB)
            inside = false;
    }
    if (!inside) {
        r.decision = static_cast<std::uint32_t>(MovementValidationDecision::Correct);
        r.reason = static_cast<std::uint32_t>(MovementValidationReason::OutOfBounds);
        for (int i = 0; i < 3; ++i) {
            const float minB = r.worldBoundsMin[i] - r.worldBoundsPadding;
            const float maxB = r.worldBoundsMax[i] + r.worldBoundsPadding;
            r.acceptedPosition[i] =
                r.reportPosition[i] < minB ? minB
                : (r.reportPosition[i] > maxB ? maxB : r.reportPosition[i]);
        }
        r.result = 1u;
        return;
    }

    if (r.crossesBlockingGeometry && !r.belowVoidFloor) {
        r.decision = static_cast<std::uint32_t>(MovementValidationDecision::Correct);
        r.reason = static_cast<std::uint32_t>(MovementValidationReason::BlockingGeometry);
        copy3(r.acceptedPosition, previousPosition);
        r.acceptedBaseVelocity[0] = r.acceptedBaseVelocity[1] = r.acceptedBaseVelocity[2] = 0.0f;
        r.acceptedExternalImpulse[0] = r.acceptedExternalImpulse[1] = r.acceptedExternalImpulse[2] = 0.0f;
        r.positionError = distance3(r.reportPosition, previousPosition);
        r.result = 1u;
        return;
    }

    r.decision = static_cast<std::uint32_t>(MovementValidationDecision::Accept);
    r.reason = static_cast<std::uint32_t>(MovementValidationReason::None);
    r.result = 1u;
}

} // namespace HotMovementValidationImpl

} // namespace MimitaNet
