// C:\important\go away v5\s\mimita-v5\src\camera.h

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

/**
 * dec 3 2025 todo backup
 * inline float CAMERA_DISTANCE = 4.0f;  // closer
inline float CAMERA_HEIGHT   = 1.5f;  // lower
inline float CAMERA_SENS     = 0.1f;
inline float CAMERA_FOV      = 90.0f; // todo add 0.01 and 359.9 capabilites
inline float CAMERA_SHOULDER_OFFSET = 1.0f;

 */

// dec 3 2025 values v2
/**
 * 
 * inline float CAMERA_DISTANCE = 4.0f;  // closer
inline float CAMERA_HEIGHT   = 2.5f;  // higher
inline float CAMERA_SENS     = 0.12f; // a little fast
inline float CAMERA_FOV      = 90.0f; // todo add 0.01 and 359.9 capabilites
inline float CAMERA_SHOULDER_OFFSET = 1.2f; // a lil further 
 */

// dec 3 2025 values v3
// dec 19 2025 move these to config.h todo 
inline float CAMERA_DISTANCE = 4.0f;  // closer
inline float CAMERA_HEIGHT   = 3.5f;  // higher
inline float CAMERA_SENS     = 0.15f; // a little fast
inline float CAMERA_FOV      = 90.0f; // todo add 0.01 and 359.9 capabilites
inline float CAMERA_SHOULDER_OFFSET = 1.5f; // a lil further 
 
class Camera {
public:
    // z is up dec 19 2025 
    glm::vec3 pos{0, -CAMERA_DISTANCE, CAMERA_HEIGHT};
    glm::vec3 front{0, 1, 0};
    glm::vec3 up{0, 0, 1};
    glm::vec3 right{1, 0, 0};

    float yaw = -90.0f;
    float pitch = 10.0f;
    bool firstMouse = true;
    // dec 19 2025 make this work with main.cpp window dimensions 
    double lastX = 400, lastY = 300;

    void updateVectors(); 
    void updateMouse(double xpos, double ypos);
    void follow(const glm::vec3& target);
    glm::mat4 getView() const;
    glm::mat4 getProj(float width, float height) const;
};
