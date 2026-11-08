#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_network_policy {

constexpr std::uint32_t ABI_VERSION = 1;

struct SnapshotPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t tick;
    std::uint32_t chunkCount;
    std::uint32_t entityCount;
    std::uint32_t playerCount;
    std::uint64_t queueBytes;
    std::uint32_t queueChunks;
    std::int32_t redundancyCopies;
    std::int32_t resendPrevious;
    std::int32_t partialKeepalive;
    std::int32_t maxBufferedTicks;
    std::int32_t maxAvatarQueueChunks;
    std::int32_t result;
};

struct ReconnectPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint64_t lastHeardAgeMs;
    std::uint64_t snapshotAgeMs;
    std::uint32_t receivedChunks;
    std::uint32_t expectedChunks;
    std::int32_t reconnectAttempts;
    std::int32_t action;
    std::int32_t backoffMs;
    std::int32_t retryIntervalMs;
    std::int32_t graceMs;
    std::int32_t result;
};

// Correction classification policy. The host owns the measured authoritative
// error; the module decides whether it is small, medium, or a hard snap. This
// keeps the correction cause and magnitude live-editable without exposing
// Player or MultiplayerContext.
struct MovementCorrectionPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    float errorDistance;
    float smallCorrectionDistance;
    float mediumCorrectionDistance;
    float majorCorrectionDistance;
    std::uint32_t outClass; // 0 none, 1 small, 2 medium, 3 major
    std::uint32_t outGeneration;
};

// Server hit-claim acceptance tolerances. The host owns the rewind pose and
// geometric tests; the module decides the tolerance values and can reject an
// over-wide claim budget live.
struct HitClaimPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    float rewindHitTolerance;
    float claimLagAllowance;
    float baseOriginTolerance;
    float pingAllowance;
    float outHitTolerance;
    float outOriginTolerance;
    std::uint32_t outAccept;
    std::uint32_t outGeneration;
};

// Client local-player reconciliation decision. The host owns the measured
// error and transform; the module decides hard snap versus smoothing.
struct ReconcilePolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    float positionError;
    float hardSnapDistance;
    float correctionLogDistance;
    std::uint32_t flags; // 1=authoritative epoch ready, 2=post-gap resync
    std::uint32_t outHardSnap;
    std::uint32_t outLogCorrection;
    std::uint32_t outGeneration;
};

// Remote interpolation discontinuity policy. The host owns the snapshot
// buffer; the module decides teleport gap distance and extrapolation limits.
struct InterpolationPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t gapTicks;
    std::uint32_t teleportGapTicks;
    float gapDistance;
    float teleportDistance;
    float maximumExtrapolationSeconds;
    std::uint32_t outSnapTicks;
    std::uint32_t outSnapDistance;
    float outMaximumExtrapolationSeconds;
    std::uint32_t outGeneration;
};

} // namespace mimita_network_policy
