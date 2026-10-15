// 2026 10 15
/* purpose
* Own the single shared roll-movement kernel: one force/gravity/speed formula
* and one contact cleanup for every force-driven rolling body. Client fixed-tick
* prediction and any future server simulation call the same functions so the
* actor has one movement meaning instead of separate per-path formulas.
* See docs/specs/griever-monster/griever-monster.md (Slice 1) and 5.2.
* Does NOT read camera state, avatar names, network transport, or GLB nodes:
* callers pass a resolved intent direction and the resolved AvatarMovementCap.
* Does NOT run collision; the shared collision owner runs before cleanup.
* Does NOT own animation clips or spike damage.
*/
#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "entities/avatar-rig.h"

// A normalized (or zero) 3D intent direction. Player control derives it from
// the full camera basis including pitch; navigation passes a route direction.
struct RollIntent {
    glm::vec3 direction{0.0f};
};

// Vertical contact cleanup bounds. A rolling body must never keep upward speed
// while it is touching the world, and tiny residual vertical motion is killed
// so a resting ball does not jitter.
struct RollContactTuning {
    float restEpsilon = 0.3f;
    float maxUpSpeed = 40.0f;
    float maxDownSpeed = 80.0f;
};

// Pure velocity integration:
//   acceleration = force / mass along the intent,
//   velocity.z   += gravity * gravity_scale * dt   (gravity is signed; negative down),
//   then an optional max speed clamp.
// `gravity` is the same signed world gravity the walk/jump gravity owner uses.
inline glm::vec3 rollIntegrateVelocity(const glm::vec3& velocity,
                                       const RollIntent& intent,
                                       const AvatarMovementCap& cap,
                                       float gravity,
                                       float dt)
{
    const float invMass = 1.0f / std::max(cap.mass, 0.01f);
    glm::vec3 v = velocity + intent.direction * (cap.force * invMass) * dt;
    v.z += gravity * cap.gravityScale * dt;

    const float speed = glm::length(v);
    if (cap.maxSpeed > 0.0f && speed > cap.maxSpeed)
        v *= cap.maxSpeed / speed;
    return v;
}

// Contact cleanup shared by every roll path. `hasContact` is true when the
// collision step reported at least one world/actor contact this tick. It kills
// contact hover and bounds vertical speed; horizontal momentum is untouched.
inline void rollCleanupContactVelocity(glm::vec3& velocity,
                                       bool hasContact,
                                       const RollContactTuning& tuning = {})
{
    if (hasContact && velocity.z > 0.0f)
        velocity.z = 0.0f;
    if (std::fabs(velocity.z) < tuning.restEpsilon)
        velocity.z = 0.0f;
    velocity.z = std::clamp(velocity.z, -tuning.maxDownSpeed, tuning.maxUpSpeed);
}

// Root rolling rotation derived from horizontal travel and radius. Presentation
// only: it never feeds back into position or velocity. Returns the incremental
// quaternion to compose with the current root rotation.
inline glm::quat rollRotationDelta(const glm::vec3& horizontalVelocity,
                                   float radius,
                                   float dt)
{
    const float speed = std::sqrt(horizontalVelocity.x * horizontalVelocity.x +
                                  horizontalVelocity.y * horizontalVelocity.y);
    if (speed <= 0.05f)
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    const float r = std::max(radius, 0.1f);
    const glm::vec3 axis = glm::normalize(
        glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), horizontalVelocity));
    return glm::angleAxis(speed * dt / r, axis);
}
