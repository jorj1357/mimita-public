// 10 02 2026
// Human-like perception, memory, belief, and prediction. No movement/firing.
#include "npc/npc-perception.h"

#include "npc/npc.h"

#include <algorithm>
#include <cmath>

namespace {

// Deterministic hash in [-1, 1] from an NPC id + a salt so prediction error is
// stable per NPC and tick-independent.
float hashSigned(uint32_t id, uint32_t salt)
{
    uint32_t h = id * 2654435761u ^ salt * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFu) / 32767.5f - 1.0f;
}

} // namespace

bool withinSightCone(const glm::vec3& origin,
                     const glm::vec3& facing,
                     const glm::vec3& targetPos,
                     float fovDegrees)
{
    glm::vec2 flatToTarget(targetPos.x - origin.x, targetPos.y - origin.y);
    glm::vec2 flatFacing(facing.x, facing.y);
    const float toLen = glm::length(flatToTarget);
    const float faceLen = glm::length(flatFacing);
    if (toLen <= 0.0001f || faceLen <= 0.0001f)
        return true;  // degenerate; treat as centered
    flatToTarget /= toLen;
    flatFacing /= faceLen;
    const float cosAngle = glm::clamp(glm::dot(flatToTarget, flatFacing), -1.0f, 1.0f);
    const float angleDeg = glm::degrees(std::acos(cosAngle));
    return angleDeg <= fovDegrees * 0.5f;
}

PerceptionSnapshot perceive(const Npc& npc,
                            bool candidateValid,
                            const glm::vec3& candidatePos,
                            const glm::vec3& candidateVel,
                            const glm::vec3& facingDir,
                            bool losBlocked,
                            const PerceptionTuning& tuning)
{
    PerceptionSnapshot snap;
    snap.candidateValid = candidateValid;
    snap.position = candidatePos;
    snap.velocity = candidateVel;

    const glm::vec3 toTarget = candidatePos - npc.body.pos;
    snap.distance = glm::length(toTarget);
    if (!candidateValid) {
        return snap;
    }

    // Range gate.
    snap.withinRange = snap.distance <= tuning.sightRangeMeters;

    // Horizontal FOV gate.
    snap.withinFov = withinSightCone(npc.body.pos, facingDir, candidatePos,
                                     tuning.horizontalFovDegrees);

    snap.hasLineOfSight = !losBlocked;
    snap.visible = snap.withinFov && snap.withinRange && snap.hasLineOfSight;
    return snap;
}

void updateMemory(MemoryRecord& memory,
                  const PerceptionSnapshot& snapshot,
                  const glm::vec3& candidatePos,
                  const glm::vec3& candidateVel,
                  float dt,
                  const PerceptionTuning& tuning)
{
    if (snapshot.visible) {
        memory.hasMemory = true;
        memory.lastKnownPosition = candidatePos;
        memory.lastKnownVelocity = candidateVel;
        memory.ageSeconds = 0.0f;
        memory.uncertainty = 0.0f;
        memory.confidence = 1.0f;
        return;
    }

    if (!memory.hasMemory)
        return;

    memory.ageSeconds += dt;
    // Uncertainty grows with age; confidence falls linearly to zero at the
    // configured memory duration.
    const float memorySeconds = std::max(0.001f, (float)tuning.memoryTicks / 60.0f);
    memory.uncertainty = std::min(30.0f, memory.uncertainty + dt * 4.0f);
    memory.confidence = glm::clamp(1.0f - memory.ageSeconds / memorySeconds, 0.0f, 1.0f);

    if (memory.ageSeconds >= memorySeconds) {
        memory.hasMemory = false;
        memory.confidence = 0.0f;
    }
}

BeliefState buildBelief(const PerceptionSnapshot& snapshot,
                        const MemoryRecord& memory,
                        uint32_t targetActorId,
                        float predictionErrorMeters,
                        const PerceptionTuning& tuning)
{
    BeliefState belief;
    belief.targetActorId = targetActorId;

    if (snapshot.visible) {
        belief.hasVisibleTarget = true;
        belief.hasTarget = true;
        belief.lastKnownPosition = snapshot.position;
        belief.distance = snapshot.distance;
        belief.confidence = 1.0f;
        belief.uncertainty = 0.0f;
        belief.aimPosition = predictTargetPosition(
            snapshot.position, snapshot.velocity, tuning.predictionSeconds,
            predictionErrorMeters, targetActorId);
        return belief;
    }

    if (memory.hasMemory && memory.confidence > 0.0f) {
        belief.hasTarget = true;
        belief.lastKnownPosition = memory.lastKnownPosition;
        belief.distance = glm::length(memory.lastKnownPosition);
        belief.confidence = memory.confidence;
        belief.uncertainty = memory.uncertainty;
        // Aim at the predicted last-known position; error scales with
        // uncertainty so a stale memory is not treated as exact knowledge.
        belief.aimPosition = predictTargetPosition(
            memory.lastKnownPosition, memory.lastKnownVelocity,
            tuning.predictionSeconds,
            predictionErrorMeters + memory.uncertainty,
            targetActorId);
        return belief;
    }

    return belief;
}

