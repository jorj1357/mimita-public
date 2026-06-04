#include "debug-visuals.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "entities/player.h"
#include "renderer/renderer.h"
#include "world/world.h"
#include "debug/debug-log.h"
#include <vector>

extern Renderer* gRenderer;

struct DebugLineVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

// idk where put this 6 3 2026 its for better rendering no crasihng 
std::vector<DebugLineVertex> gLineVerts;

namespace {
GLFWwindow* gWindow = nullptr;
DebugColors gColors;
bool gPhysics = true;
bool gUi = false;
bool gRender = true;
bool gCollision = true;
bool gCollisionVisuals = true;
bool gWireframe = false;
bool gNormals = false;
bool gBounds = true;
bool gUvChecker = false;
bool gLightingOnly = false;
bool gTexturesOnly = false;
bool gAoOnly = false;
bool gPrev[12] = {};
GLuint gLineVao = 0;
GLuint gLineVbo = 0;
std::vector<DebugVis::CollisionEvent> gCollisionEvents;


bool edge(int idx, int key)
{
    bool down = glfwGetKey(gWindow, key) == GLFW_PRESS;
    bool hit = down && !gPrev[idx];
    gPrev[idx] = down;
    return hit;
}

void setLineState(const Camera& camera, glm::vec4 color)
{
    if (!gRenderer || !gRenderer->shaderProgram) {
        Debug::warn(Debug::Category::Render, "[DEBUG WARNING] Cannot draw line: renderer/shader missing\n");
        return;
    }

    glUseProgram(gRenderer->shaderProgram);
    glm::mat4 model(1.0f);
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 1);
    glUniform4fv(glGetUniformLocation(gRenderer->shaderProgram, "uColor"), 1, &color.x);
}

void flushDebugLines(const Camera& camera)
{
    if (gLineVerts.empty())
        return;

    if (!gLineVao)
    {
        glGenVertexArrays(1, &gLineVao);
        glGenBuffers(1, &gLineVbo);
    }

    glDisable(GL_DEPTH_TEST);

    glUseProgram(gRenderer->shaderProgram);

    glm::mat4 model(1.0f);
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj(
        (float)gRenderer->width,
        (float)gRenderer->height
    );

    glUniformMatrix4fv(
        glGetUniformLocation(gRenderer->shaderProgram, "model"),
        1,
        GL_FALSE,
        &model[0][0]
    );

    glUniformMatrix4fv(
        glGetUniformLocation(gRenderer->shaderProgram, "view"),
        1,
        GL_FALSE,
        &view[0][0]
    );

    glUniformMatrix4fv(
        glGetUniformLocation(gRenderer->shaderProgram, "projection"),
        1,
        GL_FALSE,
        &proj[0][0]
    );

    glUniform1i(
        glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"),
        2
    );

    glBindVertexArray(gLineVao);
    glBindBuffer(GL_ARRAY_BUFFER, gLineVbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        gLineVerts.size() * sizeof(DebugLineVertex),
        gLineVerts.data(),
        GL_DYNAMIC_DRAW
    );

    // position
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugLineVertex),
        (void*)offsetof(DebugLineVertex, pos)
    );

    glEnableVertexAttribArray(0);

    // per-line debug color
    glVertexAttribPointer(
        3,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugLineVertex),
        (void*)offsetof(DebugLineVertex, color)
    );

    glEnableVertexAttribArray(3);

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glDrawArrays(GL_LINES, 0, (GLsizei)gLineVerts.size());

    glEnable(GL_DEPTH_TEST);

    gLineVerts.clear();
}

void drawLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color)
{
    gLineVerts.push_back({ a, color });
    gLineVerts.push_back({ b, color });
}

void drawPointCross(const Camera& camera, glm::vec3 p, float size, glm::vec4 color)
{
    drawLine(camera, p + glm::vec3(-size, 0, 0), p + glm::vec3(size, 0, 0), color);
    drawLine(camera, p + glm::vec3(0, -size, 0), p + glm::vec3(0, size, 0), color);
    drawLine(camera, p + glm::vec3(0, 0, -size), p + glm::vec3(0, 0, size), color);
}

