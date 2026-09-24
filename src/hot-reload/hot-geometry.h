// 09 23 2026
/* purpose
* Define the generic, hot-replaceable geometry primitive library and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the callers (collision, weapon execution, movement validation) and the
* data; a hot module owns the primitive math so primitives can be edited live
* and new ones added without a rebuild.
* POD only: fixed-size float arrays; no STL or engine objects cross the boundary.
* Does NOT own entities, world storage, or gameplay policy.
*/
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Primitive ids. Stable hashes; a new primitive is a new registration, not a new
// kernel call site. Callers resolve by id through the generic doorway.
static constexpr std::uint64_t GAME_GEOM_RAY_AABB = gameHash("geom.ray.aabb");
static constexpr std::uint64_t GAME_GEOM_RAY_TRIANGLE = gameHash("geom.ray.triangle");
static constexpr std::uint64_t GAME_GEOM_SWEPT_POINT_SPHERE =
    gameHash("geom.swept.point-sphere");
static constexpr std::uint64_t GAME_GEOM_POINT_IN_AABB = gameHash("geom.point.aabb");
static constexpr std::uint64_t GAME_GEOM_CLOSEST_POINT_SEGMENT =
    gameHash("geom.closest.segment-point");

// Generic geometry query. The struct is the shared shape for every primitive:
// the primitive reads the fields it needs and writes `result`/outputs. A new
// primitive uses the same struct (extend only via the reserved slots).
struct GameGeometryQueryV1 {
    std::uint32_t structSize;
    std::uint64_t primitiveId;

    // Primary ray / segment.
    float origin[3];
    float direction[3];
    float maxDistance;

    // Segment (swept) or second point.
    float pointA[3];
    float pointB[3];

    // Box.
    float boxMin[3];
    float boxMax[3];

    // Triangle.
    float triA[3];
    float triB[3];
    float triC[3];

    // Sphere.
    float sphereCenter[3];
    float radius;
    float targetRadius;
    float tolerance;

    // Point.
    float point[3];

    // out
    std::uint32_t hit;            // 1 = the primitive hit/contained
    float outDistance;
    float outPoint[3];
    float outNormal[3];
    float outClosest[3];
    std::uint32_t result;         // 1 = the primitive ran
    std::uint32_t reserved;
};

using GameGeometryFn = void (MIMITA_GAME_CALL *)(void* host, GameGeometryQueryV1* request);

// A primitive is a named callable. Registering a new one is a hot source edit.
struct GameGeometryPrimitiveV1 {
    std::uint64_t primitiveId;
    std::uint32_t version;
    std::uint32_t reserved;
    GameGeometryFn invoke;
    const char* name;
};

using GameGeometryLookupFn = const GameGeometryPrimitiveV1* (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t primitiveId);

static constexpr std::uint64_t GAME_CAP_GEOMETRY = gameHash("net.geometry");
static constexpr std::uint64_t GAME_SIG_GEOMETRY = gameHash("sig.net.geometry.v1");

// Cold-side dispatcher: run one primitive through the hot provider if present,
// otherwise the shared fallback. Never cached across a generation swap. The
// implementation that needs GenericRuntime lives in the .cpp bridge so this
// header stays dependency-light for the DLL.
void runGeometryPrimitive(GameGeometryQueryV1& query);

// ── The single shared implementation ────────────────────────────────
namespace HotGeometryImpl {

inline float* normal_out(GameGeometryQueryV1& r) { return r.outNormal; }

inline void finish(GameGeometryQueryV1& r, bool hit)
{
    r.hit = hit ? 1u : 0u;
    r.result = 1u;
}

inline void rayAabb(GameGeometryQueryV1& r)
{
    float tmin = 0.0f;
    float tmax = r.maxDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(r.direction[axis]) < 0.000001f) {
            if (r.origin[axis] < r.boxMin[axis] || r.origin[axis] > r.boxMax[axis]) {
                finish(r, false);
                return;
            }
            continue;
        }
        const float inv = 1.0f / r.direction[axis];
        float t1 = (r.boxMin[axis] - r.origin[axis]) * inv;
        float t2 = (r.boxMax[axis] - r.origin[axis]) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) { finish(r, false); return; }
    }
    r.outDistance = tmin;
    const bool ok = tmin >= 0.0f && tmin <= r.maxDistance;
    if (ok) {
        for (int i = 0; i < 3; ++i)
            r.outPoint[i] = r.origin[i] + r.direction[i] * tmin;
    }
    finish(r, ok);
}

