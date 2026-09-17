// 09 17 2026
/* purpose
* Implements the collision package world cache, multi-resolution cell index, and
* union-AABB candidate gather described in `collision-world.h`.
* Does NOT own collision response or movement.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/packages/collision/collision-world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "hot-reload/game-api.h"

namespace HotCollisionPackage {

namespace {

constexpr float kFineCell = 2.0f;
constexpr float kCoarseCell = 8.0f;
constexpr std::int64_t kMaxFineCells = 256;
constexpr std::int64_t kMaxCoarseCells = 256;
constexpr std::int64_t kMaxCellsPerAxis = 128;

std::uint64_t cellKey(int x, int y, int z)
{
    return ((std::uint64_t)(x + 4096) & 0x1fffff) |
           (((std::uint64_t)(y + 4096) & 0x1fffff) << 21) |
           (((std::uint64_t)(z + 4096) & 0x1fffff) << 42);
}

void cellRange(const glm::vec3& mn, const glm::vec3& mx, float cell,
               glm::ivec3& c0, glm::ivec3& c1)
{
    c0 = glm::ivec3((int)std::floor(mn.x / cell), (int)std::floor(mn.y / cell),
                    (int)std::floor(mn.z / cell));
    c1 = glm::ivec3((int)std::floor(mx.x / cell), (int)std::floor(mx.y / cell),
                    (int)std::floor(mx.z / cell));
    for (int a = 0; a < 3; ++a) {
        if ((std::int64_t)c1[a] - (std::int64_t)c0[a] + 1 > kMaxCellsPerAxis)
            c1[a] = c0[a] + (int)kMaxCellsPerAxis - 1;
    }
}

void insertCells(std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>& grid,
                 const glm::ivec3& c0, const glm::ivec3& c1, std::uint32_t index)
{
    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z)
        grid[cellKey(x, y, z)].push_back(index);
}

void rebuildIndex(WorldCache& c)
{
    c.fine.clear();
    c.coarse.clear();
    c.always.clear();
    c.visitStamp.assign(c.tris.size(), 0u);
    c.stamp = 0;
    auto finiteTri = [](const WorldTri& t) {
        const float* v = &t.a.x;
        for (int i = 0; i < 9; ++i)
            if (!std::isfinite(v[i]))
                return false;
        return true;
    };
    for (std::uint32_t i = 0; i < (std::uint32_t)c.tris.size(); ++i) {
        const WorldTri& t = c.tris[i];
        if (!finiteTri(t))
            continue;   // invalid geometry is never indexed
        const glm::vec3 mn = glm::min(glm::min(t.a, t.b), t.c);
        const glm::vec3 mx = glm::max(glm::max(t.a, t.b), t.c);
        glm::ivec3 f0, f1;
        cellRange(mn, mx, kFineCell, f0, f1);
        const std::int64_t fineCells =
            ((std::int64_t)f1.x - f0.x + 1) *
            ((std::int64_t)f1.y - f0.y + 1) *
            ((std::int64_t)f1.z - f0.z + 1);
        if (fineCells <= kMaxFineCells) {
            insertCells(c.fine, f0, f1, i);
            continue;
        }
        glm::ivec3 c0, c1;
        cellRange(mn, mx, kCoarseCell, c0, c1);
        const std::int64_t coarseCells =
            ((std::int64_t)c1.x - c0.x + 1) *
            ((std::int64_t)c1.y - c0.y + 1) *
            ((std::int64_t)c1.z - c0.z + 1);
        if (coarseCells <= kMaxCoarseCells) {
            insertCells(c.coarse, c0, c1, i);
            continue;
        }
        c.always.push_back(i);
    }
}

void appendGrid(const std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>& grid,
                const glm::ivec3& c0, const glm::ivec3& c1, WorldCache& c,
                std::vector<std::uint32_t>& out)
{
    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z) {
        auto it = grid.find(cellKey(x, y, z));
        if (it == grid.end())
            continue;
        for (std::uint32_t idx : it->second) {
            if (c.visitStamp[idx] == c.stamp)
                continue;
            c.visitStamp[idx] = c.stamp;
            out.push_back(idx);
        }
    }
}

std::uint64_t hashTris(const GameCollisionTriangleV1* tris, std::uint32_t count)
{
    std::uint64_t h = 14695981039346656037ull;
    auto mix = [&](float f) {
        std::uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        h ^= bits;
        h *= 1099511628211ull;
    };
    for (std::uint32_t i = 0; i < count; ++i) {
        for (int k = 0; k < 3; ++k) {
            mix(tris[i].a[k]);
            mix(tris[i].b[k]);
            mix(tris[i].c[k]);
        }
    }
    return h;
}

} // namespace

WorldCache& worldCache()
{
    static WorldCache cache;
    return cache;
}

void installWorld(const WorldTri* triangles, std::uint32_t count)
{
    WorldCache& c = worldCache();
    c.tris.assign(triangles, triangles + count);
    c.total = count;
    c.ready = count > 0;
    c.sampleHash = 0;
    rebuildIndex(c);
}

const WorldTri& triangle(std::uint32_t index)
{
    return worldCache().tris[index];
}

bool ensureWorld(void* host)
{
    WorldCache& c = worldCache();
    if (!host)
        return !c.tris.empty();
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx->resolveCapability)
        return !c.tris.empty();
    auto fn = reinterpret_cast<GameWorldCollisionFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_WORLD_COLLISION));
    if (!fn)
        return !c.tris.empty();

    // Cheap probe: total only (no triangle copy).
    GameWorldCollisionPageV1 probe{};
    probe.offset = 0;
    probe.maxTriangles = 0;
    probe.out = nullptr;
    fn(ctx->host, &probe);
    const std::uint32_t total = probe.total;
    if (total == 0)
        return false;

    if (c.ready && c.total == total) {
        // Same count: sample the first and last triangles to detect a same-size
        // map edit without re-reading the whole mesh.
        GameCollisionTriangleV1 sample[16];
        std::uint64_t h = 0;
        GameWorldCollisionPageV1 first{};
        first.offset = 0;
        first.maxTriangles = 16;
        first.out = sample;
        fn(ctx->host, &first);
        h ^= hashTris(sample, first.count) * 1099511628211ull;
        GameWorldCollisionPageV1 last{};
        last.offset = total > 16 ? total - 16 : 0;
        last.maxTriangles = 16;
        last.out = sample;
        fn(ctx->host, &last);
        h ^= hashTris(sample, last.count) * 31ull;
        if (h == c.sampleHash)
            return true;
    }

    // Full rebuild: page in every triangle once, then index.
    std::vector<GameCollisionTriangleV1> buf(4096);
    c.tris.clear();
    c.tris.reserve(total);
    for (std::uint32_t off = 0; off < total;) {
        GameWorldCollisionPageV1 page{};
        page.offset = off;
        page.maxTriangles = (std::uint32_t)buf.size();
        page.out = buf.data();
        fn(ctx->host, &page);
        if (page.count == 0)
            break;
        for (std::uint32_t i = 0; i < page.count; ++i) {
            c.tris.push_back(WorldTri{
                glm::vec3(buf[i].a[0], buf[i].a[1], buf[i].a[2]),
                glm::vec3(buf[i].b[0], buf[i].b[1], buf[i].b[2]),
                glm::vec3(buf[i].c[0], buf[i].c[1], buf[i].c[2])});
        }
        off += page.count;
    }
    c.total = total;

    GameCollisionTriangleV1 sample[16];
    std::uint64_t h = 0;
    GameWorldCollisionPageV1 first{};
    first.offset = 0;
    first.maxTriangles = 16;
    first.out = sample;
    fn(ctx->host, &first);
    h ^= hashTris(sample, first.count) * 1099511628211ull;
    GameWorldCollisionPageV1 last{};
    last.offset = total > 16 ? total - 16 : 0;
    last.maxTriangles = 16;
    last.out = sample;
    fn(ctx->host, &last);
    h ^= hashTris(sample, last.count) * 31ull;
    c.sampleHash = h;

    // A partial page-in (fewer triangles than the probe reported) means the
    // world changed mid-read. Do not publish a short index as ready; the caller
    // declines and retries next tick rather than colliding against missing
    // geometry.
    if ((std::uint32_t)c.tris.size() != total) {
        c.tris.clear();
        c.total = 0;
        c.sampleHash = 0;
        c.ready = false;
        return false;
    }

    rebuildIndex(c);
    c.ready = !c.tris.empty();
    return c.ready;
}

void gatherCandidates(const glm::vec3& min, const glm::vec3& max,
                      std::vector<std::uint32_t>& out)
{
    out.clear();
    WorldCache& c = worldCache();
    if (c.tris.empty())
        return;
    const glm::vec3 clampedMin = glm::max(glm::min(min, glm::vec3(5000.0f)),
                                          glm::vec3(-5000.0f));
    const glm::vec3 clampedMax = glm::max(glm::min(max, glm::vec3(5000.0f)),
                                          glm::vec3(-5000.0f));
    if (!std::isfinite(clampedMin.x) || !std::isfinite(clampedMax.x))
        return;
    if (c.visitStamp.size() != c.tris.size())
        c.visitStamp.assign(c.tris.size(), 0u);
    ++c.stamp;
    if (c.stamp == 0) {
        std::fill(c.visitStamp.begin(), c.visitStamp.end(), 0u);
        c.stamp = 1;
    }
    glm::ivec3 f0, f1;
    cellRange(clampedMin, clampedMax, kFineCell, f0, f1);
    appendGrid(c.fine, f0, f1, c, out);
    glm::ivec3 c0, c1;
    cellRange(clampedMin, clampedMax, kCoarseCell, c0, c1);
    appendGrid(c.coarse, c0, c1, c, out);
    for (std::uint32_t idx : c.always) {
        if (c.visitStamp[idx] == c.stamp)
            continue;
        c.visitStamp[idx] = c.stamp;
        out.push_back(idx);
    }
}

glm::vec3 closestPointOnTri(const glm::vec3& p, const WorldTri& t)
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

int gatherSphereHits(const glm::vec3& center, float radius, float tolerance,
                     const std::vector<std::uint32_t>& candidates,
                     SphereHit* out, int maxOut)
{
    WorldCache& c = worldCache();
    int n = 0;
    for (std::uint32_t idx : candidates) {
        if (n >= maxOut)
            break;
        if (idx >= c.tris.size())
            continue;
        const glm::vec3 cp = closestPointOnTri(center, c.tris[idx]);
        const glm::vec3 d = center - cp;
        const float dist = glm::length(d);
        if (dist < 1e-6f)
            continue;
        const bool penetrating = dist < radius;
        const bool touching = !penetrating && dist <= radius + tolerance;
        if (!penetrating && !touching)
            continue;
        out[n].triangle = (std::int32_t)idx;
        out[n].point = cp;
        out[n].normal = d / dist;
        out[n].penetration = penetrating ? (radius - dist) : 0.0f;
        out[n].touching = penetrating ? 0u : 1u;
        ++n;
    }
    return n;
}

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
