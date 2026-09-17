// 09 17 2026
/* purpose
* Hot alive-ragdoll motor policy. This module owns the camera-driven aim
* response for the head and torso; the cold orchestrator only snapshots the
* limb orientations/angular velocities and applies the results. It is fully
* live-editable: edit and save and the running client's alive-ragdoll aim
* changes on the next generation switch, with no EXE rebuild.
* The aim is a damped velocity controller (no overshoot): a desired angular
* velocity is derived from the rotation error to the camera look basis and
* blended into the limb's angular velocity, clamped to the configured max
* speed. Gravity, joints and grabs stay in the `ragdoll.solve` seam.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

// Live tuning (edit these; no restart).
struct Tuning {
    float aimDeadband = 1e-4f;   // axis-length epsilon
    float upParallelEps = 0.05f; // when the look basis degenerates, use camera up
};
constexpr Tuning kTune{};

// Build an orientation quaternion from a look direction using the ragdoll
// convention: local +Y = forward, local +Z = up, local +X = right.
glm::quat lookRotation(glm::vec3 forward, glm::vec3 up)
{
    forward = glm::normalize(forward);
    up = glm::normalize(up);

    glm::vec3 right = glm::cross(forward, up);
    if (glm::length(right) < 0.001f)
        right = glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f));
    right = glm::normalize(right);
    up = glm::normalize(glm::cross(right, forward));

    glm::mat3 m(right, forward, up);
    return glm::normalize(glm::quat_cast(m));
}

// Physically rotate a limb toward a target orientation with a damped velocity
// controller. `angularVelocity` is updated in place.
void aimAt(glm::vec3& angularVelocity, const glm::quat& orientation,
           const glm::quat& aimOffset, const glm::quat& lookRot,
           float strength, float maxSpeed, float lookDamping, float dt)
{
    const glm::quat target = lookRot * aimOffset;
    const glm::quat diff = glm::normalize(target * glm::inverse(orientation));

    const float w = glm::clamp(diff.w, -1.0f, 1.0f);
    const float angle = 2.0f * std::acos(std::fabs(w));
    const float s = std::sqrt(std::max(0.0f, 1.0f - w * w));
    glm::vec3 axis = (s > kTune.aimDeadband)
        ? glm::vec3(diff.x, diff.y, diff.z) / s
        : glm::vec3(0.0f, 0.0f, 1.0f);
    if (w < 0.0f)
        axis = -axis;

    const float desiredSpeed = glm::clamp(angle * strength, -maxSpeed, maxSpeed);
    const glm::vec3 desiredVel = axis * desiredSpeed;
    const float blend = glm::clamp(dt * lookDamping, 0.0f, 1.0f);
    angularVelocity += (desiredVel - angularVelocity) * blend;

    const float spd = glm::length(angularVelocity);
    if (spd > maxSpeed && spd > 0.0f)
        angularVelocity *= maxSpeed / spd;
}

void ragdollAimProvider(void* host, GameRagdollAimV1* a)
{
    (void)host;
    if (!a)
        return;
    a->handled = 1;

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    const glm::vec3 front(a->cameraFront[0], a->cameraFront[1], a->cameraFront[2]);
    if (glm::length(glm::cross(front, up)) < kTune.upParallelEps)
        up = glm::vec3(a->cameraUp[0], a->cameraUp[1], a->cameraUp[2]);
    const glm::quat lookRot = lookRotation(front, up);

    auto doLimb = [&](std::uint32_t idx, float strength, float maxSpeed) {
        if (idx >= a->limbCount || idx >= GAME_MAX_RAGDOLL_LIMBS)
            return;
        GameRagdollAimLimbV1& l = a->limbs[idx];
        const glm::quat orient(l.orientation[0], l.orientation[1],
                               l.orientation[2], l.orientation[3]);
        const glm::quat aim(l.aimOffset[0], l.aimOffset[1],
                            l.aimOffset[2], l.aimOffset[3]);
        glm::vec3 ang(l.angularVelocity[0], l.angularVelocity[1], l.angularVelocity[2]);
        aimAt(ang, orient, aim, lookRot, strength, maxSpeed, a->lookDamping, a->dt);
        l.angularVelocity[0] = ang.x;
        l.angularVelocity[1] = ang.y;
        l.angularVelocity[2] = ang.z;
    };

    doLimb(a->headIndex, a->headStrength, a->headMaxSpeed);
    doLimb(a->torsoIndex, a->torsoStrength, a->torsoMaxSpeed);
}

const MimitaHotPackage::CapabilityRegistrar s_ragdollAimProvider{
    {GAME_CAP_RAGDOLL_AIM, gameHash("sig.ragdoll.aim.v1"), 0,
     reinterpret_cast<void*>(&ragdollAimProvider), "ragdoll.aim"}};

} // namespace

#endif
