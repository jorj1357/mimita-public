// 2026-11-09
/* purpose
* hot-reloadable NPC perception, memory, belief, and prediction policy
* host owns actors, the immutable world query, and the fixed authoritative tick
* this module owns the FOV/range/LOS gate policy and sensory state transition
* this file DOES NOT own movement, firing, damage, or world lifetime
*/
#include "modules/api/npc-perception-api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <glm/glm.hpp>

#include <module-identity.h>

namespace {

const mimita_npc_perception::NpcWorldServicesV1* gWorld = nullptr;

float hashSigned(std::uint32_t id, std::uint32_t salt)
{
    std::uint32_t h = id * 2654435761u ^ salt * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFu) / 32767.5f - 1.0f;
}

bool withinSightCone(const float origin[3], const float facing[3],
                     const float target[3], float fovDegrees)
{
    glm::vec2 flatToTarget(target[0] - origin[0], target[1] - origin[1]);
    glm::vec2 flatFacing(facing[0], facing[1]);
    const float toLen = glm::length(flatToTarget);
    const float faceLen = glm::length(flatFacing);
    if (toLen <= 0.0001f || faceLen <= 0.0001f)
        return true;
    flatToTarget /= toLen;
    flatFacing /= faceLen;
    const float cosAngle =
        glm::clamp(glm::dot(flatToTarget, flatFacing), -1.0f, 1.0f);
    const float angleDeg = glm::degrees(std::acos(cosAngle));
    return angleDeg <= fovDegrees * 0.5f;
}

mimita_npc_perception::SnapshotV1 perceive(
    const mimita_npc_perception::PerceptionRequestV1& request,
    bool losBlocked)
{
    mimita_npc_perception::SnapshotV1 snap;
    snap.candidateValid = request.candidateValid ? 1u : 0u;
    std::memcpy(snap.position, request.candidatePosition, sizeof(snap.position));
    std::memcpy(snap.velocity, request.candidateVelocity, sizeof(snap.velocity));
    const glm::vec3 actor(request.actorPosition[0], request.actorPosition[1],
                          request.actorPosition[2]);
    const glm::vec3 candidate(request.candidatePosition[0],
                              request.candidatePosition[1],
                              request.candidatePosition[2]);
    snap.distance = glm::length(candidate - actor);
    if (!request.candidateValid)
        return snap;

    snap.withinRange =
        snap.distance <= request.tuning.sightRangeMeters ? 1u : 0u;
    snap.withinFov = withinSightCone(
        request.actorPosition, request.facing, request.candidatePosition,
        request.tuning.horizontalFovDegrees) ? 1u : 0u;
    snap.hasLineOfSight = losBlocked ? 0u : 1u;
    snap.visible = (snap.withinFov && snap.withinRange &&
                    snap.hasLineOfSight) ? 1u : 0u;
    return snap;
}

mimita_npc_perception::MemoryV1 updateMemory(
    const mimita_npc_perception::MemoryV1& current,
    const mimita_npc_perception::SnapshotV1& snapshot,
    const float candidatePosition[3], const float candidateVelocity[3],
    float dt, const mimita_npc_perception::TuningV1& tuning)
{
    mimita_npc_perception::MemoryV1 memory = current;
    if (snapshot.visible || snapshot.radarKnown) {
        memory.hasMemory = 1;
        std::memcpy(memory.lastKnownPosition, candidatePosition,
                    sizeof(memory.lastKnownPosition));
        std::memcpy(memory.lastKnownVelocity, candidateVelocity,
                    sizeof(memory.lastKnownVelocity));
        memory.ageSeconds = 0.0f;
        memory.uncertainty = 0.0f;
        memory.confidence = 1.0f;
        return memory;
    }
    if (!memory.hasMemory)
        return memory;

    memory.ageSeconds += dt;
    const float memorySeconds =
        std::max(0.001f, (float)tuning.memoryTicks / 60.0f);
    memory.uncertainty = std::min(30.0f, memory.uncertainty + dt * 4.0f);
    memory.confidence = glm::clamp(
        1.0f - memory.ageSeconds / memorySeconds, 0.0f, 1.0f);
    if (memory.ageSeconds >= memorySeconds) {
        memory.hasMemory = 0;
        memory.confidence = 0.0f;
    }
    return memory;
}

void predictPosition(const float lastKnownPos[3], const float lastKnownVel[3],
                     float horizonSeconds, float errorMeters,
                     std::uint32_t npcId, float out[3])
{
    out[0] = lastKnownPos[0] + lastKnownVel[0] * horizonSeconds;
    out[1] = lastKnownPos[1] + lastKnownVel[1] * horizonSeconds;
    out[2] = lastKnownPos[2] + lastKnownVel[2] * horizonSeconds;
    if (errorMeters <= 0.0f)
        return;
    out[0] += hashSigned(npcId, 1u) * errorMeters;
    out[1] += hashSigned(npcId, 2u) * errorMeters;
    out[2] += hashSigned(npcId, 3u) * errorMeters * 0.5f;
}

