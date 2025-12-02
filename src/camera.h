// C:\important\go away v5\s\mimita-v5\src\camera.h

#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

inline float CAMERA_DISTANCE = 4.0f;  // closer
inline float CAMERA_HEIGHT   = 1.5f;  // lower
inline float CAMERA_SENS     = 0.1f;
inline float CAMERA_FOV      = 90.0f; // todo add 0.01 and 359.9 capabilites
inline float CAMERA_SHOULDER_OFFSET = 1.0f;

class Camera {
public:
    glm::vec3 pos{0, CAMERA_HEIGHT, CAMERA_DISTANCE};
    glm::vec3 front{0, 0, -1};
    glm::vec3 up{0, 1, 0};
    glm::vec3 right{1, 0, 0};

    float yaw = -90.0f;
    float pitch = 10.0f;
    bool firstMouse = true;
    double lastX = 400, lastY = 300;

    void updateVectors();  // ✅ keep this line here
    void updateMouse(double xpos, double ypos);
    void follow(const glm::vec3& target);
    glm::mat4 getView() const;
    glm::mat4 getProj(float width, float height) const;
};
