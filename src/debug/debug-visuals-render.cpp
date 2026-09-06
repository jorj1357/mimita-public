#include "debug/debug-visuals.h"

#include <cmath>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "camera.h"

struct DebugTriVertex
{
    glm::vec3 pos;
    glm::vec4 color;
};

extern std::vector<DebugTriVertex> gTriVerts;
extern std::vector<DebugTriVertex> gOverlayTriVerts;

// =====================================================
// Solid triangle rendering
// =====================================================

void drawFilledDecal(const Camera& camera, glm::vec3 position, glm::vec3 normal, float radius, glm::vec4 color)
{
    (void)camera;
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
                gTriVerts.push_back({p01, color});
                gTriVerts.push_back({p10, color});
            }
            if (lat < LAT_SEGMENTS - 1) {
                gTriVerts.push_back({p01, color});
                gTriVerts.push_back({p11, color});
                gTriVerts.push_back({p10, color});
            }
        }
    }
}

static void impactBasis(const glm::vec3& direction, const char* localAxis,
                        glm::vec3& x, glm::vec3& y, glm::vec3& z)
{
    const glm::vec3 forward = glm::normalize(direction);
    const glm::vec3 guide = std::fabs(forward.z) < 0.9f
        ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 side = glm::normalize(glm::cross(guide, forward));
    const glm::vec3 up = glm::normalize(glm::cross(forward, side));
    if (localAxis && localAxis[0] == '+' && localAxis[1] == 'X') { x = forward; y = up; z = side; }
    else if (localAxis && localAxis[0] == '-' && localAxis[1] == 'X') { x = -forward; y = up; z = -side; }
    else if (localAxis && localAxis[0] == '+' && localAxis[1] == 'Y') { x = side; y = forward; z = up; }
    else if (localAxis && localAxis[0] == '-' && localAxis[1] == 'Y') { x = side; y = -forward; z = -up; }
    else if (localAxis && localAxis[0] == '-' && localAxis[1] == 'Z') { x = side; y = up; z = -forward; }
    else { x = side; y = up; z = forward; }
}

