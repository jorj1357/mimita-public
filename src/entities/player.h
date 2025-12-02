// C:\important\go away v5\s\mimita-v5\src\entities\player.h

#pragma once
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <GLFW/glfw3.h>

class Camera;

// dec 2 2025 i think we should have a single file/folder that controls
// all player data and health and size and blah balh
inline float PLAYER_WIDTH  = 1.0f;
inline float PLAYER_HEIGHT = 2.0f;
inline float PLAYER_DEPTH  = 0.5f;

// dec 2 2025 todo where put this ???
float yaw = 0.0f;
OBB getOBB() const;

// idk where put nov 12 2025 todo
struct OBB {
    glm::vec3 center;
    glm::vec3 halfSize;
    glm::mat4 orientation;
};

class Player {
public:
    // -------- Core State --------
    glm::vec3 pos{0, 2, 0};
    glm::vec3 vel{0};
    bool onGround = false;

    // -------- Hitbox Config --------
    glm::vec3 hitboxSize   = {0.8f, 1.8f, 0.8f};   // width, height, depth
    glm::vec3 hitboxOffset = {0.0f, 0.0f, 0.0f};   // center offset

    // -------- Mesh Config --------
    glm::vec3 meshScale  = {1.0f, 1.0f, 1.0f};
    glm::vec3 meshOffset = {0.0f, -0.9f, 0.0f};    // shift mesh within hitbox

    // -------- Camera Config --------
    glm::vec3 cameraOffset = {0.0f, 0.6f, 2.5f};

    // -------- Derived Helpers --------
    inline glm::vec3 halfExtents() const { return hitboxSize * 0.5f; }
    inline glm::mat4 getRootTransform(float yaw) const {
        glm::mat4 root = glm::mat4(1.0f);
        root = glm::translate(root, pos - glm::vec3(0, hitboxSize.y * 0.5f, 0));
        root = glm::rotate(root, glm::radians(-yaw), glm::vec3(0, 1, 0));
        return root;
    }

    glm::mat4 rootTransform = glm::mat4(1.0f);

    Player();
    void reset();
    void jump(float strength);
    void applyGravity(float gravity, float dt);
    void move(const glm::vec3& dir, float speed, float dt);
    void render(GLuint shaderProgram, GLuint vao, int vertCount,
                const glm::mat4& view, const glm::mat4& proj,
                const Camera& camera, GLuint tex);
};
