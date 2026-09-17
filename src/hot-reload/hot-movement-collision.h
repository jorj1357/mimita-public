// 09 16 2026
/* purpose
* Hot capsule-vs-world movement collision. Replaces the cold physics.move
* pipeline for the hot movement system: it integrates gravity and resolves the
* actor capsule against the world collision triangles (fetched from the
* `world.collision` capability) with swept substepping, slide, and a grounded
* heuristic. Fully live-editable: edit and save, no EXE rebuild.
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
    constexpr float kCell = 2.0f;
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
            if (mx.x - mn.x > kCell || mx.y - mn.y > kCell || mx.z - mn.z > kCell) {
                large().push_back(idx);
            } else {
                const glm::vec3 c = (t.a + t.b + t.c) / 3.0f;
                grid()[cellKey((int)std::floor(c.x / kCell),
                               (int)std::floor(c.y / kCell),
                               (int)std::floor(c.z / kCell))]
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

// Resolve one sphere sample against nearby triangles. Returns true on contact;
// pushes the sphere out and returns the contact normal in `outNormal`.
inline bool resolveSphere(glm::vec3& p, float r, glm::vec3& outNormal)
{
    bool hit = false;
    const glm::ivec3 c0((int)std::floor((p.x - r) / 2.0f),
                        (int)std::floor((p.y - r) / 2.0f),
                        (int)std::floor((p.z - r) / 2.0f));
    const glm::ivec3 c1((int)std::floor((p.x + r) / 2.0f),
                        (int)std::floor((p.y + r) / 2.0f),
                        (int)std::floor((p.z + r) / 2.0f));
    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z) {
        auto it = grid().find(cellKey(x, y, z));
        if (it == grid().end()) continue;
        for (std::uint32_t idx : it->second) {
            const glm::vec3 cp = closestPointOnTri(p, tris()[idx]);
            const glm::vec3 d = p - cp;
            const float dist = glm::length(d);
            if (dist >= r || dist < 1e-6f) continue;
            const glm::vec3 n = d / dist;
            p += n * (r - dist);
            outNormal = n;
            hit = true;
        }
    }
    // Large triangles (e.g. floors) are not cell-indexed; always test them.
    for (std::uint32_t idx : large()) {
        const glm::vec3 cp = closestPointOnTri(p, tris()[idx]);
        const glm::vec3 d = p - cp;
        const float dist = glm::length(d);
        if (dist >= r || dist < 1e-6f) continue;
        const glm::vec3 n = d / dist;
        p += n * (r - dist);
        outNormal = n;
        hit = true;
    }
    return hit;
}

// Hot replacement for physics.move on the actor capsule. Returns false when no
// world collision data is available so the caller can fall back.
inline bool hotMoveCapsule(void* host, MovementStateV1* st, float dt)
{
    if (!st || dt <= 0.0f)
        return false;
    if (!ensureWorld(host))
        return false;

    const float radius = st->radius > 0.0f ? st->radius : 0.4f;
    const float tipHalf = st->halfHeight > 0.0f ? st->halfHeight : 0.5f;
    const float segHalf = tipHalf > radius ? tipHalf - radius : 0.0f;

    glm::vec3 pos(st->position[0], st->position[1], st->position[2]);
    glm::vec3 vel(st->velocity[0], st->velocity[1], st->velocity[2]);

    // gravityScale < 0 means the caller already integrated gravity.
    const float gScale = st->gravityScale < 0.0f
        ? 0.0f
        : (st->gravityScale > 0.0f ? st->gravityScale : 1.0f);
    vel.z -= 9.81f * gScale * dt;

    const float speed = glm::length(vel);
    const float maxStep = std::max(radius * 0.4f, 0.05f);
    int steps = (int)std::ceil(speed * dt / maxStep);
    if (steps < 1) steps = 1;
    if (steps > 16) steps = 16;
    const glm::vec3 move = vel * (dt / (float)steps);

    bool contacted = false;
    glm::vec3 contactNormal(0.0f);
    for (int s = 0; s < steps; ++s) {
        pos += move;
        glm::vec3 n(0.0f);
        bool hit = false;
        // Sample the capsule as three spheres along its vertical axis.
        const glm::vec3 samples[3] = {
            pos + glm::vec3(0.0f, 0.0f, segHalf),
            pos,
            pos - glm::vec3(0.0f, 0.0f, segHalf)};
        for (const glm::vec3& sp : samples) {
            glm::vec3 p = sp;
            glm::vec3 hn(0.0f);
            if (resolveSphere(p, radius, hn)) {
                pos += p - sp;  // apply the push-out to the capsule center
                n = hn;
                hit = true;
            }
        }
        if (hit) {
            // Slide: remove velocity into the surface.
            const float vn = glm::dot(vel, n);
            if (vn < 0.0f)
                vel -= n * vn;
            contactNormal = n;
            contacted = true;
        }
    }

    st->position[0] = pos.x; st->position[1] = pos.y; st->position[2] = pos.z;
    st->velocity[0] = vel.x; st->velocity[1] = vel.y; st->velocity[2] = vel.z;
    st->collided = contacted ? 1u : 0u;
    if (contacted && contactNormal.z > 0.5f &&
        vel.z > -0.05f && vel.z < 0.05f)
        st->grounded = 1u;
    else if (!contacted)
        st->grounded = 0u;
    return true;
}

} // namespace HotCollision

#endif // MIMITA_GAME_DLL
