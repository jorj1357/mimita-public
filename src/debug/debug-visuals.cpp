#include "debug-visuals.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

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
#include "config.h"
#include <vector>

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

namespace {
GLFWwindow* gWindow = nullptr;
DebugColors gColors;
GLuint gLineVao = 0;
GLuint gLineVbo = 0;
// CHANGED: Added triangle VAO/VBO for solid decals, jun 6 2026
GLuint gTriVao = 0;
GLuint gTriVbo = 0;
std::vector<DebugVis::CollisionEvent> gCollisionEvents;
std::vector<DebugTextLabel> gTextLabels;

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

// CHANGED: New triangle flush for solid decals (blood splats), jun 6 2026
void flushDebugTris(const Camera& camera)
{
    if (gTriVerts.empty())
        return;
    if (!gRenderer || !gRenderer->shaderProgram) {
        gTriVerts.clear();
        return;
    }

    if (!gTriVao)
    {
        MIMITA_GL_CLEAR_STAGE("flushDebugTris init");
        MIMITA_GL_CALL(glGenVertexArrays(1, &gTriVao));
        MIMITA_GL_CALL(glGenBuffers(1, &gTriVbo));
    }

    MIMITA_GL_CLEAR_STAGE("flushDebugTris");
    // Transparent particle pass: disable depth writes so particles
    // (blood, debris, sparks) blend together instead of occluding each other.
    // Depth test is kept so particles sort behind world geometry, but they
    // should not write their own depth and hide other particles nearby.
    MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));
    MIMITA_GL_CALL(glDepthMask(GL_FALSE));
    MIMITA_GL_CALL(glEnable(GL_BLEND));
    MIMITA_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

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

    MIMITA_GL_CALL(glBindVertexArray(gTriVao));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, gTriVbo));

    MIMITA_GL_CALL(glBufferData(
        GL_ARRAY_BUFFER,
        gTriVerts.size() * sizeof(DebugTriVertex),
        gTriVerts.data(),
        GL_DYNAMIC_DRAW
    ));

    // position
    MIMITA_GL_CALL(glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugTriVertex),
        (void*)offsetof(DebugTriVertex, pos)
    ));

    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    // per-vertex color
    MIMITA_GL_CALL(glVertexAttribPointer(
        3,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(DebugTriVertex),
        (void*)offsetof(DebugTriVertex, color)
    ));

    MIMITA_GL_CALL(glEnableVertexAttribArray(3));

    MIMITA_GL_CALL(glDisableVertexAttribArray(1));
    MIMITA_GL_CALL(glDisableVertexAttribArray(2));

    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, (GLsizei)gTriVerts.size()));

    MIMITA_GL_CALL(glDepthMask(GL_TRUE));

    gTriVerts.clear();
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

void drawWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color)
{
    if (!text || !*text)
        return;
    if (gTextLabels.size() >= 96)
        return;
    gTextLabels.push_back({worldPos, text, color});
}

bool projectToScreen(const Camera& camera, glm::vec3 worldPos, float& x, float& y)
{
    if (!gRenderer)
        return false;

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f)
        return false;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f)
        return false;

    x = (ndc.x * 0.5f + 0.5f) * (float)gRenderer->width;
    y = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)gRenderer->height;
    return true;
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

