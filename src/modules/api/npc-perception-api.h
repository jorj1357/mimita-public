// 2026-11-09
/* purpose
* define the versioned plain-data boundary for hot NPC perception and sensing
* the module owns FOV/range/LOS gates, memory decay, belief, and prediction
* the host owns actor state, the world query mechanism, and authoritative ticks
* this file DOES NOT expose Npc, glm, STL, physics, or collision ownership
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_npc_perception {

constexpr std::uint32_t ABI_VERSION = 1;
constexpr std::uint32_t MAX_REPORT = 512;

// Host world-query capability. The host sets the current immutable world for
// the duration of a fixed-tick sense call; the module decides when to ask for
// a line-of-sight or ray result. Geometry data and lifetime stay in the host.
using LineOfSightBlockedFn = int (MIMITA_MODULE_CALL *)(
    const float from[3], const float to[3]);
using RaycastBlockedFn = int (MIMITA_MODULE_CALL *)(
    const float from[3], const float direction[3], float maxDistance,
    float* outDistance);

struct NpcWorldServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    LineOfSightBlockedFn lineOfSightBlocked;
    RaycastBlockedFn raycastBlocked;
};

struct TuningV1
{
    float horizontalFovDegrees = 100.0f;
    float sightRangeMeters = 100.0f;
    float hearingRangeMeters = 40.0f;
    float predictionSeconds = 0.15f;
    float predictionErrorMeters = 0.4f;
    std::int32_t reactionDelayTicks = 8;
    std::int32_t memoryTicks = 180;
};

struct MemoryV1
{
    std::uint32_t hasMemory = 0;
    float lastKnownPosition[3] = {};
    float lastKnownVelocity[3] = {};
    float ageSeconds = 0.0f;
    float uncertainty = 0.0f;
    float confidence = 0.0f;
};

struct SnapshotV1
{
    std::uint32_t candidateValid = 0;
    std::uint32_t withinFov = 0;
    std::uint32_t withinRange = 0;
    std::uint32_t hasLineOfSight = 0;
    std::uint32_t visible = 0;
    std::uint32_t radarKnown = 0;
    float position[3] = {};
    float velocity[3] = {};
    float distance = 0.0f;
};

struct BeliefV1
{
    std::uint32_t hasVisibleTarget = 0;
    std::uint32_t hasTarget = 0;
    std::uint32_t targetActorId = 0;
    float aimPosition[3] = {};
    float lastKnownPosition[3] = {};
    float distance = 0.0f;
    float confidence = 0.0f;
    float uncertainty = 0.0f;
};

enum Operation : std::uint32_t {
    OPERATION_PERCEIVE = 0,
    OPERATION_UPDATE_MEMORY = 1,
    OPERATION_BUILD_BELIEF = 2,
    OPERATION_PREDICT = 3,
    OPERATION_STEP = 4,
    OPERATION_SELFTEST = 5,
};

enum LosMode : std::uint32_t {
    LOS_PROVIDED = 0,
    LOS_QUERY_HOST = 1,
};

struct PerceptionRequestV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t operation;
    std::uint32_t actorId;
    std::uint32_t tick;
    float actorPosition[3] = {};
    float facing[3] = {};
    std::uint32_t candidateValid = 0;
    float candidatePosition[3] = {};
    float candidateVelocity[3] = {};
    TuningV1 tuning;
    std::uint32_t losMode = LOS_PROVIDED;
    std::uint32_t losBlocked = 0;
    float losFrom[3] = {};
    float losTo[3] = {};
    float dt = 0.0f;
    MemoryV1 memory;
    SnapshotV1 snapshot;
    float predictionPosition[3] = {};
    float predictionVelocity[3] = {};
    float predictionHorizonSeconds = 0.0f;
    float predictionErrorMeters = 0.0f;
    std::uint32_t targetActorId = 0;
    SnapshotV1 outSnapshot;
    MemoryV1 outMemory;
    BeliefV1 outBelief;
    float outPredictedPosition[3] = {};
    std::uint32_t outGeneration = 0;
    std::uint32_t outOk = 0;
    char outReport[MAX_REPORT] = {};
};

} // namespace mimita_npc_perception
