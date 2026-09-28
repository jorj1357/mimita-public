// 2026-09-28
/* purpose
* Implement the table-free isosurface extractor for destructible geometry.
* One vertex per active cell, one quad per sign-changing grid edge; the quad is
* canonically owned by the chunk containing the edge's minimum corner cell so
* chunk meshes are watertight, non-overlapping, and deterministic.
* Emits a flat-shaded triangle soup for rendering and collision.
* Does NOT own cut history, chunk scheduling, or rendering itself.
*/

#include "impact/isosurface.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace MimitaImpact {
namespace {

// Corner offsets: bit0 = x, bit1 = y, bit2 = z.
const int kCorner[8][3] = {
    {0,0,0},{1,0,0},{1,1,0},{0,1,0},
    {0,0,1},{1,0,1},{1,1,1},{0,1,1}
};

// 12 cube edges as corner-index pairs.
const int kEdge[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

struct Grid
{
    glm::vec3 origin{0.0f};
    float cell = 1.0f;
};

inline glm::vec3 samplePos(const Grid& grid, int ix, int iy, int iz)
{
    return grid.origin +
        glm::vec3((float)ix, (float)iy, (float)iz) * grid.cell;
}

inline float sampleValue(const DestructibleGeometry& geometry,
                         const Grid& grid, int ix, int iy, int iz)
{
    return destructibleCrateDistance(geometry, samplePos(grid, ix, iy, iz));
}

inline int signBit(float v)
{
    return v < 0.0f ? 1 : 0;   // solid (negative) = inside
}

glm::vec3 sdfGradient(const DestructibleGeometry& geometry, const glm::vec3& p, float h)
{
    const float dx = destructibleCrateDistance(geometry, p + glm::vec3(h, 0, 0)) -
                     destructibleCrateDistance(geometry, p - glm::vec3(h, 0, 0));
    const float dy = destructibleCrateDistance(geometry, p + glm::vec3(0, h, 0)) -
                     destructibleCrateDistance(geometry, p - glm::vec3(0, h, 0));
    const float dz = destructibleCrateDistance(geometry, p + glm::vec3(0, 0, h)) -
                     destructibleCrateDistance(geometry, p - glm::vec3(0, 0, h));
    const glm::vec3 g(dx, dy, dz);
    const float len = glm::length(g);
    return len > 1e-6f ? g / len : glm::vec3(0.0f, 0.0f, 1.0f);
}

} // anonymous namespace

GeneratedDestructionMesh meshDestructibleChunk(
    const DestructibleGeometry& geometry,
    glm::ivec3 baseIndex,
    glm::ivec3 cellsPerAxis,
    float cellSize,
    const glm::vec3& gridOrigin,
    size_t triangleBudget)
{
    GeneratedDestructionMesh mesh;
    if (cellsPerAxis.x <= 0 || cellsPerAxis.y <= 0 || cellsPerAxis.z <= 0 || cellSize <= 0.0f)
        return mesh;

    Grid grid;
    grid.origin = gridOrigin;
    grid.cell = cellSize;

    const int K = std::max(1, geometry.chunkCountPerAxis);
    const int C = std::max(1, geometry.cellsPerChunkAxis);
    const int totalCells = K * C + 2;

    // Owned cells [beg, end]; one halo cell on the positive side is generated so
    // quads whose minimum cell lies inside this chunk can be emitted.
    const glm::ivec3 beg = baseIndex;
    const glm::ivec3 end =
        glm::min(baseIndex + cellsPerAxis - glm::ivec3(1), glm::ivec3(totalCells - 1));
    const glm::ivec3 genEnd = glm::min(end + 1, glm::ivec3(totalCells));
    const glm::ivec3 genBeg = beg;

    const glm::ivec3 span = genEnd - genBeg + 1;
    const int sx = span.x, sy = span.y, sz = span.z;
    std::vector<glm::vec3> cellPos((size_t)sx * sy * sz, glm::vec3(0.0f));
    std::vector<uint8_t> cellActive((size_t)sx * sy * sz, 0);

    auto cellIndex = [&](int cx, int cy, int cz) -> int
    {
        if (cx < genBeg.x || cx > genEnd.x ||
            cy < genBeg.y || cy > genEnd.y ||
            cz < genBeg.z || cz > genEnd.z)
            return -1;
        return ((cz - genBeg.z) * sy + (cy - genBeg.y)) * sx + (cx - genBeg.x);
    };

    // ── Cell vertices (averaged edge crossings) ─────────────────────
    for (int cz = genBeg.z; cz <= genEnd.z; ++cz)
    for (int cy = genBeg.y; cy <= genEnd.y; ++cy)
    for (int cx = genBeg.x; cx <= genEnd.x; ++cx)
    {
        float v[8];
        int mask = 0;
        for (int c = 0; c < 8; ++c)
        {
            v[c] = sampleValue(geometry, grid,
                               cx + kCorner[c][0], cy + kCorner[c][1], cz + kCorner[c][2]);
            mask |= signBit(v[c]) << c;
        }
        if (mask == 0 || mask == 255)
            continue;

        glm::vec3 sum(0.0f);
        int crossings = 0;
        for (int e = 0; e < 12; ++e)
        {
            const int a = kEdge[e][0];
            const int b = kEdge[e][1];
            if (signBit(v[a]) == signBit(v[b]))
                continue;
            const float denom = v[b] - v[a];
            const float t = std::fabs(denom) > 1e-9f ? (-v[a] / denom) : 0.5f;
            const glm::vec3 pa = samplePos(grid, cx + kCorner[a][0], cy + kCorner[a][1], cz + kCorner[a][2]);
            const glm::vec3 pb = samplePos(grid, cx + kCorner[b][0], cy + kCorner[b][1], cz + kCorner[b][2]);
            sum += pa + (pb - pa) * glm::clamp(t, 0.0f, 1.0f);
            ++crossings;
        }
        if (crossings == 0)
            continue;
        const int ci = cellIndex(cx, cy, cz);
        if (ci < 0)
            continue;
        cellPos[(size_t)ci] = sum / (float)crossings;
        cellActive[(size_t)ci] = 1;
    }

    auto vertexAt = [&](int cx, int cy, int cz, glm::vec3& out) -> bool
    {
        const int ci = cellIndex(cx, cy, cz);
        if (ci < 0 || !cellActive[(size_t)ci])
            return false;
        out = cellPos[(size_t)ci];
        return true;
    };

    const glm::vec3 localOrigin = geometry.localOrigin;
    const glm::vec3 localExtent = geometry.localExtent;
    const float invX = localExtent.x > 1e-6f ? 1.0f / localExtent.x : 0.0f;
    const float invY = localExtent.y > 1e-6f ? 1.0f / localExtent.y : 0.0f;

    auto appendRender = [&](const glm::vec3& p, const glm::vec3& n)
    {
        Vertex v;
        v.pos = p;
        v.normal = n;
        v.uv = glm::vec2((p.x - localOrigin.x) * invX,
                         (p.y - localOrigin.y) * invY);
        mesh.renderVertices.push_back(v);
    };

    auto emitTriangle = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        if (mesh.collisionTriangles.size() >= triangleBudget)
        {
            mesh.budgetExceeded = true;
            return;
        }
        glm::vec3 n = glm::cross(b - a, c - a);
        const float len = glm::length(n);
        if (len < 1e-9f)
            return;
        n /= len;
        const glm::vec3 center = (a + b + c) / 3.0f;
        const glm::vec3 outward = sdfGradient(geometry, center, cellSize * 0.5f);
        if (glm::dot(n, outward) < 0.0f)
            n = -n;
        CollisionTriangle tri;
        tri.a = a; tri.b = b; tri.c = c; tri.normal = n;
        mesh.collisionTriangles.push_back(tri);
        appendRender(a, n);
        appendRender(b, n);
        appendRender(c, n);
    };

