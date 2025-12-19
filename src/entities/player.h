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
    glm::vec3 pos{0, 0, 2};
    glm::vec3 vel{0};
    bool onGround = false;

    // Orientation
    float yaw = 0.0f;   // <-- THIS was missing

    // --- HITBOX SECTION START ---
    // dec 2 2025 this size shouldddd be like robloxian
    // head torso and legs are solid but arms arent
    // dec 3 2025 todo is THIS hitbox size or is the other one hitbox size
    // glm::vec3 hitboxSize = {1.0f, 1.8f, 0.35f}; 
    // default offset
        // hitboxOffset = glm::vec3(0.0f, hitboxSize.y * 0.5f, 0.0f);
    // new offset dec 3 2025 todo
        // glm::vec3 hitboxOffset = {0.0f, hitboxSize.y * 0.5f, 0.0f};

    // dec 3 2025 todo skipping it so that player.cpp can do it
    // even tho i hate that 
    // but we will consolidate later, just make it work for now 
    glm::vec3 hitboxSize;
    glm::vec3 hitboxOffset;

    // the origin is the center of torso
    // but dont make me just sit in the floor bc my center of torso is on the floor
    // make the edges of the torso do it 
    // glm::vec3 hitboxOffset = {0.0f, 0.0f, 0.0f};
    // --- HITBOX SECTION END ---

    // Mesh
    glm::vec3 meshScale  = {1,1,1};
    // mesh v2 i think blender is 2x smaller than the 0.0f scale
    // glm::vec3 meshScale  = {0.5,0.5,0.5};

    // new cool one dec 3 2025 todo is it good
    glm::vec3 meshOffset = {0.0f, 0.0f, 0.0f};
    // old one
    // glm::vec3 meshOffset = {0.0f, -0.9f, 0.0f};


    // Camera follow
    // dec 19 2025 i donth think wer even use this its in camera.h and camera.cpp todo 
    // dec 19 2025 z is up but idont think we use this 
    glm::vec3 cameraOffset = {0.0f, -2.5f, 0.6f};

    // Helpers
    inline glm::vec3 halfExtents() const { return hitboxSize * 0.5f; }

    glm::mat4 rootTransform = glm::mat4(1.0f);

    // REQUIRED NEW FUNCTION
    OBB getOBB() const;

    // ---- DEBUG COLLISION SPHERES (visual only) ----
    // dec 16 2025 im conuffsed and making bunches of edits and it mightbreak later but ok wahtrever 
    // glm::vec3 debugFeetSphere {0};
    // glm::vec3 debugMidSphere  {0};
    // glm::vec3 debugHeadSphere {0};

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