void drawCollisionEvents(const Camera& camera)
{
    if (!gCollisionVisuals)
        return;

    constexpr int MAX_EVENTS = 512;
    int drawn = 0;
    for (const DebugVis::CollisionEvent& event : gCollisionEvents)
    {
        if (drawn++ >= MAX_EVENTS)
            break;

        switch (event.type)
        {
            case DebugVis::CollisionEvent::Type::Sweep:
                drawLine(camera, event.a, event.b, {1.0f, 0.0f, 1.0f, 0.95f});
                break;
            case DebugVis::CollisionEvent::Type::Hit:
                drawPointCross(camera, event.a, 0.06f, {1.0f, 0.0f, 0.0f, 1.0f});
                drawLine(camera, event.a, event.a + event.normal * 0.45f, {1.0f, 1.0f, 0.0f, 1.0f});
                break;
            case DebugVis::CollisionEvent::Type::Contact:
                drawPointCross(camera, event.a, 0.045f, {1.0f, 0.0f, 0.0f, 0.95f});
                drawLine(camera, event.a, event.a + event.normal * (0.25f + event.amount), {1.0f, 1.0f, 0.0f, 0.95f});
                break;
            case DebugVis::CollisionEvent::Type::Depenetration:
                drawLine(camera, event.a, event.a + event.b, {1.0f, 0.5f, 0.0f, 1.0f});
                drawPointCross(camera, event.a + event.b, 0.045f, {1.0f, 0.5f, 0.0f, 1.0f});
                break;
            case DebugVis::CollisionEvent::Type::Movement:
                drawLine(camera, event.a, event.a + event.b, {0.9f, 0.2f, 1.0f, 1.0f});
                break;
            case DebugVis::CollisionEvent::Type::GroundNormal:
                drawLine(camera, event.a, event.a + event.normal * 0.65f, {0.0f, 1.0f, 0.2f, 1.0f});
                break;
            case DebugVis::CollisionEvent::Type::Triangle:
                drawLine(camera, event.a, event.b, {1.0f, 0.85f, 0.0f, 0.65f});
                drawLine(camera, event.b, event.c, {1.0f, 0.85f, 0.0f, 0.65f});
                drawLine(camera, event.c, event.a, {1.0f, 0.85f, 0.0f, 0.65f});
                break;
            case DebugVis::CollisionEvent::Type::ChunkBounds:
                break;
        }
    }
}

void drawBox(const Camera& camera, glm::vec3 center, glm::vec3 half, glm::vec4 color)
{
    glm::vec3 v[8] = {
        center + glm::vec3(-half.x,-half.y,-half.z),
        center + glm::vec3( half.x,-half.y,-half.z),
        center + glm::vec3( half.x, half.y,-half.z),
        center + glm::vec3(-half.x, half.y,-half.z),
        center + glm::vec3(-half.x,-half.y, half.z),
        center + glm::vec3( half.x,-half.y, half.z),
        center + glm::vec3( half.x, half.y, half.z),
        center + glm::vec3(-half.x, half.y, half.z)
    };
    int e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (auto& edgePair : e)
        drawLine(camera, v[edgePair[0]], v[edgePair[1]], color);
}

void drawOrientedBounds(
    const Camera& camera,
    const glm::mat4& transform,
    glm::vec3 localMin,
    glm::vec3 localMax,
    glm::vec4 color
) {
    glm::vec3 p[8] = {
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
        {localMin.x, localMax.y, localMax.z}
    };
    glm::vec3 v[8];
    for (int i = 0; i < 8; ++i)
        v[i] = glm::vec3(transform * glm::vec4(p[i], 1.0f));
    int e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (auto& edgePair : e)
        drawLine(camera, v[edgePair[0]], v[edgePair[1]], color);
}

void drawCapsuleApprox(const Player& player, const Camera& camera)
{
    Capsule c = player.getCapsule();
    glm::vec3 z(0,0,1);
    glm::vec4 cyan{0.0f, 1.0f, 1.0f, 1.0f};
    drawLine(camera, c.a, c.b, cyan);
    drawLine(camera, c.a + glm::vec3(c.r,0,0), c.b + glm::vec3(c.r,0,0), cyan);
    drawLine(camera, c.a - glm::vec3(c.r,0,0), c.b - glm::vec3(c.r,0,0), cyan);
    drawLine(camera, c.a + glm::vec3(0,c.r,0), c.b + glm::vec3(0,c.r,0), cyan);
    drawLine(camera, c.a - glm::vec3(0,c.r,0), c.b - glm::vec3(0,c.r,0), cyan);
    drawLine(camera, c.a - z * c.r, c.a + z * c.r, cyan);
    drawLine(camera, c.b - z * c.r, c.b + z * c.r, cyan);
}
}

