// C:\important\go away v5\s\mimita-v5\src\camera.h

#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

struct CollisionTriangle;

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
float roll = 0.0f;
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