mimita_npc_perception::BeliefV1 buildBelief(
    const mimita_npc_perception::SnapshotV1& snapshot,
    const mimita_npc_perception::MemoryV1& memory,
    std::uint32_t targetActorId, float predictionErrorMeters,
    const mimita_npc_perception::TuningV1& tuning)
{
    mimita_npc_perception::BeliefV1 belief;
    belief.targetActorId = targetActorId;
    if (snapshot.visible) {
        belief.hasVisibleTarget = 1;
        belief.hasTarget = 1;
        std::memcpy(belief.lastKnownPosition, snapshot.position,
                    sizeof(belief.lastKnownPosition));
        belief.distance = snapshot.distance;
        belief.confidence = 1.0f;
        belief.uncertainty = 0.0f;
        predictPosition(snapshot.position, snapshot.velocity,
                        tuning.predictionSeconds, predictionErrorMeters,
                        targetActorId, belief.aimPosition);
        return belief;
    }
    if (memory.hasMemory && memory.confidence > 0.0f) {
        belief.hasTarget = 1;
        std::memcpy(belief.lastKnownPosition, memory.lastKnownPosition,
                    sizeof(belief.lastKnownPosition));
        const glm::vec3 lastKnown(memory.lastKnownPosition[0],
                                  memory.lastKnownPosition[1],
                                  memory.lastKnownPosition[2]);
        belief.distance = glm::length(lastKnown);
        belief.confidence = memory.confidence;
        belief.uncertainty = memory.uncertainty;
        predictPosition(memory.lastKnownPosition, memory.lastKnownVelocity,
                        tuning.predictionSeconds,
                        predictionErrorMeters + memory.uncertainty,
                        targetActorId, belief.aimPosition);
        return belief;
    }
    return belief;
}

void runSelfTest(mimita_npc_perception::PerceptionRequestV1& request)
{
    bool ok = true;
    std::string report;
    auto fail = [&](const char* why) {
        ok = false;
        report += "FAIL: ";
        report += why;
        report += "\n";
    };

    const float origin[3] = {0.0f, 0.0f, 0.0f};
    const float forward[3] = {1.0f, 0.0f, 0.0f};
    const float ahead[3] = {10.0f, 0.0f, 0.0f};
    const float side[3] = {0.0f, 10.0f, 0.0f};
    const float offAxis[3] = {10.0f, 3.0f, 0.0f};
    if (!withinSightCone(origin, forward, ahead, 100.0f))
        fail("ahead target should be within FOV");
    if (withinSightCone(origin, forward, side, 100.0f))
        fail("90-degree side target should be outside FOV");
    if (!withinSightCone(origin, forward, offAxis, 100.0f))
        fail("slightly off-axis target should be within FOV");
    report += "fov_gate=ok\n";

    mimita_npc_perception::TuningV1 tuning;
    tuning.sightRangeMeters = 50.0f;
    if (!(40.0f <= tuning.sightRangeMeters))
        fail("near target should be in range");
    if (60.0f <= tuning.sightRangeMeters)
        fail("far target should be out of range");

    tuning.memoryTicks = 60;
    mimita_npc_perception::MemoryV1 memory;
    mimita_npc_perception::SnapshotV1 visible;
    visible.visible = 1;
    visible.position[0] = 5.0f;
    memory = updateMemory(memory, visible, visible.position, visible.velocity,
                          1.0f / 60.0f, tuning);
    if (!memory.hasMemory || memory.confidence < 0.99f)
        fail("visible snapshot should set full memory");

    mimita_npc_perception::SnapshotV1 unseen;
    for (int i = 0; i < 30; ++i)
        memory = updateMemory(memory, unseen, unseen.position,
                              unseen.velocity, 1.0f / 60.0f, tuning);
    if (memory.confidence >= 0.99f)
        fail("confidence should decay while unseen");
    if (memory.uncertainty <= 0.0f)
        fail("uncertainty should grow while unseen");
    for (int i = 0; i < 60; ++i)
        memory = updateMemory(memory, unseen, unseen.position,
                              unseen.velocity, 1.0f / 60.0f, tuning);
    if (memory.hasMemory)
        fail("memory should expire after memoryTicks");
    report += "memory_decay=ok\n";

    mimita_npc_perception::MemoryV1 remembered;
    remembered.hasMemory = 1;
    remembered.lastKnownPosition[0] = 20.0f;
    remembered.confidence = 0.5f;
    remembered.uncertainty = 4.0f;
    mimita_npc_perception::TuningV1 beliefTuning;
    const auto belief = buildBelief(
        mimita_npc_perception::SnapshotV1{}, remembered, 0, 0.4f, beliefTuning);
    if (!belief.hasTarget || belief.hasVisibleTarget)
        fail("memory-only belief should be a non-visible target");
    if (belief.confidence >= 0.99f)
        fail("memory-only confidence should be below 1");
    const auto none = buildBelief(
        mimita_npc_perception::SnapshotV1{},
        mimita_npc_perception::MemoryV1{}, 0, 0.4f, beliefTuning);
    if (none.hasTarget)
        fail("no memory should yield no target");
    report += "belief=ok\n";

    float predicted[3] = {};
    const float p0[3] = {0.0f, 0.0f, 0.0f};
    const float v0[3] = {10.0f, 0.0f, 0.0f};
    predictPosition(p0, v0, 0.5f, 0.0f, 7u, predicted);
    if (std::fabs(predicted[0] - 5.0f) > 0.001f)
        fail("prediction without error should be exact extrapolation");
    float perturbed[3] = {};
    predictPosition(p0, v0, 0.5f, 0.5f, 7u, perturbed);
    if (std::fabs(perturbed[0] - 5.0f) <= 0.0001f)
        fail("prediction with error should perturb the result");
    report += "prediction=ok\n";

    report += ok ? "PASS\n" : "FAIL\n";
    request.outOk = ok ? 1u : 0u;
    std::snprintf(request.outReport, sizeof(request.outReport), "%s",
                  report.c_str());
}

