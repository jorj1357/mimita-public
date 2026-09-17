// 09 17 2026
/* purpose
* Hot world-geometry helpers used by the ragdoll solver (and any hot consumer
* that wants a simple grid cache). The movement/actor collision solve lives in
* the `collision.main` package (`src/hot-reload/packages/collision/`), which owns
* its own multi-resolution broadphase; this header is not that owner.
* Fully live-editable: edit and save, no EXE rebuild.
* The world triangle grid is cached hot-side and rebuilt when the map changes.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "hot-reload/game-api.h"

namespace HotCollision {

struct Tri { glm::vec3 a, b, c; };

inline std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>& grid()
{
    static std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> g;
    return g;
}
inline std::vector<Tri>& tris()
{
    static std::vector<Tri> t;
    return t;
}
// Triangles too large to index by a single cell are always tested.
inline std::vector<std::uint32_t>& large()
{
    static std::vector<std::uint32_t> v;
    return v;
}
inline std::uint32_t& cachedTotal()
{
    static std::uint32_t n = 0;
    return n;
}

// World grid cell size in world units. Shared by the index build and queries.
inline constexpr float kCellSize = 2.0f;

inline std::uint64_t cellKey(int x, int y, int z)
{
    return ((std::uint64_t)(x + 4096) & 0x1fffff) |
           (((std::uint64_t)(y + 4096) & 0x1fffff) << 21) |
           (((std::uint64_t)(z + 4096) & 0x1fffff) << 42);
}

// Fetch and cache the world collision triangles from the kernel capability.
inline bool ensureWorld(void* host)
{
    if (!host)
        return !tris().empty();
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx->resolveCapability)
        return !tris().empty();
    using Fn = void (MIMITA_GAME_CALL *)(void*, GameWorldCollisionPageV1*);
    auto fn = reinterpret_cast<Fn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_WORLD_COLLISION));
    if (!fn)
        return !tris().empty();

    std::vector<GameCollisionTriangleV1> buf(4096);
    GameWorldCollisionPageV1 probe{};
    probe.offset = 0;
    probe.maxTriangles = (std::uint32_t)buf.size();
    probe.out = buf.data();
    fn(ctx->host, &probe);  // total + first page
    if (probe.total == 0)
        return false;
    if (!tris().empty() && cachedTotal() == probe.total)
        return true;

    tris().clear();
    grid().clear();
    large().clear();
    cachedTotal() = probe.total;
    for (std::uint32_t off = 0; off < probe.total;) {
        GameWorldCollisionPageV1 q{};
        q.offset = off;
        q.maxTriangles = (std::uint32_t)buf.size();
        q.out = buf.data();
        fn(ctx->host, &q);
        if (q.count == 0)
            break;
        for (std::uint32_t i = 0; i < q.count; ++i) {
            Tri t{{buf[i].a[0], buf[i].a[1], buf[i].a[2]},
                  {buf[i].b[0], buf[i].b[1], buf[i].b[2]},
                  {buf[i].c[0], buf[i].c[1], buf[i].c[2]}};
            const std::uint32_t idx = (std::uint32_t)tris().size();
            tris().push_back(t);
            const glm::vec3 mn = glm::min(glm::min(t.a, t.b), t.c);
            const glm::vec3 mx = glm::max(glm::max(t.a, t.b), t.c);
            if (mx.x - mn.x > kCellSize || mx.y - mn.y > kCellSize ||
                mx.z - mn.z > kCellSize) {
                large().push_back(idx);
            } else {
                const glm::vec3 c = (t.a + t.b + t.c) / 3.0f;
                grid()[cellKey((int)std::floor(c.x / kCellSize),
                               (int)std::floor(c.y / kCellSize),
                               (int)std::floor(c.z / kCellSize))]
                    .push_back(idx);
            }
        }
        off += q.count;
    }
    return !tris().empty();
}

inline glm::vec3 closestPointOnTri(const glm::vec3& p, const Tri& t)
{
    const glm::vec3 ab = t.b - t.a, ac = t.c - t.a, ap = p - t.a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return t.a;
    const glm::vec3 bp = p - t.b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return t.b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        return t.a + ab * (d1 / (d1 - d3));
    const glm::vec3 cp = p - t.c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return t.c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        return t.a + ac * (d2 / (d2 - d6));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return t.b + (t.c - t.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const float denom = 1.0f / (va + vb + vc);
    return t.a + ab * (vb * denom) + ac * (vc * denom);
}

// One sphere/triangle overlap: surface point, outward normal, and depth.
struct TriContact {
    int tri;
    glm::vec3 point;
    glm::vec3 normal;
    float penetration;
};

// Append every triangle the sphere overlaps (deepest first is not required).
// Returns the number of contacts written (capped at maxOut).
inline int gatherSphereContacts(const glm::vec3& p, float r, TriContact* out,
                                int maxOut)
{
    int n = 0;
    auto test = [&](std::uint32_t idx) {
        if (n >= maxOut) return;
        const glm::vec3 cp = closestPointOnTri(p, tris()[idx]);
        const glm::vec3 d = p - cp;
        const float dist = glm::length(d);
        if (dist >= r || dist < 1e-6f) return;
        out[n].tri = (int)idx;
        out[n].point = cp;
        out[n].normal = d / dist;
        out[n].penetration = r - dist;
        ++n;
    };

    const glm::ivec3 c0((int)std::floor((p.x - r) / kCellSize),
                        (int)std::floor((p.y - r) / kCellSize),
                        (int)std::floor((p.z - r) / kCellSize));
    const glm::ivec3 c1((int)std::floor((p.x + r) / kCellSize),
                        (int)std::floor((p.y + r) / kCellSize),
                        (int)std::floor((p.z + r) / kCellSize));
    for (int x = c0.x; x <= c1.x && n < maxOut; ++x)
    for (int y = c0.y; y <= c1.y && n < maxOut; ++y)
    for (int z = c0.z; z <= c1.z && n < maxOut; ++z) {
        auto it = grid().find(cellKey(x, y, z));
        if (it == grid().end()) continue;
        for (std::uint32_t idx : it->second) test(idx);
    }
    // Large triangles (e.g. floors) are not cell-indexed; always test them.
    for (std::uint32_t idx : large()) {
        if (n >= maxOut) break;
        test(idx);
    }
    return n;
}

} // namespace HotCollision

#endif // MIMITA_GAME_DLL