void DebugVis::init(GLFWwindow* win)
{
    gWindow = win;
    printf("[DEBUG] DebugVis initialized. 8 collision-vis F1 physics F2 UI F3 render F4 collision-log F5 wireframe F6 normals F7 bounds F8 UV F9 light F10 texture F11 AO\n");
}

void DebugVis::update()
{
    if (!gWindow) return;
    if (edge(0, GLFW_KEY_8)) { gCollisionVisuals = !gCollisionVisuals; printf("[DEBUG] collisionVisuals=%d\n", gCollisionVisuals); }
    if (edge(1, GLFW_KEY_F1)) { gPhysics = !gPhysics; printf("[DEBUG] physics=%d\n", gPhysics); }
    if (edge(2, GLFW_KEY_F2)) { gUi = !gUi; printf("[DEBUG] ui=%d\n", gUi); }
    if (edge(3, GLFW_KEY_F3)) { gRender = !gRender; printf("[DEBUG] render=%d\n", gRender); }
    if (edge(4, GLFW_KEY_F4)) { gCollision = !gCollision; printf("[DEBUG] collision=%d\n", gCollision); }
    if (edge(5, GLFW_KEY_F5)) { gWireframe = !gWireframe; printf("[DEBUG] wireframe=%d\n", gWireframe); }
    if (edge(6, GLFW_KEY_F6)) { gNormals = !gNormals; printf("[DEBUG] normals=%d\n", gNormals); }
    if (edge(7, GLFW_KEY_F7)) { gBounds = !gBounds; printf("[DEBUG] bounds=%d\n", gBounds); }
    if (edge(8, GLFW_KEY_F8)) { gUvChecker = !gUvChecker; printf("[DEBUG] uvChecker=%d\n", gUvChecker); }
    if (edge(9, GLFW_KEY_F9)) { gLightingOnly = !gLightingOnly; printf("[DEBUG] lightingOnly=%d\n", gLightingOnly); }
    if (edge(10, GLFW_KEY_F10)) { gTexturesOnly = !gTexturesOnly; printf("[DEBUG] texturesOnly=%d\n", gTexturesOnly); }
    if (edge(11, GLFW_KEY_F11)) { gAoOnly = !gAoOnly; printf("[DEBUG] aoOnly=%d\n", gAoOnly); }
    glPolygonMode(GL_FRONT_AND_BACK, gWireframe ? GL_LINE : GL_FILL);
}

bool DebugVis::enabled() { return gPhysics || gCollision || gBounds || gNormals; }
bool DebugVis::physics() { return gPhysics; }
bool DebugVis::ui() { return gUi; }
bool DebugVis::render() { return gRender; }
bool DebugVis::collision() { return gCollision; }
bool DebugVis::wireframe() { return gWireframe; }
bool DebugVis::normals() { return gNormals; }
bool DebugVis::bounds() { return gBounds; }
bool DebugVis::uvChecker() { return gUvChecker; }
bool DebugVis::lightingOnly() { return gLightingOnly; }
bool DebugVis::texturesOnly() { return gTexturesOnly; }
bool DebugVis::aoOnly() { return gAoOnly; }
int DebugVis::shaderDebugView()
{
    if (gUvChecker) return 1;
    if (gLightingOnly) return 2;
    if (gTexturesOnly) return 3;
    if (gAoOnly) return 4;
    if (gNormals) return 5;
    return 0;
}
const DebugColors& DebugVis::colors() { return gColors; }

void DebugVis::beginCollisionFrame()
{
    gCollisionEvents.clear();
}

