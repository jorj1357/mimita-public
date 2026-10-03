// 10 02 2026
/* purpose
* Human-like NPC perception and memory: FOV + range + line-of-sight gated
* target acquisition, reaction delay, bounded memory, last-known position with
* uncertainty, and skill-error position prediction.
* Reusable by NPC brains; does not fire weapons or move actors.
* Does NOT own tactical policy, movement, or damage.
* Does NOT create a second line-of-sight implementation (reuses the shared ray).
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct Npc;

// One raw observation of a candidate this tick, before belief filtering.
struct PerceptionSnapshot {
    bool candidateValid = false;   // a live hostile target exists at all
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float distance = 0.0f;
    bool withinFov = false;        // inside the NPC's horizontal sight cone
    bool withinRange = false;      // inside sight range
    bool hasLineOfSight = false;   // unobstructed ray to the candidate
    bool visible = false;          // all gates passed (valid && fov && range && los)
};

// A bounded, decaying memory of where a target was last seen and how sure the
// NPC is. Confidence decays over time; uncertainty grows while unobserved.
struct MemoryRecord {
    bool hasMemory = false;
    glm::vec3 lastKnownPosition{0.0f};
    glm::vec3 lastKnownVelocity{0.0f};
    float ageSeconds = 0.0f;       // time since the memory was last refreshed
    float uncertainty = 0.0f;      // meters; grows while unobserved
    float confidence = 0.0f;       // 0..1; falls while unobserved
};

// The NPC's current belief about its target after applying perception + memory.
struct BeliefState {
    bool hasVisibleTarget = false;
    bool hasTarget = false;              // visible OR remembered within duration
    uint32_t targetActorId = 0;
    glm::vec3 aimPosition{0.0f};         // where to shoot (predicted + noise)
    glm::vec3 lastKnownPosition{0.0f};
    float distance = 0.0f;
    float confidence = 0.0f;
    float uncertainty = 0.0f;
};

// Tunables for one perception step. Values are in meters/seconds/ticks.
struct PerceptionTuning {
    float horizontalFovDegrees = 100.0f;
    float sightRangeMeters = 100.0f;
    float hearingRangeMeters = 40.0f;
    int reactionDelayTicks = 8;
    int memoryTicks = 180;
    float predictionSeconds = 0.15f;
    float predictionErrorMeters = 0.4f;  // skill-scaled by the caller
};

// Compute the raw snapshot: whether the candidate is within FOV, range, and LOS.
// Los is evaluated by the caller-provided `losBlocked` (the shared cached ray),
// so this module does not own a second line-of-sight implementation.
PerceptionSnapshot perceive(const Npc& npc,
                            bool candidateValid,
                            const glm::vec3& candidatePos,
                            const glm::vec3& candidateVel,
                            const glm::vec3& facingDir,
                            bool losBlocked,
                            const PerceptionTuning& tuning);

// Update a memory record from a snapshot and a time step.
void updateMemory(MemoryRecord& memory,
                  const PerceptionSnapshot& snapshot,
                  const glm::vec3& candidatePos,
                  const glm::vec3& candidateVel,
                  float dt,
                  const PerceptionTuning& tuning);

// Produce the belief from a snapshot + memory. When nothing is visible, the
// memory provides a decaying pseudo-target; when memory expires, no target.
BeliefState buildBelief(const PerceptionSnapshot& snapshot,
                        const MemoryRecord& memory,
                        uint32_t targetActorId,
                        float predictionErrorMeters,
                        const PerceptionTuning& tuning);

// Skill-scaled prediction: last known position + velocity * horizon, with a
// deterministic per-NPC positional error so tracking is never perfect.
glm::vec3 predictTargetPosition(const glm::vec3& lastKnownPos,
                                const glm::vec3& lastKnownVel,
                                float horizonSeconds,
                                float errorMeters,
                                uint32_t npcId);

// Pure sight-cone test: is `targetPos` inside the horizontal FOV cone centered
// on `facing` (planar) at `origin`? Shared by perceive() and tests.
bool withinSightCone(const glm::vec3& origin,
                     const glm::vec3& facing,
                     const glm::vec3& targetPos,
                     float fovDegrees);

// World-independent selftest for the perception gates, memory decay, and
// prediction error. Returns true on success and fills `report`.
bool npcPerceptionSelfTest(std::string& report);
