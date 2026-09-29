// 2026-09-29
/* purpose
* Implement planar destruction for an axis-aligned box.
* Each face is a rectangle; a cut crossing its plane removes an N-gon (default
* sixteen sided plus any rectangle corners that fall inside the sweep) from that
* rectangle, triangulated as an annulus. A cut that spans the box along an axis
* joins the two opposite rims with an inward-facing tube; otherwise the hole is
* closed by a shallow conical cap. Winding is always resolved against the
* intended surface normal so rendering culling and collision agree.
* Does NOT store cuts or run rigid-body motion.
*/

#include "impact/box-surface.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "impact/destructible-geometry.h"

namespace MimitaImpact {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr int kRimSides = 16;
constexpr size_t kTriangleCap = 20000;

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

// Canonical tangent frame for one box axis. `n` equals cross(e0, e1) and points
// along the positive axis, so the same 2D coordinates describe both the +s and
// -s faces and their rims stay index-aligned for the tube.
struct AxisFrame
{
    glm::vec3 e0{1.0f, 0.0f, 0.0f};
    glm::vec3 e1{0.0f, 1.0f, 0.0f};
    glm::vec3 n{0.0f, 0.0f, 1.0f};
    float halfU = 1.0f;
    float halfV = 1.0f;
    float halfA = 1.0f;
};

AxisFrame makeFrame(int axis, const glm::vec3& half)
{
    AxisFrame f;
    if (axis == 0)
    {
        f.e0 = {0.0f, 1.0f, 0.0f};
        f.e1 = {0.0f, 0.0f, 1.0f};
        f.n = {1.0f, 0.0f, 0.0f};
        f.halfU = half.y;
        f.halfV = half.z;
    }
    else if (axis == 1)
    {
        f.e0 = {0.0f, 0.0f, 1.0f};
        f.e1 = {1.0f, 0.0f, 0.0f};
        f.n = {0.0f, 1.0f, 0.0f};
        f.halfU = half.z;
        f.halfV = half.x;
    }
    else
    {
        f.e0 = {1.0f, 0.0f, 0.0f};
        f.e1 = {0.0f, 1.0f, 0.0f};
        f.n = {0.0f, 0.0f, 1.0f};
        f.halfU = half.x;
        f.halfV = half.y;
    }
    f.halfA = half[axis];
    return f;
}

struct Circle
{
    Vec2 center;
    float radius = 0.0f;
    float penetration = 0.0f;
    uint64_t seed = 0;
};

struct EmitContext
{
    std::vector<Vertex>* render = nullptr;
    std::vector<CollisionTriangle>* collision = nullptr;
    bool truncated = false;
};

glm::vec2 faceUV(const Vec2& p, const AxisFrame& f)
{
    const float invU = f.halfU > 1e-6f ? 1.0f / (2.0f * f.halfU) : 0.0f;
    const float invV = f.halfV > 1e-6f ? 1.0f / (2.0f * f.halfV) : 0.0f;
    return glm::vec2((p.x + f.halfU) * invU, (p.y + f.halfV) * invV);
}

void pushTriangle(EmitContext& ctx, glm::vec3 a, glm::vec3 b, glm::vec3 c,
                  const glm::vec3& desired, glm::vec2 uvA,
                  glm::vec2 uvB, glm::vec2 uvC)
{
    glm::vec3 geo = glm::cross(b - a, c - a);
    const float len = glm::length(geo);
    if (len < 1e-9f)
        return;
    geo /= len;
    if (glm::dot(geo, desired) < 0.0f)
    {
        std::swap(b, c);
        std::swap(uvB, uvC);
        geo = -geo;
    }
    if (ctx.collision->size() >= kTriangleCap)
    {
        ctx.truncated = true;
        return;
    }
    CollisionTriangle tri;
    tri.a = a;
    tri.b = b;
    tri.c = c;
    tri.normal = geo;
    ctx.collision->push_back(tri);
    ctx.render->push_back({a, geo, uvA});
    ctx.render->push_back({b, geo, uvB});
    ctx.render->push_back({c, geo, uvC});
}

bool axisIsSpanned(int axis, const AxisFrame& f,
                   const std::vector<DestructionCutSphere>& cuts)
{
    if (cuts.empty())
        return false;
    std::vector<std::pair<float, float>> intervals;
    intervals.reserve(cuts.size());
    for (const DestructionCutSphere& cut : cuts)
        intervals.push_back({cut.localCenter[axis] - cut.radius,
                             cut.localCenter[axis] + cut.radius});
    std::sort(intervals.begin(), intervals.end());
    float lo = intervals[0].first;
    float hi = intervals[0].second;
    auto covers = [&]() {
        return lo <= -f.halfA + 1e-5f && hi >= f.halfA - 1e-5f;
    };
    for (size_t i = 1; i < intervals.size(); ++i)
    {
        if (intervals[i].first <= hi)
            hi = std::max(hi, intervals[i].second);
        else
        {
            if (covers())
                return true;
            lo = intervals[i].first;
            hi = intervals[i].second;
        }
    }
    return covers();
}

void collectCircles(int axis, int sign, const AxisFrame& f,
                    const std::vector<DestructionCutSphere>& cuts,
                    std::vector<Circle>& out)
{
    for (const DestructionCutSphere& cut : cuts)
    {
        const float d = (float)sign * f.halfA - cut.localCenter[axis];
        if (std::fabs(d) > cut.radius)
            continue;
        const float r2 = cut.radius * cut.radius - d * d;
        if (r2 <= 1e-8f)
            continue;
        Circle c;
        c.center = Vec2{glm::dot(cut.localCenter, f.e0), glm::dot(cut.localCenter, f.e1)};
        c.radius = std::sqrt(r2);
        c.penetration = std::fabs(d) + cut.radius;
        c.seed = cut.cutId;
        out.push_back(c);
    }
}

// Rim (hole edge) and outer (rectangle boundary) point for every sweep angle.
void buildRim(const AxisFrame& f, const Vec2& center, float phase,
              float phase2, const std::vector<float>& angles,
              const std::vector<Circle>& circles,
              std::vector<Vec2>& rim, std::vector<Vec2>& outer)
{
    rim.reserve(angles.size());
    outer.reserve(angles.size());
    for (float theta : angles)
    {
        const Vec2 dir{std::cos(theta), std::sin(theta)};
        float farthest = -1e30f;
        for (const Circle& c : circles)
        {
            const Vec2 g{center.x - c.center.x, center.y - c.center.y};
            const float b = 2.0f * (g.x * dir.x + g.y * dir.y);
            const float cc = g.x * g.x + g.y * g.y - c.radius * c.radius;
            const float disc = b * b - 4.0f * cc;
            if (disc < 0.0f)
                continue;
            const float t = (-b + std::sqrt(disc)) * 0.5f;
            if (t > farthest)
                farthest = t;
        }
        const float jitter =
            1.0f + 0.06f * std::sin(3.0f * theta + phase) +
            0.035f * std::sin(5.0f * theta + phase2);
        const float hole = farthest > -1e29f ? farthest * jitter : 0.0f;

        float toBox = 1e30f;
        if (std::fabs(dir.x) > 1e-8f)
            toBox = std::min(toBox, ((dir.x > 0.0f ? f.halfU : -f.halfU) - center.x) / dir.x);
        if (std::fabs(dir.y) > 1e-8f)
            toBox = std::min(toBox, ((dir.y > 0.0f ? f.halfV : -f.halfV) - center.y) / dir.y);
        if (toBox < 0.0f)
            toBox = 0.0f;

        const float inner = std::max(0.0f, std::min(hole, toBox * 0.995f));
        rim.push_back(Vec2{center.x + inner * dir.x, center.y + inner * dir.y});
        outer.push_back(Vec2{center.x + toBox * dir.x, center.y + toBox * dir.y});
    }
}

void emitAxis(int axis, const glm::vec3& half,
              const std::vector<DestructionCutSphere>& cuts, EmitContext& ctx)
{
    const AxisFrame f = makeFrame(axis, half);

    std::vector<Circle> plus;
    std::vector<Circle> minus;
    collectCircles(axis, +1, f, cuts, plus);
    collectCircles(axis, -1, f, cuts, minus);

    const bool spanned = axisIsSpanned(axis, f, cuts);

    auto mapFace = [&](const Vec2& p, int sign) {
        return f.e0 * p.x + f.e1 * p.y + f.n * ((float)sign * f.halfA);
    };

    // One shared rim centre and angle set for both faces so the tube lines up.
    Vec2 center{0.0f, 0.0f};
    uint64_t seed = 0;
    float bestRadius = -1.0f;
    auto scan = [&](const std::vector<Circle>& v) {
        for (const Circle& c : v)
            if (c.radius > bestRadius)
            {
                bestRadius = c.radius;
                center = c.center;
                seed = c.seed;
            }
    };
    scan(plus);
    scan(minus);

    const bool hasHole = bestRadius >= 0.0f;
    const float margin = 1e-3f;
    center.x = std::clamp(center.x, -f.halfU + margin, f.halfU - margin);
    center.y = std::clamp(center.y, -f.halfV + margin, f.halfV - margin);

    std::vector<float> angles;
    if (hasHole)
    {
        for (int i = 0; i < kRimSides; ++i)
            angles.push_back(2.0f * kPi * (float)i / (float)kRimSides);
        const Vec2 corners[4] = {{-f.halfU, -f.halfV}, {f.halfU, -f.halfV},
                                 {f.halfU, f.halfV}, {-f.halfU, f.halfV}};
        for (const Vec2& corner : corners)
            angles.push_back(std::atan2(corner.y - center.y, corner.x - center.x));
        std::sort(angles.begin(), angles.end());
        std::vector<float> unique;
        for (float theta : angles)
            if (unique.empty() || std::fabs(theta - unique.back()) > 1e-3f)
                unique.push_back(theta);
        angles.swap(unique);
    }

    const float phase = (float)(seed % 997u) * 0.0063019f;
    const float phase2 = (float)((seed / 7u) % 991u) * 0.0063392f;

    auto emitFace = [&](const std::vector<Circle>& circles, int sign) {
        const glm::vec3 desired = f.n * (float)sign;
        if (circles.empty())
        {
            const Vec2 q[4] = {{-f.halfU, -f.halfV}, {f.halfU, -f.halfV},
                               {f.halfU, f.halfV}, {-f.halfU, f.halfV}};
            const glm::vec3 p0 = mapFace(q[0], sign);
            const glm::vec3 p1 = mapFace(q[1], sign);
            const glm::vec3 p2 = mapFace(q[2], sign);
            const glm::vec3 p3 = mapFace(q[3], sign);
            pushTriangle(ctx, p0, p1, p2, desired,
                         glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f));
            pushTriangle(ctx, p0, p2, p3, desired,
                         glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f));
            return;
        }

