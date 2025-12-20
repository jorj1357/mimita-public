// C:\important\go away v5\s\mimita-v5\src\entities\player.h

#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <GLFW/glfw3.h>

// dec 19 2025 todo cleanthis file from haivng so much comments
// also
/**
 * // Coordinate system:
// X = right
// Y = forward
// Z = up
// All height, gravity, jump, offsets use Z
dec 19 2025 update 
 */

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
    // do we use this even? dec 19 2025 todo 
    // z = up
    glm::vec3 pos{0, 0, 2};
    glm::vec3 vel{0};
    bool onGround = false;

    // Orientation
    float yaw = 0.0f;  

    // dont fall thru stuff
    float groundGrace = 0.0f;

    // --- HITBOX SECTION START ---
    // dec 19 2025 todo make hitboxes here not in phsics
    // make it like robloxian but better
    // legs, arms, head, torso all collidable
    // or capsule idk 
    glm::vec3 hitboxSize;
    glm::vec3 hitboxOffset;

    // Mesh
    glm::vec3 meshScale  = {1,1,1};
  
    glm::vec3 meshOffset = {0.0f, 0.0f, 0.0f};

    // Camera follow
    // dec 19 2025 i donth think wer even use this its in camera.h and camera.cpp todo 
    // dec 19 2025 z is up but idont think we use this 
    glm::vec3 cameraOffset = {0.0f, -2.5f, 0.6f};

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
