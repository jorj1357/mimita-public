// C:\important\go away v5\s\mimita-v5\src\entities\player.h

#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <GLFW/glfw3.h>

class Camera;

// ------------- Oriented Bounding Box -------------
struct OBB {
    glm::vec3 center;
    glm::vec3 halfSize;
    glm::mat4 orientation;
};

// ------------- Player Definition -----------------
class Player {
public:
    // Core state
    glm::vec3 pos{0, 2, 0};
    glm::vec3 vel{0};
    bool onGround = false;

    // Orientation
    float yaw = 0.0f;   // <-- THIS was missing

    // Hitbox
    // dec 2 2025 this size shouldddd be like robloxian
    // head torso and legs are solid but arms arent
    glm::vec3 hitboxSize = {1.0f, 1.8f, 0.35f}; 
    glm::vec3 hitboxOffset = {0.0f, 0.0f, 0.0f};

    // Mesh
    glm::vec3 meshScale  = {1,1,1};
    glm::vec3 meshOffset = {0.0f, -0.9f, 0.0f};

    // Camera follow
    glm::vec3 cameraOffset = {0, 0.6f, 2.5f};

    // Helpers
    inline glm::vec3 halfExtents() const { return hitboxSize * 0.5f; }

    glm::mat4 rootTransform = glm::mat4(1.0f);

    // REQUIRED NEW FUNCTION
    OBB getOBB() const;

    // Methods
    Player();
    void reset();
    void jump(float strength);
    void applyGravity(float gravity, float dt);
    void move(const glm::vec3& dir, float speed, float dt);
    void render(GLuint shaderProgram, GLuint vao, int vertCount,
                const glm::mat4& view, const glm::mat4& proj,
                const Camera& camera, GLuint tex);
};
