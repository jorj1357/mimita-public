#include "debug-visuals.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "physics/config.h"
#include "entities/player.h"
#include "renderer/renderer.h"
#include "gui/ui-system.h"
#include "world/world.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"
#include "debug/debug-diag.h"
#include "config.h"

extern Renderer* gRenderer;

struct DebugLineVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

struct DebugTextLabel
{
    glm::vec3 worldPos{0.0f};
    std::string text;
    glm::vec4 color{1.0f};
};

// idk where put this 6 3 2026 its for better rendering no crasihng 
std::vector<DebugLineVertex> gLineVerts;

// CHANGED: Added solid triangle buffer for filled decals (blood splats), jun 6 2026
// Previously blood was drawn as wireframe lines via DebugVis::drawLine
struct DebugTriVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

std::vector<DebugTriVertex> gTriVerts;
std::vector<DebugTriVertex> gOverlayTriVerts; // always-on-top overlay

GLFWwindow* gWindow = nullptr;
DebugColors gColors;
std::vector<DebugVis::CollisionEvent> gCollisionEvents;
std::vector<DebugTextLabel> gTextLabels;

void DebugVis::init(GLFWwindow* win)
{
    gWindow = win;
    printf("[DBGVIS] init\n");
}

void DebugVis::setMasterEnabled(bool enabled) {
    if (enabled == DebugConfig::DEBUG_VISUALS_MASTER)
        return;
    DebugConfig::DEBUG_VISUALS_MASTER = enabled;
    if (!enabled) {
        gLineVerts.clear();
        gTriVerts.clear();
        gCollisionEvents.clear();
        gTextLabels.clear();
        printf("[DBGVIS] clear\n");
    } else {
        printf("[DBGVIS] enable\n");
    }
}
bool DebugVis::masterEnabled() { return DebugConfig::DEBUG_VISUALS_MASTER; }

void DebugVis::update()
{
    if (!gWindow) return;
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    glPolygonMode(GL_FRONT_AND_BACK, DebugConfig::DEBUG_WIREFRAME ? GL_LINE : GL_FILL);
}

bool DebugVis::enabled() { return DebugConfig::DEBUG_VISUALS_MASTER && (DebugConfig::DEBUG_PHYSICS || DebugConfig::DEBUG_COLLISION || DebugConfig::DEBUG_BOUNDS || DebugConfig::DEBUG_NORMALS || DebugConfig::DEBUG_PLAYERARCH); }
bool DebugVis::physics() { return DebugConfig::DEBUG_PHYSICS; }
bool DebugVis::ui() { return DebugConfig::DEBUG_UI; }
bool DebugVis::render() { return DebugConfig::DEBUG_RENDER; }
bool DebugVis::collision() { return DebugConfig::DEBUG_COLLISION; }
bool DebugVis::wireframe() { return DebugConfig::DEBUG_WIREFRAME; }
bool DebugVis::normals() { return DebugConfig::DEBUG_NORMALS; }
bool DebugVis::bounds() { return DebugConfig::DEBUG_BOUNDS; }
bool DebugVis::uvChecker() { return DebugConfig::DEBUG_UVCHECKER; }
bool DebugVis::lightingOnly() { return DebugConfig::DEBUG_LIGHTING_ONLY; }
bool DebugVis::texturesOnly() { return DebugConfig::DEBUG_TEXTURES_ONLY; }
bool DebugVis::aoOnly() { return DebugConfig::DEBUG_AO_ONLY; }
bool DebugVis::playerArchitecture() { return DebugConfig::DEBUG_PLAYERARCH; }
int DebugVis::shaderDebugView()
{
    if (DebugConfig::DEBUG_UVCHECKER) return 1;
    if (DebugConfig::DEBUG_LIGHTING_ONLY) return 2;
    if (DebugConfig::DEBUG_TEXTURES_ONLY) return 3;
    if (DebugConfig::DEBUG_AO_ONLY) return 4;
    if (DebugConfig::DEBUG_NORMALS) return 5;
    return 0;
}
const DebugColors& DebugVis::colors() { return gColors; }

void DebugVis::beginCollisionFrame()
{
    gCollisionEvents.clear();
}

void DebugVis::recordCollisionEvent(const CollisionEvent& event)
{
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    if (!DebugConfig::DEBUG_COLLISION_VISUALS)
        return;
    if (gCollisionEvents.size() >= 1024)
        return;
    gCollisionEvents.push_back(event);
}

void DebugVis::recordSweep(glm::vec3 from, glm::vec3 to, const char* label)
{
    CollisionEvent event;
    event.type = CollisionEvent::Type::Sweep;
    event.a = from;
    event.b = to;
    event.label = label ? label : "";
    recordCollisionEvent(event);
}

void DebugVis::recordHit(glm::vec3 point, glm::vec3 normal, int triangleIndex, const char* label)
{
    CollisionEvent event;
    event.type = CollisionEvent::Type::Hit;
    event.a = point;
    event.normal = normal;
    event.triangleIndex = triangleIndex;
    event.label = label ? label : "";
    recordCollisionEvent(event);
}

void DebugVis::recordContact(glm::vec3 point, glm::vec3 normal, float penetration, int triangleIndex, const char* label)
{
    CollisionEvent event;
    event.type = CollisionEvent::Type::Contact;
    event.a = point;
    event.normal = normal;
    event.amount = penetration;
    event.triangleIndex = triangleIndex;
    event.label = label ? label : "";
    recordCollisionEvent(event);
}

void DebugVis::recordDepenetration(glm::vec3 from, glm::vec3 push, const char* label)
{
    CollisionEvent event;
    event.type = CollisionEvent::Type::Depenetration;
    event.a = from;
    event.b = push;
    event.label = label ? label : "";
    recordCollisionEvent(event);
}

void DebugVis::recordMovement(glm::vec3 from, glm::vec3 move, const char* label)
{
    CollisionEvent event;
    event.type = CollisionEvent::Type::Movement;
    event.a = from;
    event.b = move;
    event.label = label ? label : "";
    recordCollisionEvent(event);
}

void DebugVis::recordGroundNormal(glm::vec3 point, glm::vec3 normal, const char* label)
{
    CollisionEvent event;
    event.type = CollisionEvent::Type::GroundNormal;
    event.a = point;
    event.normal = normal;
    event.label = label ? label : "";
    recordCollisionEvent(event);
}

void DebugVis::recordTriangle(const CollisionTriangle& tri, int triangleIndex, const char* label)
{
    CollisionEvent event;
    event.type = CollisionEvent::Type::Triangle;
    event.a = tri.a;
    event.b = tri.b;
    event.c = tri.c;
    event.normal = tri.normal;
    event.triangleIndex = triangleIndex;
    event.label = label ? label : "";
    recordCollisionEvent(event);
}