// CHANGED: Solid filled decal for blood splats, jun 6 2026
void drawFilledDecal(const Camera& camera, glm::vec3 position, glm::vec3 normal, float radius, glm::vec4 color)
{
    constexpr int SEGMENTS = 20;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0,0,1);
    glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0,0,1))
                                                             : glm::cross(n, glm::vec3(0,1,0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
    for (int i = 0; i < SEGMENTS; ++i)
    {
        float a0 = 6.2831853f * i / SEGMENTS;
        float a1 = 6.2831853f * ((i + 1) % SEGMENTS) / SEGMENTS;
        glm::vec3 p0 = position + (tangent * std::cos(a0) + bitangent * std::sin(a0)) * radius;
        glm::vec3 p1 = position + (tangent * std::cos(a1) + bitangent * std::sin(a1)) * radius;
        gTriVerts.push_back({position, color});
        gTriVerts.push_back({p0, color});
        gTriVerts.push_back({p1, color});
    }
}

void drawBloodDecal(
    const Camera& camera,
    glm::vec3 position,
    glm::vec3 normal,
    float radius,
    float rotation,
    float stretch,
    glm::vec4 color)
{
    (void)camera;
    constexpr int SEGMENTS = 24;
    const glm::vec3 n = glm::length(normal) > 0.001f
        ? glm::normalize(normal)
        : glm::vec3(0, 0, 1);
    const glm::vec3 baseTangent = glm::normalize(
        std::fabs(n.z) < 0.9f
            ? glm::cross(n, glm::vec3(0, 0, 1))
            : glm::cross(n, glm::vec3(0, 1, 0)));
    const glm::vec3 baseBitangent = glm::normalize(glm::cross(n, baseTangent));
    const glm::vec3 tangent =
        baseTangent * std::cos(rotation) + baseBitangent * std::sin(rotation);
    const glm::vec3 bitangent =
        -baseTangent * std::sin(rotation) + baseBitangent * std::cos(rotation);

    const auto drawBlob = [&](const glm::vec3& center, float blobRadius,
                              float blobStretch, float phase) {
        for (int i = 0; i < SEGMENTS; ++i) {
            const float a0 = 6.2831853f * (float)i / (float)SEGMENTS;
            const float a1 = 6.2831853f * (float)(i + 1) / (float)SEGMENTS;
            const float noise0 =
                0.96f + 0.025f * std::sin(a0 * 2.0f + phase) +
                0.015f * std::sin(a0 * 3.0f - phase * 0.7f);
            const float noise1 =
                0.96f + 0.025f * std::sin(a1 * 2.0f + phase) +
                0.015f * std::sin(a1 * 3.0f - phase * 0.7f);
            const glm::vec3 p0 = center +
                (tangent * std::cos(a0) * blobStretch + bitangent * std::sin(a0)) *
                    blobRadius * noise0;
            const glm::vec3 p1 = center +
                (tangent * std::cos(a1) * blobStretch + bitangent * std::sin(a1)) *
                    blobRadius * noise1;
            gTriVerts.push_back({center, color});
            gTriVerts.push_back({p0, color});
            gTriVerts.push_back({p1, color});
        }
    };

    drawBlob(position, radius, stretch, rotation);
    for (int blob = 0; blob < 4; ++blob) {
        const float angle = rotation * 1.7f + (float)blob * 1.5707963f;
        const float offset = radius * (0.48f + 0.06f * std::sin(rotation + blob));
        const glm::vec3 center = position +
            tangent * std::cos(angle) * offset +
            bitangent * std::sin(angle) * offset;
        const float blobRadius =
            radius * (0.28f + 0.08f * std::sin(rotation * 2.3f + blob * 1.9f));
        drawBlob(center, blobRadius, 0.9f + 0.15f * stretch, angle);
    }
}

void drawFilledBillboard(
    const Camera& camera,
    glm::vec3 position,
    float size,
    float rotation,
    float stretch,
    glm::vec4 color)
{
    // Circular billboard: 16-segment triangle fan
    // Each segment draws a triangle from center to two consecutive edge points
    constexpr int SEGMENTS = 16;
    for (int i = 0; i < SEGMENTS; ++i) {
        const float a1 = rotation + (float)i * 6.2831855f / (float)SEGMENTS;
        const float a2 = rotation + (float)(i + 1) * 6.2831855f / (float)SEGMENTS;
        const glm::vec3 edge1 =
            position +
            (camera.right * std::cos(a1) + camera.up * std::sin(a1)) * size * stretch;
        const glm::vec3 edge2 =
            position +
            (camera.right * std::cos(a2) + camera.up * std::sin(a2)) * size;
        gTriVerts.push_back({position, color});
        gTriVerts.push_back({edge1, color});
        gTriVerts.push_back({edge2, color});
    }
}

// Solid filled sphere for production particles (footsteps, dash)
void drawFilledSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color, glm::vec3 scale)
{
    (void)camera;
    constexpr int LAT_SEGMENTS = 8;
    constexpr int LON_SEGMENTS = 12;
    for (int lat = 0; lat < LAT_SEGMENTS; ++lat)
    {
        float a0 = 3.14159265f * (float)lat / (float)LAT_SEGMENTS;
        float a1 = 3.14159265f * (float)(lat + 1) / (float)LAT_SEGMENTS;
        float y0 = std::cos(a0);
        float y1 = std::cos(a1);
        float r0 = std::sin(a0);
        float r1 = std::sin(a1);
        for (int lon = 0; lon < LON_SEGMENTS; ++lon)
        {
            float b0 = 2.0f * 3.14159265f * (float)lon / (float)LON_SEGMENTS;
            float b1 = 2.0f * 3.14159265f * (float)((lon + 1) % LON_SEGMENTS) / (float)LON_SEGMENTS;
            glm::vec3 off0 = scale * glm::vec3(radius * r0 * std::sin(b0), radius * r0 * std::cos(b0), radius * y0);
            glm::vec3 off1 = scale * glm::vec3(radius * r1 * std::sin(b0), radius * r1 * std::cos(b0), radius * y1);
            glm::vec3 off2 = scale * glm::vec3(radius * r1 * std::sin(b1), radius * r1 * std::cos(b1), radius * y1);
            glm::vec3 off3 = scale * glm::vec3(radius * r0 * std::sin(b1), radius * r0 * std::cos(b1), radius * y0);
            glm::vec3 p00 = center + off0;
            glm::vec3 p10 = center + off1;
            glm::vec3 p01 = center + off3;
            glm::vec3 p11 = center + off2;
            if (lat > 0) {
                gTriVerts.push_back({p00, color});
                gTriVerts.push_back({p10, color});
                gTriVerts.push_back({p01, color});
            }
            if (lat < LAT_SEGMENTS - 1) {
                gTriVerts.push_back({p01, color});
                gTriVerts.push_back({p10, color});
                gTriVerts.push_back({p11, color});
            }
        }
    }
}

