// 09 12 2026
/* purpose
* Generic physics constraint primitive shared by ragdolls, corpses, persistent
* objects, and future vehicles/VR hands. A constraint references bodies by
* stable ids (or the world) with local anchors and strength/limits, so no
* subsystem needs a bespoke grab/cross-body special case.
* Does NOT own body state, the ECS, input, or networking.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace Physics {

// Sentinel body id meaning "static world anchor" in bodyB.
inline constexpr std::uint32_t kWorldBody = 0xffffffffu;

enum class ConstraintType : std::uint8_t {
    Point,     // anchors coincide
    Distance,  // |anchorB-anchorA| kept >= minDistance and <= maxDistance
    Grab       // a hand anchor pinned to a world point or another body's anchor
};

// Value form used by the solver (no ECS dependency).
struct Constraint {
    ConstraintType type = ConstraintType::Grab;
    bool active = true;
    std::uint32_t bodyA = kWorldBody;
    std::int32_t limbA = -1;
    glm::vec3 anchorA{0.0f};
    std::uint32_t bodyB = kWorldBody;
    std::int32_t limbB = -1;
    glm::vec3 anchorB{0.0f};
    glm::vec3 worldPoint{0.0f};
    float strength = 1.0f;
    float damping = 1.0f;
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
};

// ECS storage form. `ownerActor` + `createdTick` support replication/late join.
struct ConstraintComponent {
    Constraint constraint;
    std::uint32_t constraintSerial = 0;
    std::uint32_t ownerActor = 0;
    std::uint32_t createdTick = 0;
    std::uint32_t releaseTick = 0;
    bool released = false;
};

} // namespace Physics
