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
    void loadConfig();
    void saveConfig();
    void setMasterEnabled(bool enabled);
    bool masterEnabled();
    void update();                 // applies wireframe mode from config
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
    bool playerArchitecture();
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

    struct NpcDebugInfo {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        glm::vec3 acceleration{0.0f};
        glm::vec3 targetPosition{0.0f};
        glm::vec3 moveDirection{0.0f};
        glm::vec3 pathTarget{0.0f};
        std::string action;
        float difficulty = 0.0f;
        float awarenessRadius = 0.0f;
        float finalSpeed = 0.0f;
        bool onFloor = false;
        bool hasTarget = false;
    };

    void drawNpcDebugStuff(const std::vector<NpcDebugInfo>& npcs,
                           const Camera& camera);

    // Debug drawing primitives (for dev tools)
    void drawWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color);
    void drawLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color);
    void drawWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color);
    void drawDiagnosticWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color);
    void drawDiagnosticLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color);
    void drawDiagnosticWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color);
    void drawPointCross(const Camera& camera, glm::vec3 p, float size, glm::vec4 color);
    void drawWireBox(const Camera& camera, glm::vec3 center, glm::vec3 halfSize, glm::vec4 color);

    // Solid filled decal (not gated behind masterEnabled — for production particles/blood)
    // Changed from debug-only wireframe to solid triangle decal, jun 6 2026
    void drawFilledDecal(const Camera& camera, glm::vec3 position, glm::vec3 normal, float radius, glm::vec4 color);
    void drawBloodDecal(const Camera& camera, glm::vec3 position, glm::vec3 normal,
                        float radius, float rotation, float stretch, glm::vec4 color);
    void drawFilledBillboard(const Camera& camera, glm::vec3 position, float size,
                             float rotation, float stretch, glm::vec4 color);
    
    // Solid filled sphere (for production particles — footsteps, dash)
    void drawFilledSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color, glm::vec3 scale = glm::vec3(1.0f));

    // World-space billboard crosshair (four arms + dot, camera-facing)
    void drawCrosshairBillboard(const Camera& camera, glm::vec3 position,
                                float size, float gap, float thickness,
                                bool showDot, glm::vec4 color);

    // Always-on-top overlay version (depth-test disabled)
    void drawCrosshairBillboardOverlay(const Camera& camera, glm::vec3 position,
                                       float size, float gap, float thickness,
                                       bool showDot, glm::vec4 color);
    void drawFilledCylinder(const Camera& camera, glm::vec3 center, glm::vec3 axis, float radius, float height, glm::vec4 color);
    void drawFilledBeam(const Camera& camera, glm::vec3 start, glm::vec3 end, float thickness, glm::vec4 color);
    void drawFilledBox(const Camera& camera, glm::vec3 center, glm::vec3 halfSize, glm::vec4 color, glm::vec3 rotationEuler = glm::vec3(0.0f));
    
    // Weapon collision visuals: draw calls that bypass DEBUG_VISUALS_MASTER gate
    // so weapon_collision_visuals works independently of other debug flags.
    void drawWeaponWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color);
    void drawWeaponLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color);
    void drawWeaponCapsuleWire(const Camera& camera, const Capsule& c, glm::vec4 color);
    void flushWeaponLines(const Camera& camera);

    // Flush production VFX triangles (beams, blood, impacts, debris)
    // NOT gated behind debug flags — always renders
    void flushTris(const Camera& camera);

    // Flush overlay triangles (always-on-top, depth-test disabled)
    void flushOverlayTris(const Camera& camera);

    // World to screen projection
    bool projectToScreen(const Camera& camera, glm::vec3 worldPos, float& x, float& y);
}


// goes here or in world.h? idk jan 30 2026 
struct ChunkDebug {
    glm::vec3 min;
    glm::vec3 max;
};

void drawDebugStuff(const Player& player,
                    const Camera& camera,
                    const World& world);

// Camera axis debug visualization (red=right, green=up, blue=forward)
// Toggle with: cam_axis_debug 1|0
extern bool gCamAxisDebug;


