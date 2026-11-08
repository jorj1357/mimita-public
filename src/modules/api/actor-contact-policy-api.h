// 2026-11-07
/* purpose
* define the versioned plain-data collision policy shared with the live
* collision module: actor seam/contact response tuning and streamed-surface
* merge mode
* this file DOES NOT expose RecoveryContact, World, or STL types
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_collision_policy {

// Bounded host-owned contact arrays for the shared depenetration solver.
// This interface is independent of Player, World, glm, and STL storage.
constexpr std::uint32_t MAX_SOLVE_CONTACTS = 1024;
struct SolverContactV1 {
    float normal[3];
    float point[3];
    float penetration;
};
struct ContactSolveV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t count;
    std::uint32_t capacity;
    const SolverContactV1* contacts;
    SolverContactV1* scratch;
    float slop;
    float movementBias;
    float intendedMove[3];
    float correction[3];
    float weightedNormal[3];
    float maxPenetration;
    std::uint32_t manifoldCount;
    std::uint32_t generation;
};

constexpr std::uint32_t ABI_VERSION = 1;

struct HostServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    mimita_module::LogFn log;
};

enum SurfaceMergeMode : std::uint32_t
{
    SURFACE_RAW = 0,
    SURFACE_FACE_REMOVAL = 1,
    SURFACE_BOUNDED_UNION = 2
};

struct ContactPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;

    // Master switches for the hot seam-response policy.
    int enabled;
    int removeTouchingFaceSeams;
    int collapseFeatureContacts;
    int suppressLowFlushSeams;
    int collapseSamePartFeatures;

    // Distances are meters. Values are clamped by the host to safe ranges.
    float opposingFaceSeamDistance;
    float opposingFaceNormalDot;
    float featureCollapseDistance;
    float flushSeamHorizontalDistance;
    float flushWallTopHeight;
    float flushSupportZTolerance;
    float flushSupportHorizontalDistance;

    int result;
};

struct SurfacePolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;

    // SURFACE_FACE_REMOVAL or SURFACE_BOUNDED_UNION. Raw is the fallback.
    std::uint32_t mergeMode;
    // Maximum gap considered touching when validating connector contact.
    float contactTolerance;
    int result;
};

enum ActorPairPolicyFlags : std::uint32_t
{
    ACTOR_PAIR_CONTINUING_SUPPORT = 1u << 0,
    ACTOR_PAIR_UPPER_ON_GROUND = 1u << 1,
    ACTOR_PAIR_STATIC = 1u << 2,
    ACTOR_PAIR_DEAD = 1u << 3,
};

// Bounded actor-pair response policy. The host owns the broadphase, capsule
// sweep, and state mutation; the module decides push speed and support
// eligibility from profile tuning and the measured pair facts.
struct ActorPairPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t flags;

    float weakHitDistanceM;
    float weakHitMotionSeconds;
    float minImpactSpeedMps;
    float maxImpactSpeedMps;
    float impactSpeedCurveExponent;
    float impactAngleMinScale;
    float impactAngleCurveExponent;
    float maxKnockbackSpeedMps;

    float impactSpeed;
    float directness;

    float upperBottom;
    float lowerTop;
    float relativeVertical;
    float approachSpeed;
    float supportTopToleranceM;
    float supportSnapMaxM;
    float supportMaxImpactSpeedMps;

    float outPushSpeed;
    float outSupportDeltaZ;
    std::uint32_t outPush;
    std::uint32_t outSupport;
    std::uint32_t outGeneration;
};

// Stable containment limits for the final-pose safety pass. These are
// authoritative safety bounds, not gameplay tuning; the module may adjust them
// live, and the host applies the returned values.
struct FinalPosePolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    float maxSolidPushOut;
    float pushSkin;
    float contactEpsilon;
    float clearanceEpsilon;
    std::int32_t maxContainmentIterations;
    float maxCorrectionPerIteration;
    float maxTotalCorrection;
    std::uint32_t outGeneration;
};

} // namespace mimita_collision_policy
