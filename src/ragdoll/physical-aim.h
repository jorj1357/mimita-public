// 09 29 2026
/* purpose
* Own the pure aim controller math shared by the ragdoll look motor and the
* normal-play physical aim body: turn a desired orientation into a torque.
* Header-only and free of engine/transport/render dependencies so it can be
* unit tested without linking the game.
* Does NOT own config parsing, body state, joints, collision, or rendering.
*/
#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Which damping model the aim controller uses.
enum class PhysicalAimDamping {
    // Existing behavior: blend angular velocity toward the desired velocity at
    // a fixed look-damping rate (no overshoot).
    Look,
    // Torque/PD model: orientation error -> desired angular velocity -> torque.
    Physical,
};

struct PhysicalAimConfig {
    // Orientation error (radians) -> desired angular velocity (rad/s).
    float torqueGain = 28.0f;
    // Angular velocity damping applied inside the PD controller (1/s).
    float angularDamping = 5.0f;
    // Cap on the desired angular velocity produced by the orientation error.
    float maxAngularSpeed = 18.0f;
    // Per-part motor weights.
    float headWeight = 1.0f;
    float torsoWeight = 0.65f;
    // Fraction of the torso's inherited velocity kept when physical aim starts.
    float limbInheritance = 1.0f;
    // How tightly the physical torso follows the movement root (1/s). Higher is
    // stiffer; the editable kinematic tether.
    float torsoTetherStiffness = 40.0f;
    // Velocity blend rate for the Look damping model (matches ragdoll.json
    // look_damping when left at the default).
    float lookDamping = 12.0f;
    PhysicalAimDamping damping = PhysicalAimDamping::Physical;

    // Per-limb range of motion. The torso is clamped relative to the movement
    // root (pitch/roll); child limbs are clamped by swing magnitude relative to
    // their bind orientation. Degrees.
    float torsoMaxPitchDeg = 35.0f;
    float torsoMaxRollDeg = 25.0f;
    float headMaxSwingDeg = 55.0f;
    float armMaxSwingDeg = 80.0f;
    float legMaxSwingDeg = 45.0f;

    // Hybrid (animation-following) tracking. All hybrid pose following is a
    // stable exponential blend, so any follow force is safe. The effective
    // tracking rate is baseRate * followForce (1/s): 1.0 is baseline, 10.0
    // tracks the animation pose ten times harder. Higher = limbs look like the
    // default animated pose even at high speed; lower = more sway and momentum.
    float hybridFollowForce = 1.0f;
    float hybridBaseRate = 12.0f;
    // Extra multiplier on the two arms' tracking rate. 1.0 = same as the rest of
    // the body; higher makes the arms stick much harder to their aimbody /
    // animation orientation so a fast-moving player's weapon does not lag and
    // aim wrong (moving left making the gun point right).
    float hybridArmsFollowForce = 1.0f;
    // 0 = orientation-only following, 1 = limbs also follow their animated
    // position. Multiplies the position blend fraction.
    float hybridPositionFollow = 1.0f;
};

// Local +Y = forward, +Z = up, +X = right.
inline glm::quat aimLookRotation(glm::vec3 forward, glm::vec3 up)
{
    forward = glm::normalize(forward);
    up = glm::normalize(up);

    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f));
    right = glm::normalize(right);

    glm::vec3 trueUp = glm::normalize(glm::cross(right, forward));
    glm::mat3 basis(right, forward, trueUp);
    return glm::normalize(glm::quat_cast(basis));
}

// Shortest world-space rotation vector (axis * angle) taking current -> desired.
inline glm::vec3 rotationErrorVector(const glm::quat& current, const glm::quat& desired)
{
    glm::quat error = glm::normalize(desired * glm::inverse(current));
    if (error.w < 0.0f)
        error = -error;

    const float w = glm::clamp(error.w, -1.0f, 1.0f);
    const float angle = 2.0f * std::acos(w);
    const float sinHalfAngle = std::sqrt(std::max(0.0f, 1.0f - w * w));
    if (sinHalfAngle < 1e-4f)
        return glm::vec3(0.0f);

    return glm::vec3(error.x, error.y, error.z) / sinHalfAngle * angle;
}

// Desired angular velocity from the orientation error, clamped to the cap.
inline glm::vec3 aimDesiredAngularVelocity(const glm::quat& current,
                                           const glm::quat& desired,
                                           const PhysicalAimConfig& cfg,
                                           float weight)
{
    glm::vec3 desiredVel = rotationErrorVector(current, desired) * cfg.torqueGain * weight;
    const float speed = glm::length(desiredVel);
    if (speed > cfg.maxAngularSpeed && speed > 0.0f)
        desiredVel *= cfg.maxAngularSpeed / speed;
    return desiredVel;
}

// Torque (angular acceleration) that drives angularVelocity toward the desired
// angular velocity. Apply with angularVelocity += torque * dt. No orientation is
// ever assigned directly; callers integrate the physical state.
inline glm::vec3 computeAimTorque(const glm::quat& current,
                                  const glm::quat& desired,
                                  const glm::vec3& angularVelocity,
                                  const PhysicalAimConfig& cfg,
                                  float weight)
{
    const glm::vec3 desiredVel =
        aimDesiredAngularVelocity(current, desired, cfg, weight);
    return (desiredVel - angularVelocity) * cfg.torqueGain
         - angularVelocity * cfg.angularDamping;
}