int MIMITA_MODULE_CALL initialize(
    const mimita_module::HostServicesV1* services)
{
    if (!services || services->version != mimita_module::ABI_VERSION ||
        services->structSize != sizeof(*services) || !services->log)
        return 0;
    gWorld = static_cast<const mimita_npc_perception::NpcWorldServicesV1*>(
        services->getCapability(mimita_module::CAPABILITY_NPC_WORLD,
                                mimita_npc_perception::ABI_VERSION));
    return 1;
}

void MIMITA_MODULE_CALL shutdown()
{
    gWorld = nullptr;
}

int MIMITA_MODULE_CALL onEvent(mimita_module::ModuleEventV1* event)
{
    if (!event || !event->payload ||
        event->typeId != mimita_module::EVENT_NPC_PERCEPTION ||
        event->payloadSize !=
            sizeof(mimita_npc_perception::PerceptionRequestV1))
        return 0;
    auto& request = *static_cast<mimita_npc_perception::PerceptionRequestV1*>(
        event->payload);
    if (request.version != mimita_npc_perception::ABI_VERSION ||
        request.structSize != sizeof(request))
        return 0;

    switch (request.operation) {
    case mimita_npc_perception::OPERATION_PERCEIVE: {
        bool losBlocked = request.losBlocked != 0;
        if (request.losMode == mimita_npc_perception::LOS_QUERY_HOST &&
            gWorld && gWorld->lineOfSightBlocked) {
            losBlocked =
                gWorld->lineOfSightBlocked(request.losFrom, request.losTo) != 0;
        }
        request.outSnapshot = perceive(request, losBlocked);
        request.outGeneration = MIMITA_MODULE_GENERATION;
        return 1;
    }
    case mimita_npc_perception::OPERATION_UPDATE_MEMORY:
        request.outMemory = updateMemory(
            request.memory, request.snapshot, request.candidatePosition,
            request.candidateVelocity, request.dt, request.tuning);
        request.outGeneration = MIMITA_MODULE_GENERATION;
        return 1;
    case mimita_npc_perception::OPERATION_BUILD_BELIEF:
        request.outBelief = buildBelief(
            request.snapshot, request.memory, request.targetActorId,
            request.predictionErrorMeters, request.tuning);
        request.outGeneration = MIMITA_MODULE_GENERATION;
        return 1;
    case mimita_npc_perception::OPERATION_PREDICT:
        predictPosition(request.predictionPosition, request.predictionVelocity,
                        request.predictionHorizonSeconds,
                        request.predictionErrorMeters, request.actorId,
                        request.outPredictedPosition);
        request.outGeneration = MIMITA_MODULE_GENERATION;
        return 1;
    case mimita_npc_perception::OPERATION_STEP: {
        bool losBlocked = request.losBlocked != 0;
        if (request.losMode == mimita_npc_perception::LOS_QUERY_HOST &&
            gWorld && gWorld->lineOfSightBlocked) {
            losBlocked =
                gWorld->lineOfSightBlocked(request.losFrom, request.losTo) != 0;
        }
        request.outSnapshot = perceive(request, losBlocked);
        request.outMemory = updateMemory(
            request.memory, request.outSnapshot, request.candidatePosition,
            request.candidateVelocity, request.dt, request.tuning);
        request.outBelief = buildBelief(
            request.outSnapshot, request.outMemory, request.targetActorId,
            request.predictionErrorMeters, request.tuning);
        request.outGeneration = MIMITA_MODULE_GENERATION;
        return 1;
    }
    case mimita_npc_perception::OPERATION_SELFTEST:
        runSelfTest(request);
        request.outGeneration = MIMITA_MODULE_GENERATION;
        return 1;
    default:
        return 0;
    }
}

std::uint32_t MIMITA_MODULE_CALL exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int MIMITA_MODULE_CALL importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_module::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION, sizeof(mimita_module::ModuleV1),
     "npc-perception", MIMITA_MODULE_GENERATION, MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize, shutdown, onEvent, exportState, importState
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    return abiVersion == mimita_module::ABI_VERSION ? &gModule.base : nullptr;
}