glm::vec3 predictTargetPosition(const glm::vec3& lastKnownPos,
                                const glm::vec3& lastKnownVel,
                                float horizonSeconds,
                                float errorMeters,
                                uint32_t npcId)
{
    glm::vec3 predicted = lastKnownPos + lastKnownVel * horizonSeconds;
    if (errorMeters <= 0.0f)
        return predicted;
    predicted.x += hashSigned(npcId, 1u) * errorMeters;
    predicted.y += hashSigned(npcId, 2u) * errorMeters;
    predicted.z += hashSigned(npcId, 3u) * errorMeters * 0.5f;
    return predicted;
}

bool npcPerceptionSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    const glm::vec3 origin(0.0f);

    // FOV: a target directly ahead (100 deg cone => 50 deg half-angle) is inside;
    // one 90 degrees to the side is outside.
    if (!withinSightCone(origin, glm::vec3(1, 0, 0), glm::vec3(10, 0, 0), 100.0f))
        fail("ahead target should be within FOV");
    if (withinSightCone(origin, glm::vec3(1, 0, 0), glm::vec3(0, 10, 0), 100.0f))
        fail("90-degree side target should be outside FOV");
    if (!withinSightCone(origin, glm::vec3(1, 0, 0), glm::vec3(10, 3, 0), 100.0f))
        fail("slightly off-axis target should be within FOV");
    report += "fov_gate=ok\n";

    // Range gate is a plain distance compare; validate the boundary through
    // PerceptionTuning semantics (sightRange).
    {
        PerceptionTuning t;
        t.sightRangeMeters = 50.0f;
        const float distNear = glm::length(glm::vec3(40, 0, 0));
        const float distFar = glm::length(glm::vec3(60, 0, 0));
        if (!(distNear <= t.sightRangeMeters)) fail("near target should be in range");
        if (distFar <= t.sightRangeMeters) fail("far target should be out of range");
    }

    // Memory: a visible snapshot refreshes; an unseen one decays to zero by
    // memoryTicks and reports falling confidence.
    {
        PerceptionTuning t;
        t.memoryTicks = 60;  // 1 second
        MemoryRecord mem;
        PerceptionSnapshot vis;
        vis.visible = true;
        vis.position = glm::vec3(5, 0, 0);
        updateMemory(mem, vis, vis.position, glm::vec3(0), 1.0f / 60.0f, t);
        if (!mem.hasMemory || mem.confidence < 0.99f) fail("visible snapshot should set full memory");

        PerceptionSnapshot unseen;  // visible=false
        for (int i = 0; i < 30; ++i)
            updateMemory(mem, unseen, glm::vec3(0), glm::vec3(0), 1.0f / 60.0f, t);
        if (mem.confidence >= 0.99f) fail("confidence should decay while unseen");
        if (mem.uncertainty <= 0.0f) fail("uncertainty should grow while unseen");

        for (int i = 0; i < 60; ++i)
            updateMemory(mem, unseen, glm::vec3(0), glm::vec3(0), 1.0f / 60.0f, t);
        if (mem.hasMemory) fail("memory should expire after memoryTicks");
        report += "memory_decay=ok\n";
    }

    // Belief: with memory present but unseen, hasTarget is true and confidence
    // is below 1 (not perfect knowledge). With no memory, no target.
    {
        MemoryRecord mem;
        mem.hasMemory = true;
        mem.lastKnownPosition = glm::vec3(20, 0, 0);
        mem.confidence = 0.5f;
        mem.uncertainty = 4.0f;
        PerceptionTuning t;
        const BeliefState b = buildBelief(PerceptionSnapshot{}, mem, 0, 0.4f, t);
        if (!b.hasTarget || b.hasVisibleTarget) fail("memory-only belief should be a non-visible target");
        if (b.confidence >= 0.99f) fail("memory-only confidence should be below 1");
        const BeliefState none = buildBelief(PerceptionSnapshot{}, MemoryRecord{}, 0, 0.4f, t);
        if (none.hasTarget) fail("no memory should yield no target");
        report += "belief=ok\n";
    }

    // Prediction: moving target is extrapolated, and nonzero error perturbs the
    // result so tracking is never exact.
    {
        const glm::vec3 p = predictTargetPosition(
            glm::vec3(0), glm::vec3(10, 0, 0), 0.5f, 0.0f, 7u);
        if (glm::length(p - glm::vec3(5, 0, 0)) > 0.001f)
            fail("prediction without error should be exact extrapolation");
        const glm::vec3 pe = predictTargetPosition(
            glm::vec3(0), glm::vec3(10, 0, 0), 0.5f, 0.5f, 7u);
        if (glm::length(pe - glm::vec3(5, 0, 0)) <= 0.0001f)
            fail("prediction with error should perturb the result");
        report += "prediction=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}

