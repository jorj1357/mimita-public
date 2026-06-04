// C:\important\quiet\n\mimita-public\mimita-public\src\debug\debug-visuals.h
// dec 24 2025
/**
 * purpose
 * header for debug visals file 
 */

#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include "physics/physics-types.h"

// forward declatrions so we can use them elsewhere i think idk 
// jan 30 2026 
class Player;
class Camera;
class World;   // forward declare

struct DebugColors {
    glm::vec3 playerCapsule   = {1.0f, 0.0f, 0.0f}; // red
    glm::vec3 collisionBox    = {1.0f, 1.0f, 0.0f}; // yellow
    glm::vec3 worldChunks     = {0.0f, 1.0f, 0.0f}; // green
    glm::vec3 lookVector      = {0.0f, 0.5f, 1.0f}; // blue
};

namespace DebugVis {
    void init(GLFWwindow* win);
    void update();                 // handles F1-F7 hotkey toggles
    bool enabled();
    bool physics();
    bool ui();
    bool render();
    bool collision();
    bool wireframe();
    bool normals();
    bool bounds();
    bool uvChecker();
    bool lightingOnly();
    bool texturesOnly();
    bool aoOnly();
    int shaderDebugView();

    const DebugColors& colors();

    struct CollisionEvent {
        enum class Type {
            Sweep,
            Hit,
            Contact,
            Depenetration,
            Movement,
            GroundNormal,
            Triangle,
            ChunkBounds
        };

        Type type = Type::Sweep;
        glm::vec3 a{0.0f};
        glm::vec3 b{0.0f};
        glm::vec3 c{0.0f};
        glm::vec3 normal{0.0f, 0.0f, 1.0f};
        float amount = 0.0f;
        int triangleIndex = -1;
        std::string label;
    };

    void beginCollisionFrame();
    void recordCollisionEvent(const CollisionEvent& event);
    void recordSweep(glm::vec3 from, glm::vec3 to, const char* label = "");
    void recordHit(glm::vec3 point, glm::vec3 normal, int triangleIndex = -1, const char* label = "");
    void recordContact(glm::vec3 point, glm::vec3 normal, float penetration, int triangleIndex = -1, const char* label = "");
    void recordDepenetration(glm::vec3 from, glm::vec3 push, const char* label = "");
    void recordMovement(glm::vec3 from, glm::vec3 move, const char* label = "");
    void recordGroundNormal(glm::vec3 point, glm::vec3 normal, const char* label = "");
    void recordTriangle(const CollisionTriangle& tri, int triangleIndex = -1, const char* label = "");
}


// goes here or in world.h? idk jan 30 2026 
struct ChunkDebug {
    glm::vec3 min;
    glm::vec3 max;
};

void drawDebugStuff(const Player& player,
                    const Camera& camera,
                    const World& world);