void drawFilledSphereOriented(const Camera& camera, glm::vec3 center, glm::vec3 direction,
                              float radius, glm::vec4 color, glm::vec3 scale,
                              const char* localAxis)
{
    if (glm::length(direction) <= 0.001f) { drawFilledSphere(camera, center, radius, color, scale); return; }
    glm::vec3 x, y, z;
    impactBasis(direction, localAxis, x, y, z);
    constexpr int LAT = 8;
    constexpr int LON = 12;
    auto world = [&](const glm::vec3& p) { return center + x * p.x + y * p.y + z * p.z; };
    for (int lat = 0; lat < LAT; ++lat) {
        const float a0 = 3.14159265f * lat / LAT;
        const float a1 = 3.14159265f * (lat + 1) / LAT;
        const float y0 = std::cos(a0), y1 = std::cos(a1);
        const float r0 = std::sin(a0), r1 = std::sin(a1);
        for (int lon = 0; lon < LON; ++lon) {
            const float b0 = 6.2831853f * lon / LON;
            const float b1 = 6.2831853f * ((lon + 1) % LON) / LON;
            const glm::vec3 p00 = world(scale * glm::vec3(radius*r0*std::sin(b0), radius*r0*std::cos(b0), radius*y0));
            const glm::vec3 p10 = world(scale * glm::vec3(radius*r1*std::sin(b0), radius*r1*std::cos(b0), radius*y1));
            const glm::vec3 p01 = world(scale * glm::vec3(radius*r0*std::sin(b1), radius*r0*std::cos(b1), radius*y0));
            const glm::vec3 p11 = world(scale * glm::vec3(radius*r1*std::sin(b1), radius*r1*std::cos(b1), radius*y1));
            if (lat > 0) { gTriVerts.push_back({p00,color}); gTriVerts.push_back({p01,color}); gTriVerts.push_back({p10,color}); }
            if (lat < LAT - 1) { gTriVerts.push_back({p01,color}); gTriVerts.push_back({p11,color}); gTriVerts.push_back({p10,color}); }
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

void drawFilledShockwave(const Camera&, glm::vec3 center, glm::vec3 normal,
                         float radius, float height, int points, float pointLength, float rotationDegrees, glm::vec4 color)
{
    const glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0, 0, 1);
    glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0, 0, 1)) : glm::cross(n, glm::vec3(0, 1, 0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
    const int count = std::clamp(points, 3, 64);
    const float inner = std::max(0.0f, radius * 0.42f);
    const float outer = std::max(inner, radius);
    const float tip = outer + std::max(0.0f, pointLength);
    const float halfHeight = std::max(0.001f, height * 0.5f);
    const float rotation = glm::radians(rotationDegrees);
    auto point = [&](float r, float angle, float z) {
        return center + tangent * (std::cos(angle) * r) + bitangent * (std::sin(angle) * r) + n * z;
    };
    for (int i = 0; i < count; ++i) {
        const float a0 = rotation + glm::two_pi<float>() * (float)i / (float)count;
        const float a1 = rotation + glm::two_pi<float>() * (float)(i + 1) / (float)count;
        const float amid = (a0 + a1) * 0.5f;
        const glm::vec3 p0 = point(inner, a0, halfHeight);
        const glm::vec3 p1 = point(outer, a0, halfHeight);
        const glm::vec3 pm = point(tip, amid, halfHeight);
        const glm::vec3 p2 = point(outer, a1, halfHeight);
        const glm::vec3 p3 = point(inner, a1, halfHeight);
        gTriVerts.push_back({p0, color}); gTriVerts.push_back({p1, color}); gTriVerts.push_back({pm, color});
        gTriVerts.push_back({p0, color}); gTriVerts.push_back({pm, color}); gTriVerts.push_back({p3, color});
        gTriVerts.push_back({point(inner, a0, -halfHeight), color}); gTriVerts.push_back({point(inner, a1, -halfHeight), color}); gTriVerts.push_back({point(tip, amid, -halfHeight), color});
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

namespace DebugVis {

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

void drawFilledSphere(const Camera& camera, glm::vec3 center, float radius, glm::vec4 color, glm::vec3 scale) {
    ::drawFilledSphere(camera, center, radius, color, scale);
}

void drawFilledSphereOriented(const Camera& camera, glm::vec3 center, glm::vec3 direction,
                              float radius, glm::vec4 color, glm::vec3 scale,
                              const char* localAxis) {
    ::drawFilledSphereOriented(camera, center, direction, radius, color, scale, localAxis);
}

void drawFilledCylinder(const Camera& camera, glm::vec3 center, glm::vec3 axis, float radius, float height, glm::vec4 color) {
    ::drawFilledCylinder(camera, center, axis, radius, height, color);
}

void drawFilledShockwave(const Camera& camera, glm::vec3 center, glm::vec3 normal,
                         float radius, float height, int points, float pointLength, float rotationDegrees, glm::vec4 color) {
    ::drawFilledShockwave(camera, center, normal, radius, height, points, pointLength, rotationDegrees, color);
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

} // namespace DebugVis

static void drawCrosshairRect(const Camera& camera, glm::vec3 center,
    glm::vec3 halfExtent, glm::vec4 color)
{
    glm::vec3 r = camera.right * halfExtent.x;
    glm::vec3 u = camera.up * halfExtent.y;
    glm::vec3 corners[4] = {
        center - r - u,
        center + r - u,
        center + r + u,
        center - r + u,
    };
    gTriVerts.push_back({corners[0], color});
    gTriVerts.push_back({corners[1], color});
    gTriVerts.push_back({corners[3], color});
    gTriVerts.push_back({corners[1], color});
    gTriVerts.push_back({corners[2], color});
    gTriVerts.push_back({corners[3], color});
}

namespace DebugVis {

void drawCrosshairBillboard(const Camera& camera, glm::vec3 position,
    float size, float gap, float thickness,
    bool showDot, glm::vec4 color)
{
    glm::vec3 hs = glm::vec3(size * 0.5f, thickness * 0.5f, 0.0f);
    glm::vec3 ht = glm::vec3(thickness * 0.5f, size * 0.5f, 0.0f);
    glm::vec3 hd = glm::vec3(thickness * 0.5f, thickness * 0.5f, 0.0f);
    drawCrosshairRect(camera, position + camera.right * (-gap - size * 0.5f), hs, color);
    drawCrosshairRect(camera, position + camera.right * ( gap + size * 0.5f), hs, color);
    drawCrosshairRect(camera, position + camera.up * (-gap - size * 0.5f), ht, color);
    drawCrosshairRect(camera, position + camera.up * ( gap + size * 0.5f), ht, color);
    if (showDot) {
        drawCrosshairRect(camera, position, hd, color);
    }
}

void drawCrosshairBillboardOverlay(const Camera& camera, glm::vec3 position,
    float size, float gap, float thickness,
    bool showDot, glm::vec4 color)
{
    auto drawRect = [&](glm::vec3 c, glm::vec3 he) {
        glm::vec3 r = camera.right * he.x;
        glm::vec3 u = camera.up * he.y;
        glm::vec3 corners[4] = {
            c - r - u, c + r - u, c + r + u, c - r + u,
        };
        gOverlayTriVerts.push_back({corners[0], color});
        gOverlayTriVerts.push_back({corners[1], color});
        gOverlayTriVerts.push_back({corners[3], color});
        gOverlayTriVerts.push_back({corners[1], color});
        gOverlayTriVerts.push_back({corners[2], color});
        gOverlayTriVerts.push_back({corners[3], color});
    };
    glm::vec3 hs = glm::vec3(size * 0.5f, thickness * 0.5f, 0.0f);
    glm::vec3 ht = glm::vec3(thickness * 0.5f, size * 0.5f, 0.0f);
    glm::vec3 hd = glm::vec3(thickness * 0.5f, thickness * 0.5f, 0.0f);
    drawRect(position + camera.right * (-gap - size * 0.5f), hs);
    drawRect(position + camera.right * ( gap + size * 0.5f), hs);
    drawRect(position + camera.up * (-gap - size * 0.5f), ht);
    drawRect(position + camera.up * ( gap + size * 0.5f), ht);
    if (showDot) drawRect(position, hd);
}

} // namespace DebugVis
