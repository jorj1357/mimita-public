// 09 12 2026
/* purpose
* Define ragdoll entity/component data: one entity per limb plus joint, grab,
* root, and solver-policy components. Persistent state lives in the registry,
* not in the RagdollModeSystem singleton.
* Does NOT own the solver or rendering.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace Ragdoll {

inline constexpr std::uint32_t kInvalidLimb = 0xffffffffu;

struct LimbComponent {
    std::uint32_t limbIndex = 0;
    std::uint32_t parentIndex = kInvalidLimb;
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 linearVelocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float mass = 1.0f;
    float radius = 0.1f;
    float halfHeight = 0.0f;
    float inverseMass = 1.0f;
};

struct JointComponent {
    std::uint32_t limbIndex = 0;
    std::uint32_t parentLimb = kInvalidLimb;
    glm::vec3 parentLocalAnchor{0.0f};
    glm::vec3 childLocalAnchor{0.0f};
    float restLength = 0.0f;
    float maxStretch = 0.0f;
    float stiffness = 1.0f;
    float damping = 1.0f;
    float positionBeta = 0.2f;
    float coneLimitDeg = 0.0f;
};

struct GrabComponent {
    bool active = false;
    bool wasActive = false;
    std::uint32_t hand = 0;  // 0 = left, 1 = right
    // Limb entity performing the grab (kInvalidEntityId when inactive).
    std::uint32_t limbEntity = 0xffffffffu;
    // World anchor when grabbing static geometry.
    glm::vec3 grabPoint{0.0f};
    glm::vec3 grabNormal{0.0f};
    glm::vec3 handPosition{0.0f};
    glm::vec3 handLocalAnchor{0.0f};
    // Entity-to-entity constraint target. 0xffffffff = world anchor only.
    std::uint32_t targetEntity = 0xffffffffu;
    std::uint32_t grabbedActorId = 0;
    float strength = 1.0f;
};

struct RagdollRootComponent {
    std::uint32_t ownerActorId = 0;
    std::uint32_t limbCount = 0;
    std::uint32_t solverIterations = 24;
    float gravityScale = 1.0f;
    float stiffness = 1.0f;
    float damping = 1.0f;
    bool alive = false;
    bool corpse = false;
    std::uint64_t lastSolveTick = 0;
};

// Live solver policy for one ragdoll. Base values come from the root component
// / config; a hot gameplay module may override them per solve.
struct SolveParams {
    float stiffness = 1.0f;
    float damping = 1.0f;
    int iterations = 24;
    float gravityScale = 1.0f;
};

} // namespace Ragdoll
