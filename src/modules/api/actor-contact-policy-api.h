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

} // namespace mimita_collision_policy