        std::vector<Vec2> rim;
        std::vector<Vec2> outer;
        buildRim(f, center, phase, phase2, angles, circles, rim, outer);
        const size_t count = rim.size();
        for (size_t i = 0; i < count; ++i)
        {
            const size_t j = (i + 1) % count;
            const glm::vec3 oi = mapFace(outer[i], sign);
            const glm::vec3 oj = mapFace(outer[j], sign);
            const glm::vec3 pj = mapFace(rim[j], sign);
            const glm::vec3 pi = mapFace(rim[i], sign);
            pushTriangle(ctx, oi, oj, pj, desired,
                         faceUV(outer[i], f), faceUV(outer[j], f), faceUV(rim[j], f));
            pushTriangle(ctx, oi, pj, pi, desired,
                         faceUV(outer[i], f), faceUV(rim[j], f), faceUV(rim[i], f));
        }

        if (!spanned)
        {
            float penetration = 0.0f;
            for (const Circle& c : circles)
                penetration = std::max(penetration, c.penetration);
            const float apexA =
                (float)sign * (f.halfA - std::clamp(penetration, 0.0f, 2.0f * f.halfA));
            const glm::vec3 apex =
                f.e0 * center.x + f.e1 * center.y + f.n * apexA;
            for (size_t i = 0; i < count; ++i)
            {
                const size_t j = (i + 1) % count;
                pushTriangle(ctx, apex, mapFace(rim[i], sign), mapFace(rim[j], sign),
                             desired, faceUV(center, f), faceUV(rim[i], f), faceUV(rim[j], f));
            }
        }
    };

    emitFace(plus, +1);
    emitFace(minus, -1);

    if (spanned && !plus.empty() && !minus.empty())
    {
        std::vector<Vec2> rimPlus, outerPlus, rimMinus, outerMinus;
        buildRim(f, center, phase, phase2, angles, plus, rimPlus, outerPlus);
        buildRim(f, center, phase, phase2, angles, minus, rimMinus, outerMinus);
        const size_t count = rimPlus.size();
        for (size_t i = 0; i < count; ++i)
        {
            const size_t j = (i + 1) % count;
            const glm::vec3 a0 = mapFace(rimPlus[i], +1);
            const glm::vec3 a1 = mapFace(rimPlus[j], +1);
            const glm::vec3 b0 = mapFace(rimMinus[i], -1);
            const glm::vec3 b1 = mapFace(rimMinus[j], -1);
            const glm::vec3 centroid = (a0 + a1 + b0 + b1) * 0.25f;
            const glm::vec3 axisPoint =
                f.e0 * center.x + f.e1 * center.y + f.n * glm::dot(centroid, f.n);
            glm::vec3 inward = axisPoint - centroid;
            if (glm::dot(inward, inward) < 1e-12f)
                inward = -f.n;
            const float invSpan = 1.0f / std::max(2.0f * f.halfA, 1e-4f);
            auto tubeUV = [&](const glm::vec3& p) {
                return glm::vec2((float)i / (float)count,
                                 (glm::dot(p, f.n) + f.halfA) * invSpan);
            };
            pushTriangle(ctx, a0, a1, b1, inward, tubeUV(a0), tubeUV(a1), tubeUV(b1));
            pushTriangle(ctx, a0, b1, b0, inward, tubeUV(a0), tubeUV(b1), tubeUV(b0));
        }
    }
}

} // anonymous namespace

void buildDestructibleBoxSurface(
    const glm::vec3& halfExtents,
    const std::vector<DestructionCutSphere>& cuts,
    std::vector<Vertex>& outRenderVertices,
    std::vector<CollisionTriangle>& outCollisionTriangles)
{
    outRenderVertices.clear();
    outCollisionTriangles.clear();

    EmitContext ctx;
    ctx.render = &outRenderVertices;
    ctx.collision = &outCollisionTriangles;

    for (int axis = 0; axis < 3; ++axis)
        emitAxis(axis, halfExtents, cuts, ctx);
}

} // namespace MimitaImpact