void drawFilledCylinder(const Camera& camera, glm::vec3 center, glm::vec3 axis, float radius, float height, glm::vec4 color)
{
    (void)camera;
    constexpr int SEGMENTS = 16;
    glm::vec3 n = glm::length(axis) > 0.001f ? glm::normalize(axis) : glm::vec3(0, 0, 1);
    glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0, 0, 1))
                                                             : glm::cross(n, glm::vec3(0, 1, 0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
    glm::vec3 topCenter = center + n * (height * 0.5f);
    glm::vec3 bottomCenter = center - n * (height * 0.5f);

    for (int i = 0; i < SEGMENTS; ++i)
    {
        float a0 = 6.2831853f * (float)i / (float)SEGMENTS;
        float a1 = 6.2831853f * (float)(i + 1) / (float)SEGMENTS;
        glm::vec3 radial0 = (tangent * std::cos(a0) + bitangent * std::sin(a0)) * radius;
        glm::vec3 radial1 = (tangent * std::cos(a1) + bitangent * std::sin(a1)) * radius;
        glm::vec3 top0 = topCenter + radial0;
        glm::vec3 top1 = topCenter + radial1;
        glm::vec3 bottom0 = bottomCenter + radial0;
        glm::vec3 bottom1 = bottomCenter + radial1;

        gTriVerts.push_back({topCenter, color});
        gTriVerts.push_back({top0, color});
        gTriVerts.push_back({top1, color});
        gTriVerts.push_back({bottomCenter, color});
        gTriVerts.push_back({bottom1, color});
        gTriVerts.push_back({bottom0, color});
        gTriVerts.push_back({top0, color});
        gTriVerts.push_back({bottom0, color});
        gTriVerts.push_back({top1, color});
        gTriVerts.push_back({top1, color});
        gTriVerts.push_back({bottom0, color});
        gTriVerts.push_back({bottom1, color});
    }
}

glm::mat4 eulerToMat4(const glm::vec3& euler)
{
    float cx = std::cos(euler.x), sx = std::sin(euler.x);
    float cy = std::cos(euler.y), sy = std::sin(euler.y);
    float cz = std::cos(euler.z), sz = std::sin(euler.z);
    glm::mat4 m(1.0f);
    m[0][0] = cy * cz;               m[0][1] = cy * sz;               m[0][2] = -sy;
    m[1][0] = sx * sy * cz - cx * sz; m[1][1] = sx * sy * sz + cx * cz; m[1][2] = sx * cy;
    m[2][0] = cx * sy * cz + sx * sz; m[2][1] = cx * sy * sz - sx * cz; m[2][2] = cx * cy;
    return m;
}

void drawFilledBox(const Camera& camera, glm::vec3 center, glm::vec3 halfSize, glm::vec4 color, glm::vec3 rotationEuler)
{
    (void)camera;
    glm::mat4 rotMat = glm::mat4(1.0f);
    if (glm::length(rotationEuler) > 0.001f)
        rotMat = eulerToMat4(rotationEuler);
    glm::vec3 corners[8] = {
        glm::vec3(-halfSize.x, -halfSize.y, -halfSize.z),
        glm::vec3( halfSize.x, -halfSize.y, -halfSize.z),
        glm::vec3( halfSize.x,  halfSize.y, -halfSize.z),
        glm::vec3(-halfSize.x,  halfSize.y, -halfSize.z),
        glm::vec3(-halfSize.x, -halfSize.y,  halfSize.z),
        glm::vec3( halfSize.x, -halfSize.y,  halfSize.z),
        glm::vec3( halfSize.x,  halfSize.y,  halfSize.z),
        glm::vec3(-halfSize.x,  halfSize.y,  halfSize.z)
    };
    glm::vec3 v[8];
    for (int i = 0; i < 8; ++i)
        v[i] = center + glm::vec3(rotMat * glm::vec4(corners[i], 1.0f));
    const int triangles[36] = {
        0,2,1, 0,3,2, 4,5,6, 4,6,7,
        0,1,5, 0,5,4, 1,2,6, 1,6,5,
        2,3,7, 2,7,6, 3,0,4, 3,4,7
    };
    for (int index : triangles)
        gTriVerts.push_back({v[index], color});
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

void drawDebugLabels(const Camera& camera)
{
    if (gTextLabels.empty() || !gWindow)
        return;

    uiBeginFrame(gWindow, "player-architecture-labels");
    for (const DebugTextLabel& label : gTextLabels)
    {
        float x = 0.0f;
        float y = 0.0f;
        if (projectToScreen(camera, label.worldPos, x, y))
            uiDrawText(label.text.c_str(), x + 4.0f, y - 4.0f, 0.24f, label.color);
    }
    uiEndFrame();
    gTextLabels.clear();
}
}

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

void DebugVis::drawNpcDebugStuff(const std::vector<NpcDebugInfo>& npcs,
                                 const Camera& camera)
{
    if (!DebugVis::masterEnabled() || !DebugConfig::DEBUG_NPC)
        return;

    const glm::vec4 awarenessColor{1.0f, 0.35f, 0.05f, 0.28f};
    const glm::vec4 moveColor{0.15f, 1.0f, 0.35f, 0.95f};
    const glm::vec4 targetColor{1.0f, 0.12f, 0.12f, 0.9f};
    const glm::vec4 pathColor{0.2f, 0.6f, 1.0f, 0.95f};
    const glm::vec4 groundedColor{0.1f, 1.0f, 0.25f, 1.0f};
    const glm::vec4 airborneColor{1.0f, 0.25f, 0.1f, 1.0f};

    for (const NpcDebugInfo& npc : npcs)
    {
        drawWireSphere(camera, npc.position, npc.awarenessRadius, awarenessColor);
        drawLine(camera, npc.position, npc.position + npc.velocity * 0.20f, {0.0f, 1.0f, 1.0f, 0.9f});
        drawLine(camera, npc.position, npc.position + npc.acceleration * 0.02f, {1.0f, 0.2f, 1.0f, 0.9f});
        drawLine(camera, npc.position, npc.position + npc.moveDirection * 2.0f, moveColor);
        drawLine(camera, npc.position, npc.pathTarget, pathColor);
        drawWireSphere(
            camera,
            npc.position + glm::vec3(0.0f, 0.0f, 0.15f),
            0.18f,
            npc.grounded ? groundedColor : airborneColor
        );

        if (npc.hasTarget)
            drawLine(camera, npc.position + glm::vec3(0.0f, 0.0f, 1.2f),
                     npc.targetPosition + glm::vec3(0.0f, 0.0f, 1.2f),
                     targetColor);

        char label[160];
        snprintf(label, sizeof(label), "NPC d=%.1f %s speed=%.1f %s",
                 npc.difficulty,
                 npc.action.c_str(),
                 npc.finalSpeed,
                 npc.grounded ? "grounded" : "airborne");
        drawWorldLabel(
            npc.position + glm::vec3(0.0f, 0.0f, 2.25f),
            label,
            npc.grounded ? groundedColor : airborneColor
        );
    }
}

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

void drawDebugStuff(const Player& player, const Camera& camera, const World& world)
{
    if (!DebugVis::enabled()) return;

    if (DebugVis::physics()) {
        drawCapsuleApprox(player, camera);
        drawLine(camera, player.pos, player.pos + player.vel * 0.25f, {0.0f,1.0f,0.2f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,-3), {0.2f,0.5f,1.0f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,2), {1.0f,1.0f,0.0f,1.0f});
        for (const Collider& collider : player.bodyColliders) {
            auto it = std::find_if(player.nodes.begin(), player.nodes.end(), [&](const TransformNode& node) {
                return node.name == collider.name;
            });
            if (it != player.nodes.end())
            {
                drawOrientedBounds(camera, it->worldTransform, collider.localMin, collider.localMax, {1.0f,0.2f,0.9f,0.85f});
                glm::vec3 origin = glm::vec3(it->worldTransform[3]);
                glm::vec3 xAxis = glm::normalize(glm::vec3(it->worldTransform[0])) * 0.25f;
                glm::vec3 yAxis = glm::normalize(glm::vec3(it->worldTransform[1])) * 0.25f;
                glm::vec3 zAxis = glm::normalize(glm::vec3(it->worldTransform[2])) * 0.25f;
                drawLine(camera, origin, origin + xAxis, {1.0f,0.1f,0.1f,1.0f});
                drawLine(camera, origin, origin + yAxis, {0.1f,1.0f,0.1f,1.0f});
                drawLine(camera, origin, origin + zAxis, {0.1f,0.4f,1.0f,1.0f});
            }
        }
    }

    if (DebugVis::playerArchitecture()) {
        drawPlayerArchitectureDebug(player, camera);
    }

    if (DebugVis::collision()) {
        drawCollisionEvents(camera);
    }

    // collision_debug gated visualization
    if (DebugConfig::DEBUG_COLLISION_SYSTEM) {
        drawCollisionEvents(camera);

        // Grounded state label above player
        char info[128];
        if (player.stableOnGround) {
            snprintf(info, sizeof(info), "GROUNDED stable=%d lostTimer=%.3f",
                     (int)player.onGround, player.groundLostTimer);
        } else {
            snprintf(info, sizeof(info), "AIR vel.z=%.2f",
                     player.vel.z);
        }
        drawWorldLabel(player.pos + glm::vec3(0, 0, PLAYER_HEIGHT + 0.8f), info, {0.0f, 1.0f, 0.0f, 1.0f});

        // Limb collider wireframes
        const glm::vec4 limbColor{0.8f, 0.4f, 0.1f, 0.8f};
        for (const Collider& collider : player.bodyColliders)
        {
            auto it = std::find_if(player.nodes.begin(), player.nodes.end(), [&](const TransformNode& node) {
                return node.name == collider.name;
            });
            if (it == player.nodes.end()) continue;

            const glm::mat4& xform = it->worldTransform;
            if (!collider.samplePoints.empty())
            {
                for (glm::vec3 point : collider.samplePoints)
                {
                    glm::vec3 worldPt = glm::vec3(xform * glm::vec4(point, 1.0f));
                    drawWireSphere(camera, worldPt, BODY_SAMPLE_RADIUS, limbColor);
                }
            }
            if (!collider.triangles.empty())
            {
                for (const CollisionTriangle& tri : collider.triangles)
                {
                    glm::vec3 wa = glm::vec3(xform * glm::vec4(tri.a, 1.0f));
                    glm::vec3 wb = glm::vec3(xform * glm::vec4(tri.b, 1.0f));
                    glm::vec3 wc = glm::vec3(xform * glm::vec4(tri.c, 1.0f));
                    drawLine(camera, wa, wb, limbColor);
                    drawLine(camera, wb, wc, limbColor);
                    drawLine(camera, wc, wa, limbColor);
                }
            }
        }
    }

    if (DebugVis::render()) {
        drawLine(camera, camera.pos, camera.pos + camera.front * 5.0f, {0.2f,0.8f,1.0f,1.0f});

        for (size_t i = 0; i < world.spawnPoints.size(); ++i)
        {
            const SpawnPoint& spawn = world.spawnPoints[i];
            const bool selected = (int)i == world.selectedSpawnIndex;
            const glm::vec4 color = selected
                ? glm::vec4(1.0f, 0.85f, 0.1f, 1.0f)
                : glm::vec4(0.2f, 1.0f, 0.55f, 1.0f);
            const float radius = selected ? 0.55f : 0.35f;

            DebugVis::drawWireSphere(camera, spawn.position, radius, color);
            DebugVis::drawPointCross(camera, spawn.position, radius * 1.4f, color);
            DebugVis::drawLine(
                camera, spawn.position,
                spawn.position + glm::vec3(0.0f, 0.0f, 1.5f), color);

            const glm::vec3 forward = spawn.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            DebugVis::drawLine(
                camera, spawn.position,
                spawn.position + forward * 1.2f,
                {0.25f, 0.65f, 1.0f, 1.0f});

            char label[192];
            snprintf(label, sizeof(label),
                     "SPAWN %zu%s %s (%.2f %.2f %.2f)",
                     i, selected ? " SELECTED" : "", spawn.tag.c_str(),
                     spawn.position.x, spawn.position.y, spawn.position.z);
            DebugVis::drawWorldLabel(
                spawn.position + glm::vec3(0.0f, 0.0f, 1.7f),
                label, color);
        }
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

        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 0);
    }
    flushDebugTris(camera);
    flushDebugLines(camera);
    drawDebugLabels(camera);
}