    auto emitQuad = [&](int cx0, int cy0, int cz0,
                        int cx1, int cy1, int cz1,
                        int cx2, int cy2, int cz2,
                        int cx3, int cy3, int cz3)
    {
        glm::vec3 p0, p1, p2, p3;
        if (!vertexAt(cx0, cy0, cz0, p0) || !vertexAt(cx1, cy1, cz1, p1) ||
            !vertexAt(cx2, cy2, cz2, p2) || !vertexAt(cx3, cy3, cz3, p3))
            return;
        emitTriangle(p0, p1, p2);
        emitTriangle(p0, p2, p3);
    };

    // X-edges: samples (i,j,k)-(i+1,j,k); min cell (i, j-1, k-1)
    for (int i = beg.x; i <= end.x && !mesh.budgetExceeded; ++i)
    for (int j = 1; j <= totalCells - 1; ++j)
    for (int k = 1; k <= totalCells - 1; ++k)
    {
        if (j - 1 < beg.y || j - 1 > end.y) continue;
        if (k - 1 < beg.z || k - 1 > end.z) continue;
        if (signBit(sampleValue(geometry, grid, i, j, k)) ==
            signBit(sampleValue(geometry, grid, i + 1, j, k)))
            continue;
        emitQuad(i, j - 1, k - 1, i, j, k - 1, i, j, k, i, j - 1, k);
    }

    // Y-edges: samples (i,j,k)-(i,j+1,k); min cell (i-1, j, k-1)
    for (int i = 1; i <= totalCells - 1 && !mesh.budgetExceeded; ++i)
    for (int j = beg.y; j <= end.y; ++j)
    for (int k = 1; k <= totalCells - 1; ++k)
    {
        if (i - 1 < beg.x || i - 1 > end.x) continue;
        if (k - 1 < beg.z || k - 1 > end.z) continue;
        if (signBit(sampleValue(geometry, grid, i, j, k)) ==
            signBit(sampleValue(geometry, grid, i, j + 1, k)))
            continue;
        emitQuad(i - 1, j, k - 1, i, j, k - 1, i, j, k, i - 1, j, k);
    }

    // Z-edges: samples (i,j,k)-(i,j,k+1); min cell (i-1, j-1, k)
    for (int i = 1; i <= totalCells - 1 && !mesh.budgetExceeded; ++i)
    for (int j = 1; j <= totalCells - 1; ++j)
    for (int k = beg.z; k <= end.z; ++k)
    {
        if (i - 1 < beg.x || i - 1 > end.x) continue;
        if (j - 1 < beg.y || j - 1 > end.y) continue;
        if (signBit(sampleValue(geometry, grid, i, j, k)) ==
            signBit(sampleValue(geometry, grid, i, j, k + 1)))
            continue;
        emitQuad(i - 1, j - 1, k, i, j - 1, k, i, j, k, i - 1, j, k);
    }

    return mesh;
}

} // namespace MimitaImpact