void DebugVis::recordCollisionEvent(const CollisionEvent& event)
{
    if (!gCollisionVisuals)
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

void drawDebugStuff(const Player& player, const Camera& camera, const World& world)
{
    if (!DebugVis::enabled()) return;

    if (DebugVis::physics()) {
            printf("dbg physics start\n");
        drawCapsuleApprox(player, camera);
        drawLine(camera, player.pos, player.pos + player.vel * 0.25f, {0.0f,1.0f,0.2f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,-3), {0.2f,0.5f,1.0f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,2), {1.0f,1.0f,0.0f,1.0f});
            printf("dbg  1\n");
        for (const Collider& collider : player.bodyColliders) {
            auto it = std::find_if(player.nodes.begin(), player.nodes.end(), [&](const TransformNode& node) {
                return node.name == collider.name;
            });
            if (it != player.nodes.end())
                        printf("dbg  2\n");

            {
                drawOrientedBounds(camera, it->worldTransform, collider.localMin, collider.localMax, {1.0f,0.2f,0.9f,0.85f});
                glm::vec3 origin = glm::vec3(it->worldTransform[3]);
                glm::vec3 xAxis = glm::normalize(glm::vec3(it->worldTransform[0])) * 0.25f;
                glm::vec3 yAxis = glm::normalize(glm::vec3(it->worldTransform[1])) * 0.25f;
                glm::vec3 zAxis = glm::normalize(glm::vec3(it->worldTransform[2])) * 0.25f;
                drawLine(camera, origin, origin + xAxis, {1.0f,0.1f,0.1f,1.0f});
                drawLine(camera, origin, origin + yAxis, {0.1f,1.0f,0.1f,1.0f});
                drawLine(camera, origin, origin + zAxis, {0.1f,0.4f,1.0f,1.0f});
                            printf("dbg  3\n");

            }
        }
    }

    if (DebugVis::collision()) {
        drawCollisionEvents(camera);
    }

    if (DebugVis::render()) {
        drawLine(camera, camera.pos, camera.pos + camera.front * 5.0f, {0.2f,0.8f,1.0f,1.0f});
    }

    if (DebugVis::bounds()) {
        drawBox(camera, player.pos + glm::vec3(0,0,1), {0.55f,0.55f,1.0f}, {1.0f,0.0f,1.0f,1.0f});
        int drawn = 0;
        if (!world.collisionMesh.empty()) {
            if (!world.collisionChunks.empty() && world.collisionChunkSize > 0.001f) {
                glm::ivec3 pc(
                    (int)std::floor(player.pos.x / world.collisionChunkSize),
                    (int)std::floor(player.pos.y / world.collisionChunkSize),
                    (int)std::floor(player.pos.z / world.collisionChunkSize)

                );
                int chunkDrawn = 0;
                                                printf("dbg  4\n");

                for (int x = pc.x - 1; x <= pc.x + 1; ++x)
                for (int y = pc.y - 1; y <= pc.y + 1; ++y)
                for (int z = pc.z - 1; z <= pc.z + 1; ++z) {
                    if (world.collisionChunks.find(glm::ivec3(x, y, z)) == world.collisionChunks.end())
                        continue;
                    glm::vec3 center = (glm::vec3(x, y, z) + glm::vec3(0.5f)) * world.collisionChunkSize;
                    drawBox(camera, center, glm::vec3(world.collisionChunkSize * 0.5f), {0.0f,1.0f,0.3f,0.35f});
                    if (++chunkDrawn >= 16)
                        break;
                }
            }
            for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
                drawLine(camera, tri.a, tri.b, {1.0f,0.85f,0.0f,0.65f});
                drawLine(camera, tri.b, tri.c, {1.0f,0.85f,0.0f,0.65f});
                drawLine(camera, tri.c, tri.a, {1.0f,0.85f,0.0f,0.65f});
                                                printf("dbg  5\n");

                if (DebugVis::normals()) {
                    glm::vec3 center = (tri.a + tri.b + tri.c) / 3.0f;
                    drawLine(camera, center, center + tri.normal * 0.35f, {0.2f,1.0f,1.0f,0.8f});
                }
                if (++drawn >= 96) break;
            }
        } else {
            for (const Block& b : world.blocks) {
                drawBox(camera, b.pos, b.size * 0.5f, {1.0f,1.0f,0.0f,0.85f});
                if (++drawn >= 24) break;
            }
        }
    }

    if (gRenderer && gRenderer->shaderProgram) {
        glUseProgram(gRenderer->shaderProgram);
                                        printf("dbg  6\n");

        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 0);
    }
    flushDebugLines(camera);
}
