// 07 21 2026, 16 45
/* purpose
* Declares Stage 3A client movement report validation data and helpers.
* Keeps movement report authority separate from packet transport and simulation ownership.
* Exposes bounded diagnostics, ordering, lifecycle, and correction classification APIs.
* Does NOT send packets, poll sockets, render, play audio, or mutate clients.
* Does NOT implement server-derived movement replay, input acknowledgements, or rewind.
* Does NOT trust damage, health, ammo, score, projectile hits, or weapon outcomes.
*/

#pragma once

#include "physics/movement/movement-types.h"

#include <cstdint>

#include <glm/glm.hpp>

namespace MimitaNet {

struct HeadlessWorld;
struct ServerPlayer;

enum MovementReportFlags : uint32_t
{
    MOVEMENT_REPORT_ON_GROUND = 1u << 0,
    MOVEMENT_REPORT_STABLE_ON_GROUND = 1u << 1,
    MOVEMENT_REPORT_HAS_WORLD_CONTACT = 1u << 2,
    MOVEMENT_REPORT_REAL_WORLD_CONTACT = 1u << 3,
    MOVEMENT_REPORT_AIR_JUMP_ARMED = 1u << 4,
    MOVEMENT_REPORT_AIR_JUMP_LOCKED = 1u << 5,
    MOVEMENT_REPORT_DASH_AVAILABLE = 1u << 6,
    MOVEMENT_REPORT_DASH_PROTECTED = 1u << 7,
    MOVEMENT_REPORT_DOWN_DASH_AVAILABLE = 1u << 8,
    MOVEMENT_REPORT_FREEZE_ACTIVE = 1u << 9,
    MOVEMENT_REPORT_FREEZE_AVAILABLE = 1u << 10,
    MOVEMENT_REPORT_GROUND_RETURN_AVAILABLE = 1u << 11,
    MOVEMENT_REPORT_JUMP_HELD = 1u << 12,
    MOVEMENT_REPORT_DASH_PRESSED = 1u << 13,
    MOVEMENT_REPORT_DOWN_DASH_PRESSED = 1u << 14,
    MOVEMENT_REPORT_FREEZE_HELD = 1u << 15
};

struct ClientMovementReport
{
    uint32_t playerId = 0;
    uint32_t movementSequence = 0;
    uint64_t clientSimulationTick = 0;
    MovementLifecycleIdentity lifecycle;

    glm::vec2 moveAxes{0.0f};
    glm::vec3 horizontalCameraForward{1.0f, 0.0f, 0.0f};
    glm::vec3 position{0.0f};
    glm::vec3 baseVelocity{0.0f};
    glm::vec3 externalImpulse{0.0f};
    float yaw = 0.0f;
    float lookPitch = 0.0f;
    float sizeScale = 1.0f;
    uint32_t movementFlags = 0;
    int32_t clientPingMs = 0;

