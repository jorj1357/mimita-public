// C:\important\quiet\n\mimita-priv-v7\src\camera.cpp
// mar 6 2026 i think this works
// its just old

#include "camera.h"
#include "config/camera-config.h"
#include <algorithm>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include "combat/weapon-fire.h"
#include "physics/physics-types.h"
#include "physics/movement/physics-collision.h"
#include "world/world.h"
#include "debug/structured-log.h"

void Camera::updateMouse(double xpos, double ypos) {
    if (ignoreHostMouse) {
        firstMouse = true;
        return;
    }
    lastMouseInputTime = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }

    // Mouse-down is positive dy. Camera pitch is positive upward, so a
    // downward mouse movement must subtract pitch. This keeps the direct
    // camera path and the normalized InputRouter path on the same convention.
    float xoff = float(xpos - lastX);
    float yoff = float(ypos - lastY);
    lastX = xpos;
    lastY = ypos;

    xoff *= 0.15f;
    yoff *= 0.15f;

    yaw -= xoff;
    pitch -= yoff;

    // The regular mouse camera keeps its established pitch clamp. Head-world
    // mode needs an unbounded pitch wish so quaternion head aiming can pass
    // through vertical without snapping at the Euler pole.
    if (CamConfig::instance().effectiveData().orientationMode != "head_world") {
        pitch = glm::clamp(pitch, -89.9f, 89.9f);
    }

    updateVectors();
}

void Camera::follow(const glm::vec3& target, const glm::vec3& offset, float stiffness) {
    (void)stiffness;
    glm::vec3 right = glm::normalize(glm::cross(front, up));
    mDesiredPos = target
        + right * offset.x
        - front * (-offset.y)
        + glm::vec3(0, 0, offset.z);
}

