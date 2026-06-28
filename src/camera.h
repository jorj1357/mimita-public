// C:\important\go away v5\s\mimita-v5\src\camera.h

#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

struct CollisionTriangle;

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
// inline float CAMERA_DISTANCE = 4.0f;  // closer
// inline float CAMERA_HEIGHT   = 3.5f;  // higher
// inline float CAMERA_SENS     = 0.15f; // a little fast
// inline float CAMERA_FOV      = 90.0f; // todo add 0.01 and 359.9 capabilites
// inline float CAMERA_SHOULDER_OFFSET = 1.5f; // a lil further 

// jan 30 2026 values v4 
// for debugging also we're a caspule for now so thats whi 
inline float CAMERA_DISTANCE = 3.5f;
inline float CAMERA_HEIGHT   = 1.0f;
inline float CAMERA_SENS     = 0.15f;
inline float CAMERA_FOV      = 100.0f;
inline float CAMERA_SHOULDER_OFFSET = 2.0f;


class Camera {
public:
// z is up dec 19 2025 
glm::vec3 pos{0, -CAMERA_DISTANCE, CAMERA_HEIGHT};
glm::vec3 front{0, 1, 0};
glm::vec3 up{0, 0, 1};
glm::vec3 right{1, 0, 0};

float yaw = -90.0f;
float pitch = 10.0f;
float punchPitch = 0.0f;
float punchYaw = 0.0f;
float fov = CAMERA_FOV;
bool firstMouse = true;
// dec 19 2025 make this work with main.cpp window dimensions 
double lastX = 400, lastY = 300;

    bool thirdPerson = true;

    void updateVectors(); 
    void decayPunch(float dt);
    void addPunch(float pitchAmount, float yawAmount);
    void updateMouse(double xpos, double ypos);
    void follow(const glm::vec3& target);
    void smoothCollision(const glm::vec3& playerPos, const std::vector<CollisionTriangle>& triangles, float dt);
    glm::mat4 getView() const;
    glm::mat4 getProj(float width, float height) const;

    // 0 = locked/snappy, 5 = default, 10 = floaty
    float smoothness = 0.0f;

private:
    glm::vec3 mPrevCollisionPos{0.0f};
    glm::vec3 mDesiredPos{0.0f};
    bool mFirstFrame = true;
};