namespace DebugVis {
    void drawDiagnosticWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color) {
        ::drawWireSphere(camera, center, radius, color);
    }

    void drawDiagnosticLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color) {
        ::drawLine(camera, a, b, color);
    }

    void drawDiagnosticWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color) {
        ::drawWorldLabel(worldPos, text, color);
    }

    void drawWireSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color) {
        if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
        ::drawWireSphere(camera, center, radius, color);
    }

    void flushTris(const Camera& camera) {
        ::flushDebugTris(camera);
    }

    // CHANGED: Not gated behind masterEnabled — intended for production particles/blood, jun 6 2026
    void drawFilledDecal(const Camera& camera, glm::vec3 position, glm::vec3 normal, float radius, glm::vec4 color) {
        ::drawFilledDecal(camera, position, normal, radius, color);
    }

    void drawBloodDecal(const Camera& camera, glm::vec3 position, glm::vec3 normal,
                        float radius, float rotation, float stretch, glm::vec4 color) {
        ::drawBloodDecal(camera, position, normal, radius, rotation, stretch, color);
    }

    void drawFilledBillboard(const Camera& camera, glm::vec3 position, float size,
                             float rotation, float stretch, glm::vec4 color) {
        ::drawFilledBillboard(camera, position, size, rotation, stretch, color);
    }
    
    // Not gated behind masterEnabled — intended for production particles
    void drawFilledSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color, glm::vec3 scale) {
        ::drawFilledSphere(camera, center, radius, color, scale);
    }

    void drawFilledCylinder(const Camera& camera, glm::vec3 center, glm::vec3 axis, float radius, float height, glm::vec4 color) {
        ::drawFilledCylinder(camera, center, axis, radius, height, color);
    }

    void drawFilledBeam(const Camera& camera, glm::vec3 start, glm::vec3 end, float thickness, glm::vec4 color) {
        glm::vec3 delta = end - start;
        float length = glm::length(delta);
        if (length > 0.001f)
            ::drawFilledCylinder(camera, (start + end) * 0.5f, delta / length, thickness * 0.5f, length, color);
    }

    void drawFilledBox(const Camera& camera, glm::vec3 center, glm::vec3 halfSize, glm::vec4 color, glm::vec3 rotationEuler) {
        ::drawFilledBox(camera, center, halfSize, color, rotationEuler);
    }
    
    void drawLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color) {
        if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
        ::drawLine(camera, a, b, color);
    }

    void drawWireBox(const Camera& camera, glm::vec3 center, glm::vec3 halfSize, glm::vec4 color) {
        if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
        ::drawBox(camera, center, halfSize, color);
    }
    
    void drawWorldLabel(glm::vec3 worldPos, const char* text, glm::vec4 color) {
        if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
        ::drawWorldLabel(worldPos, text, color);
    }
    
    void drawPointCross(const Camera& camera, glm::vec3 p, float size, glm::vec4 color) {
        if (!DebugConfig::DEBUG_VISUALS_MASTER) return;
        ::drawPointCross(camera, p, size, color);
    }
    
    bool projectToScreen(const Camera& camera, glm::vec3 worldPos, float& x, float& y) {
        return ::projectToScreen(camera, worldPos, x, y);
    }
}