void Camera::updateVectors() {
    float effectivePitch = pitch + punchPitch + weaponRecoilPitch;
    if (CamConfig::instance().effectiveData().orientationMode != "head_world")
        effectivePitch = glm::clamp(effectivePitch, -89.9f, 89.9f);
    float effectiveYaw = yaw + punchYaw + weaponRecoilYaw;

    // Compose world yaw and local pitch as quaternions. This remains
    // continuous when pitch passes through straight up or down.
    const glm::quat yawRotation = glm::angleAxis(
        glm::radians(effectiveYaw), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::quat pitchRotation = glm::angleAxis(
        glm::radians(effectivePitch), glm::vec3(0.0f, -1.0f, 0.0f));
    const glm::quat lookRotation = glm::normalize(yawRotation * pitchRotation);
    lookWishFront = glm::normalize(lookRotation * glm::vec3(1.0f, 0.0f, 0.0f));
    lookWishUp = glm::normalize(lookRotation * glm::vec3(0.0f, 0.0f, 1.0f));

    front = lookWishFront;
    up = lookWishUp;
    right = glm::normalize(glm::cross(front, up));
    if (glm::length(right) > 1e-4f)
        up = glm::normalize(glm::cross(right, front));
}

void Camera::decayPunch(float dt, float decayRate) {
    if (punchPitch > 0.0f)
        punchPitch = std::max(0.0f, punchPitch - dt * decayRate);
    else if (punchPitch < 0.0f)
        punchPitch = std::min(0.0f, punchPitch + dt * decayRate);

    if (punchYaw > 0.0f)
        punchYaw = std::max(0.0f, punchYaw - dt * decayRate);
    else if (punchYaw < 0.0f)
        punchYaw = std::min(0.0f, punchYaw + dt * decayRate);
}

void Camera::addPunch(float pitchAmount, float yawAmount) {
    punchPitch = glm::clamp(punchPitch + pitchAmount, -10.0f, 10.0f);
    punchYaw = glm::clamp(punchYaw + yawAmount, -6.0f, 6.0f);
}

void Camera::addWeaponRecoil(float pitchAmount, float yawAmount) {
    if (!std::isfinite(pitchAmount) || !std::isfinite(yawAmount)) return;
    weaponRecoilPitch += pitchAmount;
    weaponRecoilYaw += yawAmount;
    if (!std::isfinite(weaponRecoilPitch) || !std::isfinite(weaponRecoilYaw)) {
        weaponRecoilPitch = 0.0f;
        weaponRecoilYaw = 0.0f;
    }
}

void Camera::decayWeaponRecoil(float dt, float recoveryRate) {
    if (!std::isfinite(dt) || !std::isfinite(recoveryRate)) return;
    const float step = std::max(0.0f, dt) * std::max(0.0f, recoveryRate);
    if (weaponRecoilPitch > 0.0f) weaponRecoilPitch = std::max(0.0f, weaponRecoilPitch - step);
    else if (weaponRecoilPitch < 0.0f) weaponRecoilPitch = std::min(0.0f, weaponRecoilPitch + step);
    if (weaponRecoilYaw > 0.0f) weaponRecoilYaw = std::max(0.0f, weaponRecoilYaw - step);
    else if (weaponRecoilYaw < 0.0f) weaponRecoilYaw = std::min(0.0f, weaponRecoilYaw + step);
}

void Camera::addCameraSwayImpulse(float pitchImpulse, float rollImpulse) {
    if (!std::isfinite(pitchImpulse) || !std::isfinite(rollImpulse)) return;
    sway.velocity.x += pitchImpulse;
    sway.velocity.y += rollImpulse;
}

void Camera::updateCameraSway(float dt, float stiffness, float damping,
                              float maxPitch, float maxRoll) {
    dt = std::clamp(dt, 0.0f, 0.1f);
    stiffness = std::max(0.01f, stiffness);
    damping = std::max(0.0f, damping);
    const glm::vec2 acceleration = -sway.offset * stiffness - sway.velocity * damping;
    sway.velocity += acceleration * dt;
    sway.offset += sway.velocity * dt;
    sway.offset.x = glm::clamp(sway.offset.x, -std::abs(maxPitch), std::abs(maxPitch));
    sway.offset.y = glm::clamp(sway.offset.y, -std::abs(maxRoll), std::abs(maxRoll));
    if (!std::isfinite(sway.offset.x) || !std::isfinite(sway.offset.y) ||
        !std::isfinite(sway.velocity.x) || !std::isfinite(sway.velocity.y))
        sway = {};
}

glm::mat4 Camera::getView() const {
    const glm::vec3 swayedFront = glm::normalize(
        glm::angleAxis(glm::radians(sway.offset.x), right) * front);
    const glm::vec3 swayedUpBase = glm::normalize(
        glm::cross(right, swayedFront));
    const float totalRoll = roll + sway.offset.y;
    if (totalRoll != 0.0f) {
        glm::vec3 rolledUp = glm::normalize(
            glm::angleAxis(glm::radians(totalRoll), swayedFront) * swayedUpBase);
        return glm::lookAt(pos, pos + swayedFront, rolledUp);
    }
    return glm::lookAt(pos, pos + swayedFront, swayedUpBase);
}

glm::mat4 Camera::getProj(float width, float height) const {
    // Log projection params once per second
    static float logTimer = 0.0f;
    logTimer += 1.0f / 60.0f;
    if (logTimer >= 1.0f) {
        logTimer = 0.0f;
        DBG(Camera, "nearPlane=0.01 farPlane=5000.0 fov=%.0f", fov);
    }
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
        return glm::mat4(1.0f);
    const float aspect = width / height;
    if (!std::isfinite(aspect) || aspect <= 0.0f)
        return glm::mat4(1.0f);
    return glm::perspective(glm::radians(fov), aspect, 0.01f, 5000.0f);
}

void Camera::smoothCollision(
    const glm::vec3& playerPos,
    const World& world,
    float dt,
    float stiffness,
    bool stiffnessEnabled,
    bool collisionEnabled,
    bool collisionPushEnabled,
    float collisionPushback
) {
    // First frame: snap instantly
    if (mFirstFrame) {
        pos = mDesiredPos;
        mPrevCollisionPos = pos;
        mFirstFrame = false;
        return;
    }

    // FOLLOW POSITION — stiffness 1=rigid, 0=barely follows
    if (!stiffnessEnabled) {
        pos = mDesiredPos;
    } else {
        float followSpeed = 0.5f + stiffness * 19.5f;
        pos = glm::mix(pos, mDesiredPos, std::min(1.0f, followSpeed * dt));
    }

    // CAMERA COLLISION
    if (!collisionEnabled) {
        mPrevCollisionPos = pos;
        return;
    }

    glm::vec3 dir = pos - playerPos;
    float dist = glm::length(dir);

    if (dist < 0.01f) {
        mPrevCollisionPos = pos;
        return;
    }

    dir /= dist;

    float hitDist = dist;
    glm::vec3 hitNormal;
    bool hit = rayTraverseGridCells(world, playerPos, dir, dist, hitDist, &hitNormal);

    glm::vec3 targetPos = pos;

    if (hit && collisionPushEnabled && hitDist > 0.2f) {
        targetPos =
            playerPos +
            dir * std::max(hitDist - collisionPushback, collisionPushback);
    }

    if (!stiffnessEnabled) {
        pos = targetPos;
        mPrevCollisionPos = targetPos;
    } else {
        float smoothSpeed = hit ? 12.0f : 7.0f;
        mPrevCollisionPos = glm::mix(mPrevCollisionPos, targetPos, std::min(1.0f, smoothSpeed * dt));
        pos = mPrevCollisionPos;
    }
}

void Camera::applyBodycamPose(const glm::vec3& targetPosition,
                              const glm::vec3& targetFront,
                              const glm::vec3& targetUp,
                              const glm::vec3& collisionOrigin,
                              const World& world, float dt,
                              float positionResponseSeconds,
                              float rotationResponseSeconds,
                              float settleDeadbandDegrees,
                              float settlePositionMeters,
                              int settleHoldTicks,
                              bool allowSettle,
                              bool wallCollision, float probeRadiusMeters)
{
    const float posAlpha = 1.0f - std::exp(-std::max(0.0f, dt) /
        std::clamp(positionResponseSeconds, 0.01f, 2.0f));
    const float rotAlpha = 1.0f - std::exp(-std::max(0.0f, dt) /
        std::clamp(rotationResponseSeconds, 0.01f, 2.0f));
    if (!mBodycamPoseInitialized) {
        pos = targetPosition;
        front = glm::normalize(targetFront);
        up = glm::normalize(targetUp);
        right = glm::normalize(glm::cross(front, up));
        glm::mat4 basis(1.0f);
        basis[0] = glm::vec4(right, 0.0f);
        basis[1] = glm::vec4(front, 0.0f);
        basis[2] = glm::vec4(up, 0.0f);
        mBodycamRotation = glm::quat_cast(basis);
        mBodycamPoseInitialized = true;
        mBodycamSettleTicks = 0;
        mBodycamSettled = false;
    } else {
        const glm::vec3 safeTargetFront = glm::normalize(targetFront);
        const glm::vec3 safeTargetUp = glm::normalize(targetUp);
        const glm::vec3 targetRight = glm::normalize(
            glm::cross(safeTargetFront, safeTargetUp));
        glm::mat4 targetBasis(1.0f);
        targetBasis[0] = glm::vec4(targetRight, 0.0f);
        targetBasis[1] = glm::vec4(safeTargetFront, 0.0f);
        targetBasis[2] = glm::vec4(safeTargetUp, 0.0f);
        const glm::quat targetRotation = glm::quat_cast(targetBasis);
        const float rotationDot = std::fabs(glm::dot(
            glm::normalize(mBodycamRotation), glm::normalize(targetRotation)));
        const float rotationDelta = 2.0f * std::acos(
            glm::clamp(rotationDot, 0.0f, 1.0f));
        const float positionDelta = glm::length(targetPosition - pos);
        const bool withinSettle =
            allowSettle ||
            (rotationDelta <= glm::radians(std::max(0.0f, settleDeadbandDegrees)) &&
             positionDelta <= std::max(0.0f, settlePositionMeters));
        if (withinSettle) {
            ++mBodycamSettleTicks;
        } else {
            mBodycamSettleTicks = 0;
        }
        const int holdTicks = std::max(1, settleHoldTicks);
        if (mBodycamSettleTicks >= holdTicks) {
            if (!mBodycamSettled) {
                mBodycamSettled = true;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Camera, StructuredLevel::Important,
                    "camera.bodycam.settled", "bodycam",
                    "bodycam camera held still inside the settle deadband", 0,
                    nlohmann::json{
                        {"rotation_delta_degrees", glm::degrees(rotationDelta)},
                        {"position_delta_meters", positionDelta},
                        {"settle_deadband_degrees", settleDeadbandDegrees},
                        {"settle_position_meters", settlePositionMeters},
                        {"hold_ticks", holdTicks}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            // Keep the last rendered pose. This removes residual head/rigid-body
            // micro-motion when the player is not actually aiming or moving.
        } else {
            if (mBodycamSettled) {
                mBodycamSettled = false;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Camera, StructuredLevel::Important,
                    "camera.bodycam.resumed", "bodycam",
                    "bodycam camera resumed following the head", 0,
                    nlohmann::json{
                        {"rotation_delta_degrees", glm::degrees(rotationDelta)},
                        {"position_delta_meters", positionDelta}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            pos = glm::mix(pos, targetPosition, posAlpha);
            glm::vec3 currentRight = glm::normalize(glm::cross(front, up));
            glm::mat4 currentBasis(1.0f);
            currentBasis[0] = glm::vec4(currentRight, 0.0f);
            currentBasis[1] = glm::vec4(glm::normalize(front), 0.0f);
            currentBasis[2] = glm::vec4(glm::normalize(up), 0.0f);
            mBodycamRotation = glm::slerp(glm::quat_cast(currentBasis),
                targetRotation, rotAlpha);
            right = glm::normalize(mBodycamRotation * glm::vec3(1, 0, 0));
            front = glm::normalize(mBodycamRotation * glm::vec3(0, 1, 0));
            up = glm::normalize(mBodycamRotation * glm::vec3(0, 0, 1));
        }
    }

    if (wallCollision) {
        glm::vec3 ray = pos - collisionOrigin;
        const float distance = glm::length(ray);
        if (distance > 0.001f) {
            ray /= distance;
            float hitDistance = distance;
            if (rayTraverseGridCells(world, collisionOrigin, ray, distance,
                                     hitDistance, nullptr) && hitDistance < distance) {
                pos = collisionOrigin + ray * std::max(0.02f,
                    hitDistance - std::clamp(probeRadiusMeters, 0.02f, 0.5f));
            }
        }
    }
}