inline void rayTriangle(GameGeometryQueryV1& r)
{
    const float e1[3] = {r.triB[0]-r.triA[0], r.triB[1]-r.triA[1], r.triB[2]-r.triA[2]};
    const float e2[3] = {r.triC[0]-r.triA[0], r.triC[1]-r.triA[1], r.triC[2]-r.triA[2]};
    const float p[3] = {
        r.direction[1]*e2[2]-r.direction[2]*e2[1],
        r.direction[2]*e2[0]-r.direction[0]*e2[2],
        r.direction[0]*e2[1]-r.direction[1]*e2[0]};
    const float det = e1[0]*p[0]+e1[1]*p[1]+e1[2]*p[2];
    if (std::fabs(det) < 0.000001f) { finish(r, false); return; }
    const float inv = 1.0f / det;
    const float t[3] = {r.origin[0]-r.triA[0], r.origin[1]-r.triA[1], r.origin[2]-r.triA[2]};
    const float u = (t[0]*p[0]+t[1]*p[1]+t[2]*p[2]) * inv;
    if (u < 0.0f || u > 1.0f) { finish(r, false); return; }
    const float q[3] = {
        t[1]*e1[2]-t[2]*e1[1],
        t[2]*e1[0]-t[0]*e1[2],
        t[0]*e1[1]-t[1]*e1[0]};
    const float v = (r.direction[0]*q[0]+r.direction[1]*q[1]+r.direction[2]*q[2]) * inv;
    if (v < 0.0f || u + v > 1.0f) { finish(r, false); return; }
    const float dist = (e2[0]*q[0]+e2[1]*q[1]+e2[2]*q[2]) * inv;
    r.outDistance = dist;
    const bool ok = dist >= 0.0f && dist <= r.maxDistance;
    if (ok) {
        for (int i = 0; i < 3; ++i)
            r.outPoint[i] = r.origin[i] + r.direction[i] * dist;
    }
    finish(r, ok);
}

inline void closestOnSegment(GameGeometryQueryV1& r)
{
    const float ab[3] = {r.pointB[0]-r.pointA[0], r.pointB[1]-r.pointA[1], r.pointB[2]-r.pointA[2]};
    const float len2 = ab[0]*ab[0]+ab[1]*ab[1]+ab[2]*ab[2];
    float t = 0.0f;
    if (len2 > 0.000001f) {
        const float ap[3] = {r.point[0]-r.pointA[0], r.point[1]-r.pointA[1], r.point[2]-r.pointA[2]};
        t = (ap[0]*ab[0]+ap[1]*ab[1]+ap[2]*ab[2]) / len2;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    for (int i = 0; i < 3; ++i)
        r.outClosest[i] = r.pointA[i] + ab[i] * t;
    finish(r, true);
}

inline void pointInAabb(GameGeometryQueryV1& r)
{
    const bool inside =
        r.point[0] >= r.boxMin[0] && r.point[0] <= r.boxMax[0] &&
        r.point[1] >= r.boxMin[1] && r.point[1] <= r.boxMax[1] &&
        r.point[2] >= r.boxMin[2] && r.point[2] <= r.boxMax[2];
    finish(r, inside);
}

inline void sweptPointSphere(GameGeometryQueryV1& r)
{
    GameGeometryQueryV1 q = r;
    q.primitiveId = GAME_GEOM_CLOSEST_POINT_SEGMENT;
    for (int i = 0; i < 3; ++i) q.point[i] = r.sphereCenter[i];
    closestOnSegment(q);
    const float d[3] = {
        r.sphereCenter[0]-q.outClosest[0],
        r.sphereCenter[1]-q.outClosest[1],
        r.sphereCenter[2]-q.outClosest[2]};
    const float distance = std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
    const float sumRadius = r.radius + std::max(r.targetRadius, 0.01f);
    if (distance > sumRadius) { finish(r, false); return; }
    r.outDistance = distance;
    const float len = distance > 0.0001f ? distance : 0.0f;
    for (int i = 0; i < 3; ++i)
        r.outNormal[i] = len > 0.0f ? d[i] / len : (i == 2 ? 1.0f : 0.0f);
    for (int i = 0; i < 3; ++i)
        r.outPoint[i] = q.outClosest[i] + r.outNormal[i] * r.radius;
    finish(r, true);
}

} // namespace HotGeometryImpl

} // namespace MimitaNet
