#include "debug/debug-visuals.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "physics/config.h"
#include "entities/player.h"
#include "renderer/renderer.h"
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

struct DebugTriVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

extern std::vector<DebugLineVertex> gLineVerts;
extern std::vector<DebugTriVertex> gTriVerts;
extern std::vector<DebugVis::CollisionEvent> gCollisionEvents;
extern GLFWwindow* gWindow;

// Forward declaration: defined in debug-visuals-labels.cpp
void drawWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color);
bool projectToScreen(const Camera& camera, glm::vec3 worldPos, float& x, float& y);

namespace {
GLuint gLineVao = 0;
GLuint gLineVbo = 0;

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
} // anonymous namespace

void flushDebugLines(const Camera& camera)
{
    if (gLineVerts.empty())
        return;
    if (!gRenderer || !gRenderer->shaderProgram) {
        gLineVerts.clear();
        return;
    }

    if (!gLineVao)
    {
        MIMITA_GL_CLEAR_STAGE("flushDebugLines init");
        MIMITA_GL_CALL(glGenVertexArrays(1, &gLineVao));
        MIMITA_GL_CALL(glGenBuffers(1, &gLineVbo));
    }

    MIMITA_GL_CLEAR_STAGE("flushDebugLines");
    MIMITA_GL_CALL(glDisable(GL_DEPTH_TEST));

    MIMITA_GL_CALL(glUseProgram(gRenderer->shaderProgram));

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

    MIMITA_GL_CALL(glBindVertexArray(gLineVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gLineVbo));

    MIMITA_GL_CALL(glBufferData(
        GL_ARRAY_BUFFER,
        gLineVerts.size() * sizeof(DebugLineVertex),
        gLineVerts.data(),
        GL_DYNAMIC_DRAW
    ));

    // position
    MIMITA_GL_CALL(glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugLineVertex),
        (void*)offsetof(DebugLineVertex, pos)
    ));

    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    // per-line debug color
    MIMITA_GL_CALL(glVertexAttribPointer(
        3,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugLineVertex),
        (void*)offsetof(DebugLineVertex, color)
    ));

    MIMITA_GL_CALL(glEnableVertexAttribArray(3));

    MIMITA_GL_CALL(glDisableVertexAttribArray(1));
    MIMITA_GL_CALL(glDisableVertexAttribArray(2));

    MIMITA_GL_CALL(glDrawArrays(GL_LINES, 0, (GLsizei)gLineVerts.size()));

    MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));

    gLineVerts.clear();
}

// =====================================================
// Line/point rendering primitives
// =====================================================

void drawLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color)
{
    (void)camera;
    gLineVerts.push_back({ a, color });
    gLineVerts.push_back({ b, color });
}

void drawPointCross(const Camera& camera, glm::vec3 p, float size, glm::vec4 color)
{
    drawLine(camera, p + glm::vec3(-size, 0, 0), p + glm::vec3(size, 0, 0), color);
    drawLine(camera, p + glm::vec3(0, -size, 0), p + glm::vec3(0, size, 0), color);
    drawLine(camera, p + glm::vec3(0, 0, -size), p + glm::vec3(0, 0, size), color);
}

void drawTransformAxes(const Camera& camera, const glm::mat4& transform, float scale)
{
    glm::vec3 origin = glm::vec3(transform[3]);
    glm::vec3 xAxis = glm::vec3(transform[0]);
    glm::vec3 yAxis = glm::vec3(transform[1]);
    glm::vec3 zAxis = glm::vec3(transform[2]);
    if (glm::length(xAxis) > 0.0001f) xAxis = glm::normalize(xAxis) * scale;
    if (glm::length(yAxis) > 0.0001f) yAxis = glm::normalize(yAxis) * scale;
    if (glm::length(zAxis) > 0.0001f) zAxis = glm::normalize(zAxis) * scale;
    drawLine(camera, origin, origin + xAxis, {1.0f, 0.1f, 0.1f, 1.0f});
    drawLine(camera, origin, origin + yAxis, {0.1f, 1.0f, 0.1f, 1.0f});
    drawLine(camera, origin, origin + zAxis, {0.1f, 0.45f, 1.0f, 1.0f});
}

void drawWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color)
{
    constexpr int segments = 18;
    constexpr float pi = 3.1415926535f;
    for (int i = 0; i < segments; ++i)
    {
        float a0 = (float)i / (float)segments * pi * 2.0f;
        float a1 = (float)(i + 1) / (float)segments * pi * 2.0f;
        drawLine(camera, center + glm::vec3(std::cos(a0), std::sin(a0), 0.0f) * radius,
                 center + glm::vec3(std::cos(a1), std::sin(a1), 0.0f) * radius, color);
        drawLine(camera, center + glm::vec3(std::cos(a0), 0.0f, std::sin(a0)) * radius,
                 center + glm::vec3(std::cos(a1), 0.0f, std::sin(a1)) * radius, color);
        drawLine(camera, center + glm::vec3(0.0f, std::cos(a0), std::sin(a0)) * radius,
                 center + glm::vec3(0.0f, std::cos(a1), std::sin(a1)) * radius, color);
    }
}

// =====================================================
// Collision events visualization
// =====================================================

