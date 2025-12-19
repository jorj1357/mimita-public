// C:\important\go away v5\s\mimita-v5\src\camera.cpp

#include "camera.h"
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
    front = glm::normalize(glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch))
    ));

    right = glm::normalize(glm::cross(front, glm::vec3(0,0,1)));
    up    = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::getView() const {
    return glm::lookAt(pos, pos + front, up);
}

glm::mat4 Camera::getProj(float width, float height) const {
    return glm::perspective(glm::radians(CAMERA_FOV), width / height, 0.1f, 500.0f);
}
