// C:\important\quiet\n\mimita-priv-v7\src\camera.cpp
// mar 6 2026 i think this works
// its just old

#include "camera.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void Camera::updateMouse(double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }

    float xoff = float(xpos - lastX);
    float yoff = float(lastY - ypos);
    lastX = xpos;
    lastY = ypos;

    xoff *= CAMERA_SENS;
    yoff *= CAMERA_SENS;

    // this makes me look left or right dec192025 ? 
    yaw -= xoff;
    pitch += yoff;

    // Expanded pitch range
    if (pitch > 89.9f) pitch = 89.9f;
    if (pitch < -89.9f) pitch = -89.9f;

    glm::vec3 dir;
    // dec 19 2025 z is up now 
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.z = sin(glm::radians(pitch));
    front = glm::normalize(dir);
}

void Camera::follow(const glm::vec3& target) {
    // Calculate right vector from camera front and up
    glm::vec3 right = glm::normalize(glm::cross(front, up));

    // Set camera position behind and slightly right of the target
    // z is up dec 19 2025 
    pos = target
        - front * CAMERA_DISTANCE
        + glm::vec3(0, 0, CAMERA_HEIGHT)
        + right * CAMERA_SHOULDER_OFFSET;
}

void Camera::updateVectors() {
    float effectivePitch = glm::clamp(pitch + punchPitch, -89.9f, 89.9f);
    float effectiveYaw = yaw + punchYaw;

    front = glm::normalize(glm::vec3(
        cos(glm::radians(effectiveYaw)) * cos(glm::radians(effectivePitch)),
        sin(glm::radians(effectiveYaw)) * cos(glm::radians(effectivePitch)),
        sin(glm::radians(effectivePitch))
    ));

    right = glm::normalize(glm::cross(front, glm::vec3(0,0,1)));
    up    = glm::normalize(glm::cross(right, front));
}

void Camera::decayPunch(float dt) {
    const float decayRate = 12.0f;
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

glm::mat4 Camera::getView() const {
    return glm::lookAt(pos, pos + front, up);
}

glm::mat4 Camera::getProj(float width, float height) const {
    return glm::perspective(glm::radians(CAMERA_FOV), width / height, 0.1f, 500.0f);
}