void drawCollisionEvents(const Camera& camera)
{
    if (!DebugConfig::DEBUG_COLLISION_VISUALS)
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

// =====================================================
// Box/capsule wireframe rendering
// =====================================================

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

void drawCapsuleWire(const Camera& camera, const Capsule& c, glm::vec4 color)
{
    constexpr int segments = 20;
    constexpr float pi = 3.1415926535f;
    drawLine(camera, c.a, c.b, color);
    for (int i = 0; i < segments; ++i)
    {
        float a0 = (float)i / (float)segments * pi * 2.0f;
        float a1 = (float)(i + 1) / (float)segments * pi * 2.0f;
        glm::vec3 r0(std::cos(a0) * c.r, std::sin(a0) * c.r, 0.0f);
        glm::vec3 r1(std::cos(a1) * c.r, std::sin(a1) * c.r, 0.0f);
        drawLine(camera, c.a + r0, c.a + r1, color);
        drawLine(camera, c.b + r0, c.b + r1, color);
        if (i % 5 == 0)
            drawLine(camera, c.a + r0, c.b + r0, color);
    }
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

void drawPlayerArchitectureDebug(const Player& player, const Camera& camera)
{
    const glm::vec4 green{0.0f, 1.0f, 0.2f, 1.0f};
    const glm::vec4 cyan{0.0f, 1.0f, 1.0f, 1.0f};
    const glm::vec4 blue{0.15f, 0.35f, 1.0f, 1.0f};
    const glm::vec4 red{1.0f, 0.05f, 0.05f, 1.0f};
    const glm::vec4 yellow{1.0f, 0.9f, 0.0f, 1.0f};
    const glm::vec4 white{1.0f, 1.0f, 1.0f, 1.0f};
    const glm::vec4 magenta{1.0f, 0.0f, 1.0f, 1.0f};

    glm::mat4 originTransform =
        glm::translate(glm::mat4(1.0f), player.origin.position) *
        glm::mat4_cast(player.origin.rotation);
    drawWireSphere(camera, player.origin.position, 0.12f, green);
    drawTransformAxes(camera, originTransform, 0.55f);
    drawWorldLabel(player.origin.position + glm::vec3(0.0f, 0.0f, 0.35f), "PlayerOrigin", green);

    Capsule capsule = player.getCapsule();
    drawCapsuleWire(camera, capsule, cyan);
    drawLine(camera, player.movementCapsule.position,
             player.movementCapsule.position + player.movementCapsule.velocity * 0.25f,
             {0.0f, 1.0f, 1.0f, 0.85f});
    drawWorldLabel(player.movementCapsule.position + glm::vec3(0.0f, 0.0f, -0.35f), "MovementCapsule", cyan);

    for (int i = 0; i < (int)player.perfectPoseSkeleton.nodes.size(); ++i)
    {
        const TransformNode& node = player.perfectPoseSkeleton.nodes[i];
        glm::vec3 nodePos = glm::vec3(node.worldTransform[3]);
        drawWireSphere(camera, nodePos, 0.055f, blue);
        drawTransformAxes(camera, node.worldTransform, 0.22f);

        if (node.parent >= 0 && node.parent < (int)player.perfectPoseSkeleton.nodes.size())
        {
            glm::vec3 parentPos = glm::vec3(player.perfectPoseSkeleton.nodes[node.parent].worldTransform[3]);
            drawLine(camera, parentPos, nodePos, blue);
        }

        if (!node.name.empty() && i < 32)
            drawWorldLabel(nodePos + glm::vec3(0.0f, 0.0f, 0.12f), node.name.c_str(), blue);
    }

    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
        glm::vec3 partOrigin = glm::vec3(part.worldTransform[3]);
        drawPointCross(camera, partOrigin, 0.08f, yellow);
        drawTransformAxes(camera, part.worldTransform, 0.28f);
        drawOrientedBounds(camera, part.worldTransform, part.collider.localMin, part.collider.localMax, red);
        drawWorldLabel(partOrigin + glm::vec3(0.0f, 0.0f, 0.22f), part.name.c_str(), red);

        if (part.nodeIndex >= 0 && part.nodeIndex < (int)player.perfectPoseSkeleton.nodes.size())
        {
            glm::vec3 target = glm::vec3(player.perfectPoseSkeleton.nodes[part.nodeIndex].worldTransform[3]);
            if (glm::length(target - partOrigin) > 0.001f)
                drawLine(camera, target, partOrigin, magenta);
        }
    }

    drawLine(camera, player.origin.position, player.movementCapsule.position, white);
}

// =====================================================
// DebugVis namespace wrappers (line/point primitives)
// =====================================================

namespace DebugVis {

void drawDiagnosticWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color) {
    ::drawWireSphere(camera, center, radius, color);
}

void drawDiagnosticLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color) {
    ::drawLine(camera, a, b, color);
}

void drawWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color) {
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    ::drawWireSphere(camera, center, radius, color);
}

void drawLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color) {
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    ::drawLine(camera, a, b, color);
}

void drawWireBox(const Camera& camera, glm::vec3 center, glm::vec3 halfSize, glm::vec4 color) {
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    ::drawBox(camera, center, halfSize, color);
}

void drawPointCross(const Camera& camera, glm::vec3 p, float size, glm::vec4 color) {
    if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
    ::drawPointCross(camera, p, size, color);
}

} // namespace DebugVis
