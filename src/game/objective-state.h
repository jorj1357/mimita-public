// 10 02 2026
/* purpose
* Generalize mode objective items (bomb today; future payload/capture/escort).
* An objective has an id, kind, world transform, carrier, state, allowed
* carrier team, pickup/interaction radii, and an optional explosion deadline.
* The server owns all state; clients only render replicated values.
* Does NOT live inside an ActorPreset (objectives are mode rules, not actor
* configuration). Does NOT plant/defuse/explode - that is Stage 12 logic.
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

enum class ObjectiveKind : uint8_t
{
    None = 0,
    Bomb = 1
};

enum class ObjectiveState : uint8_t
{
    Inactive = 0,   // not in play
    Carried,        // held by a carrier actor
    Dropped,        // lying in the world
    Planted,        // placed at a site and armed (Stage 12)
    Defused,
    Exploded
};

// Generic objective instance owned by the server. One per active objective.
struct ObjectiveInstance
{
    std::string id;                 // e.g. "bomb"
    ObjectiveKind kind = ObjectiveKind::None;
    ObjectiveState state = ObjectiveState::Inactive;
    glm::vec3 position{0.0f};       // world transform (dropped/planted)
    uint32_t carrierActorId = 0;    // 0 = none
    int allowedCarrierTeam = -1;    // -1 = any; else fixed team index
    float pickupRadius = 1.5f;      // automatic proximity pickup
    float interactionRange = 3.0f;  // F-interaction range (Stage 12)
    float explosionSeconds = 0.0f;  // arm/explosion timer (Stage 12)
    uint32_t explosionDeadlineTick = 0;
    bool active = false;

    // ── Plant / defuse progress (Stage 12) ──────────────────────────
    // Fixed-tick progress. It is interruptible: leaving the site/range or
    // losing the required state resets progress without completing.
    int plantTicksRequired = 0;
    int plantTicksElapsed = 0;
    int defuseTicksRequired = 0;
    int defuseTicksElapsed = 0;
    uint32_t planterActorId = 0;
    uint32_t defuserActorId = 0;
    std::string plantedSiteId;

    bool valid() const { return active && kind != ObjectiveKind::None; }
    bool isPlanted() const { return state == ObjectiveState::Planted; }
};

// Resolve an objective kind from a JSON string ("bomb"). Unknown -> None.
ObjectiveKind objectiveKindFromString(const std::string& kind);

// One candidate carrier considered for pickup/assignment.
struct ObjectiveCarrierCandidate
{
    uint32_t actorId = 0;
    glm::vec3 position{0.0f};
    int team = -1;
    bool dead = false;
};

// Pick the nearest living candidate eligible to carry the objective (team gate
// + within `radiusMeters`). Returns 0 when none qualify. Pure.
uint32_t selectObjectiveCarrier(const ObjectiveInstance& obj,
                               const ObjectiveCarrierCandidate* candidates,
                               int candidateCount,
                               float radiusMeters);

// Advance a fixed-tick progress counter. Returns true when it just completed.
// `canProgress` false resets progress to zero (interruptible). Pure.
bool advanceObjectiveProgress(int& elapsed, int required, bool canProgress);

// Convert seconds to fixed 60 Hz ticks, clamped to at least 1.
int objectiveSecondsToTicks(float seconds);

// World-independent selftest for objective state transitions and team gating.
bool objectiveSelfTest(std::string& report);