    uint16_t dashSerial = 0;
    uint16_t groundJumpSerial = 0;
    uint16_t airJumpSerial = 0;
    uint16_t downDashSerial = 0;
    uint16_t freezeSerial = 0;
};

struct MovementValidationConfig
{
    uint32_t maximumReportAgeTicks = 180;
    uint32_t maximumFutureTickLead = 60;
    float ordinaryDisplacementTolerance = 3.0f;
    float latencyDisplacementAllowance = 6.0f;
    float maximumBaseHorizontalSpeed = 180.0f;
    float maximumBaseUpwardSpeed = 180.0f;
    float maximumBaseDownwardSpeed = 450.0f;
    float maximumExternalHorizontalImpulse = 260.0f;
    float maximumExternalVerticalImpulse = 260.0f;
    float maximumCombinedEmergencySpeed = 700.0f;
    float dashDisplacementAllowance = 12.0f;
    float downDashDisplacementAllowance = 18.0f;
    float externalImpulseDisplacementSeconds = 0.35f;
    float collisionDepenetrationAllowance = 2.0f;
    float authoritativeAckDistance = 5.0f;
    float wallSweepTolerance = 0.35f;
    glm::vec3 worldBoundsMin{-20000.0f, -20000.0f, -5000.0f};
    glm::vec3 worldBoundsMax{20000.0f, 20000.0f, 20000.0f};
    float worldBoundsPadding = 8.0f;
    float smallCorrectionDistance = 0.5f;
    float mediumCorrectionDistance = 5.0f;
    float majorCorrectionDistance = 100.0f;
    int maximumAirJumps = 1;
};

struct MovementValidationContext
{
    bool playerExists = true;
    bool connectionActive = true;
    bool connectionOwnsPlayer = true;
    uint32_t serverTick = 0;
    uint64_t nowMs = 0;
    const HeadlessWorld* world = nullptr;
};

enum class MovementValidationDecision : uint8_t
{
    Accept,
    Correct,
    Reject
};

enum class MovementValidationReason : uint8_t
{
    None,
    UnknownPlayer,
    WrongOwner,
    InactiveConnection,
    NotSpawned,
    NotActive,
    Dead,
    MovementDisabled,
    SpawnGenerationMismatch,
    TransformEpochMismatch,
    DuplicateSequence,
    OldSequence,
    StaleClientTick,
    FutureClientTick,
    NonFinite,
    MalformedState,
    ImpossibleDisplacement,
    ImpossibleVelocity,
    OutOfBounds,
    BlockingGeometry,
    AbilityTransition,
    AwaitingAuthoritativeTransformAck,
    TooFarFromAuthoritative
};

enum class MovementCorrectionClass : uint8_t
{
    None,
    Small,
    Medium,
    Major
};

struct MovementValidationMetrics
{
    float positionError = 0.0f;
    float velocityError = 0.0f;
    float horizontalDelta = 0.0f;
    float verticalDelta = 0.0f;
    float allowedHorizontalDelta = 0.0f;
    float allowedVerticalDelta = 0.0f;
    float horizontalBaseSpeed = 0.0f;
    float upwardBaseSpeed = 0.0f;
    float downwardBaseSpeed = 0.0f;
    float horizontalExternalImpulse = 0.0f;
    float verticalExternalImpulse = 0.0f;
    float combinedSpeed = 0.0f;
};

struct MovementValidationResult
{
    MovementValidationDecision decision = MovementValidationDecision::Reject;
    MovementValidationReason reason = MovementValidationReason::None;
    MovementState acceptedState;
    MovementValidationMetrics metrics;
    bool clearsAuthoritativeTransformAck = false;
};

struct MovementValidationCounters
{
    uint64_t acceptedReports = 0;
    uint64_t correctedReports = 0;
    uint64_t rejectedReports = 0;
    uint64_t duplicateReports = 0;
    uint64_t oldLifeReports = 0;
    uint64_t wrongEpochReports = 0;
    uint64_t nonFiniteReports = 0;
    uint64_t boundsCorrections = 0;
    uint64_t wallCorrections = 0;
    float maximumPositionError = 0.0f;
    float maximumVelocityError = 0.0f;
    uint32_t lastAcceptedSequence = 0;
    uint64_t lastAcceptedClientTick = 0;
    MovementValidationReason lastReason = MovementValidationReason::None;
};

MovementValidationConfig makeMovementValidationConfig(
    const MovementConfig& movementConfig,
    const HeadlessWorld* world = nullptr);

bool movementReportSequenceIsNewer(uint32_t incoming, uint32_t previous);

MovementCorrectionClass classifyMovementCorrection(
    float errorDistance,
    const MovementValidationConfig& config);

const char* movementValidationDecisionName(MovementValidationDecision decision);
const char* movementValidationReasonName(MovementValidationReason reason);
const char* movementCorrectionClassName(MovementCorrectionClass correctionClass);

bool movementSnapshotIsFresh(uint32_t incomingServerTick,
                             uint32_t incomingSpawnGeneration,
                             uint32_t incomingTransformEpoch,
                             uint32_t lastServerTick,
                             uint32_t lastSpawnGeneration,
                             uint32_t lastTransformEpoch);

// Lifecycle-only freshness: rejects a sample from an older spawn generation or
// transform epoch (a previous life) but does NOT gate on the tick. Used by the
// interpolation buffer so a reordered older sample can fill a hole in the tick
// history without ever leaking data from a previous life.
bool movementSnapshotLifecycleFresh(uint32_t incomingSpawnGeneration,
                                    uint32_t incomingTransformEpoch,
                                    uint32_t lastSpawnGeneration,
                                    uint32_t lastTransformEpoch);

MovementValidationResult validateClientMovementReport(
    const ServerPlayer& player,
    const ClientMovementReport& report,
    const MovementValidationContext& context,
    const MovementValidationConfig& config);

void applyMovementValidationCounters(MovementValidationCounters& counters,
                                     const MovementValidationResult& result,
                                     const ClientMovementReport& report);

void resetServerMovementForAuthoritativeLifecycle(
    ServerPlayer& player,
    const MovementConfig& movementConfig);

void recordServerMovementExternalImpulse(ServerPlayer& player,
                                         const glm::vec3& impulse);

} // namespace MimitaNet
