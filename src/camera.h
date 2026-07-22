// C:\important\go away v5\s\mimita-v5\src\camera.h

#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

struct World;

inline float CAMERA_SENS = 0.15f;

class Camera {
public:
// z is up dec 19 2025 
glm::vec3 pos{0, 0, 0};
glm::vec3 front{0, 1, 0};
glm::vec3 up{0, 0, 1};
glm::vec3 right{1, 0, 0};

float yaw = -90.0f;
float pitch = 10.0f;
float punchPitch = 0.0f;
float punchYaw = 0.0f;
float fov = 100.0f;
float roll = 0.0f;
bool firstMouse = true;
// dec 19 2025 make this work with main.cpp window dimensions 
double lastX = 400, lastY = 300;

    bool thirdPerson = true;

    void updateVectors(); 
    void decayPunch(float dt);
    void addPunch(float pitchAmount, float yawAmount);
    void updateMouse(double xpos, double ypos);
    void follow(const glm::vec3& target, const glm::vec3& offset, float stiffness);
    void smoothCollision(const glm::vec3& playerPos, const World& world, float dt, float stiffness, bool stiffnessEnabled, bool collisionEnabled, bool collisionPushEnabled = true, float collisionPushback = 0.3f);
    glm::mat4 getView() const;
    glm::mat4 getProj(float width, float height) const;

private:
    glm::vec3 mPrevCollisionPos{0.0f};
    glm::vec3 mDesiredPos{0.0f};
    bool mFirstFrame = true;
};
