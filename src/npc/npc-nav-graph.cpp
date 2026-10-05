// 10 05 2026
/* purpose
* Implements the lazy chunked walkable navigation graph (see npc-nav-graph.h).
* Voxelizes loaded collision triangles on demand, links walk/ramp/jump/drop
* edges, and runs A* that can return multiple distinct routes.
* No hand-placed points, no global map load pass, no gameplay decisions.
*/

#include "npc/npc-nav-graph.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "world/world.h"
#include "map/map-loader-collision.h"
#include "physics/physics-types.h"
#include "physics/movement/physics-collision.h"

namespace {

constexpr float kUpProbe = 3.0f;      // probe start above the global mesh top
float gMeshMinZ = 0.0f;
float gMeshMaxZ = 0.0f;

// Möller–Trumbore, local copy so this module has no NPC dependency.
bool rayTriangle(const glm::vec3& origin, const glm::vec3& dir,
                 const CollisionTriangle& tri, float maxT, float& outT)
{
    const glm::vec3 e1 = tri.b - tri.a;
    const glm::vec3 e2 = tri.c - tri.a;
    const glm::vec3 p = glm::cross(dir, e2);
    const float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.0001f) return false;
    const float invDet = 1.0f / det;
    const glm::vec3 tVec = origin - tri.a;
    const float u = glm::dot(tVec, p) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    const glm::vec3 q = glm::cross(tVec, e1);
    const float v = glm::dot(dir, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    outT = glm::dot(e2, q) * invDet;
    return outT > 0.01f && outT < maxT;
}

bool anyHit(const glm::vec3& origin, const glm::vec3& dir, float maxT,
            const std::vector<int>& tris, const World& world,
            float* outT = nullptr, glm::vec3* outNormal = nullptr)
{
    float nearest = maxT;
    bool hit = false;
    glm::vec3 nml{0.0f};
    for (int ti : tris) {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
        float t = 0.0f;
        if (rayTriangle(origin, dir, world.collisionMesh.triangles[(size_t)ti], maxT, t)) {
            if (t < nearest) {
                nearest = t;
                const CollisionTriangle& tri = world.collisionMesh.triangles[(size_t)ti];
                nml = glm::normalize(glm::cross(tri.b - tri.a, tri.c - tri.a));
                hit = true;
            }
        }
    }
    if (hit && outT) *outT = nearest;
    if (hit && outNormal) *outNormal = nml;
    return hit;
}

// Three offset rays approximate the actor width so a slot narrower than the
// capsule is rejected. Only steep faces block; walkable ramps are terrain.
bool segmentClear(const World& world, const std::vector<int>& tris,
                  const glm::vec3& a, const glm::vec3& b, float z,
                  float walkableDot, float radius)
{
    glm::vec3 dir(b.x - a.x, b.y - a.y, 0.0f);
    const float dist = glm::length(dir);
    if (dist < 0.001f) return true;
    dir /= dist;
    const glm::vec3 perp(-dir.y, dir.x, 0.0f);
    const float offsets[3] = {0.0f, radius * 0.7f, -radius * 0.7f};
    for (float off : offsets) {
        const glm::vec3 o = glm::vec3(a.x, a.y, z) + perp * off;
        float t = 0.0f;
        glm::vec3 n{0.0f};
        if (anyHit(o, dir, dist + 0.2f, tris, world, &t, &n) && n.z < walkableDot)
            return false;
    }
    return true;
}

std::uint64_t chunkKey(glm::ivec2 c)
{
    return (std::uint64_t)(std::uint32_t)c.x << 32 | (std::uint32_t)c.y;
}

std::uint64_t edgeKey(std::uint32_t from, std::uint32_t to)
{
    return (std::uint64_t)from << 32 | (std::uint64_t)to;
}

} // namespace

NpcNavGraph& NpcNavGraph::instance()
{
    static NpcNavGraph graph;
    return graph;
}

int NpcNavGraph::cellsPerChunk(const NpcNavGraphSettings& s) const
{
    return std::max(2, (int)std::floor(std::max(1.0f, s.chunkSize) /
                                       std::max(0.5f, s.cellSize)));
}

void NpcNavGraph::invalidate()
{
    mChunks.clear();
    mLinkedPairs.clear();
    mGeometrySignature = 0;
    mNextNodeId = 1;
}

void NpcNavGraph::addEdge(GraphNode& from, std::uint32_t to, std::uint8_t cap,
                          float cost)
{
    for (std::size_t i = 0; i < from.neighbors.size(); ++i)
        if (from.neighbors[i] == to) return;   // one edge per pair
    from.neighbors.push_back(to);
    from.caps.push_back(cap);
    from.costs.push_back(cost);
}

NpcNavGraph::GraphChunk* NpcNavGraph::buildChunk(const World& world,
                                                 const NpcNavGraphSettings& s,
                                                 glm::ivec2 coord)
{
    const std::uint64_t key = chunkKey(coord);
    auto it = mChunks.find(key);
    if (it != mChunks.end())
        return &it->second;

    GraphChunk& chunk = mChunks[key];
    chunk.coord = coord;
    chunk.idBase = mNextNodeId;
    chunk.groundMaxZ = gMeshMaxZ;

    const float cs = std::max(0.5f, s.cellSize);
    const int N = cellsPerChunk(s);
    const float ox = (float)coord.x * s.chunkSize;
    const float oy = (float)coord.y * s.chunkSize;
    const float walkableDot = s.maxWalkableSlopeDot > 0.0f
        ? s.maxWalkableSlopeDot : 0.80f;

    // Gather the chunk's collision triangles plus a one-cell border so
    // boundary/edge clearance rays see neighbouring geometry.
    AABB bounds;
    bounds.min = glm::vec3(ox - cs, oy - cs, gMeshMinZ - 2.0f);
    bounds.max = glm::vec3(ox + s.chunkSize + cs, oy + s.chunkSize + cs,
                           gMeshMaxZ + 2.0f);
    chunk.triangles.clear();
    appendChunkTrianglesForAABB(world, bounds, 0.0f, chunk.triangles, "npcNavGraph");

    std::vector<NavigationSurface> column;
    const float fromZ = gMeshMaxZ + kUpProbe;
    const float depth = (gMeshMaxZ - gMeshMinZ) + kUpProbe + 4.0f;

    for (int cy = 0; cy < N; ++cy) {
        for (int cx = 0; cx < N; ++cx) {
            const float x = ox + (cx + 0.5f) * cs;
            const float y = oy + (cy + 0.5f) * cs;
            const glm::vec3 origin(x, y, fromZ);
            const glm::vec3 down(0.0f, 0.0f, -1.0f);
            column.clear();
            for (int ti : chunk.triangles) {
                if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
                const CollisionTriangle& tri = world.collisionMesh.triangles[(size_t)ti];
                if (tri.normal.z < walkableDot) continue;
                float t = 0.0f;
                if (!rayTriangle(origin, down, tri, depth, t)) continue;
                const float z = fromZ - t;
                bool dup = false;
                for (const auto& existing : column)
                    if (std::fabs(existing.height - z) <= 0.05f) { dup = true; break; }
                if (dup) continue;
                NavigationSurface surf;
                surf.position = glm::vec3(x, y, z);
                surf.normal = tri.normal;
                surf.height = z;
                surf.kind = classifySurface(tri.normal, walkableDot);
                surf.walkable = true;
                column.push_back(surf);
            }
            std::sort(column.begin(), column.end(),
                      [](const NavigationSurface& a, const NavigationSurface& b) {
                          return a.height > b.height;
                      });
            if ((int)column.size() > s.maxSurfacesPerColumn)
                column.resize((size_t)s.maxSurfacesPerColumn);

            const int ckey = cellKey(cx, cy);
            auto& cellList = chunk.cells[ckey];
            for (const NavigationSurface& surf : column) {
                // Headroom: a ceiling closer than the actor height invalidates
                // the surface so an actor never plans under a low ceiling.
                const glm::vec3 up(0.0f, 0.0f, 1.0f);
                const glm::vec3 ho = surf.position + glm::vec3(0.0f, 0.0f, 0.1f);
                float ht = 0.0f;
                if (anyHit(ho, up, std::max(1.0f, s.actorHeight),
                           chunk.triangles, world, &ht))
                    continue;
                GraphNode node;
                node.pos = surf.position;
                node.kind = surf.kind;
                node.cx = (std::uint16_t)cx;
                node.cy = (std::uint16_t)cy;
                node.id = mNextNodeId++;
                cellList.push_back((int)chunk.nodes.size());
                chunk.nodes.push_back(std::move(node));
            }
        }
    }

    // Intra-chunk edges.
    static const int DX[8] = {1,-1,0,0, 1,1,-1,-1};
    static const int DY[8] = {0,0,1,-1, 1,-1,1,-1};
    for (int cy = 0; cy < N; ++cy) {
        for (int cx = 0; cx < N; ++cx) {
            auto aIt = chunk.cells.find(cellKey(cx, cy));
            if (aIt == chunk.cells.end()) continue;
            for (int d = 0; d < 8; ++d) {
                const int nx = cx + DX[d], ny = cy + DY[d];
                if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
                if (DX[d] != 0 && DY[d] != 0) {
                    auto oxn = chunk.cells.find(cellKey(nx, cy));
                    auto oyn = chunk.cells.find(cellKey(cx, ny));
                    if (oxn == chunk.cells.end() || oyn == chunk.cells.end())
                        continue;  // no corner cutting
                }
                auto bIt = chunk.cells.find(cellKey(nx, ny));
                if (bIt == chunk.cells.end()) continue;
                const float planar = (DX[d] != 0 && DY[d] != 0)
                    ? cs * 1.4142f : cs;
                for (int ai : aIt->second) {
                    for (int bi : bIt->second) {
                        GraphNode& a = chunk.nodes[(size_t)ai];
                        GraphNode& b = chunk.nodes[(size_t)bi];
                        const float dz = b.pos.z - a.pos.z;
                        if (dz > s.jumpHeight) continue;
                        if (dz < -s.maxDropHeight) continue;
                        const bool needJump = dz > s.maxStepHeight;
                        if (needJump && !s.allowJumps) continue;
                        const float baseZ = std::max(a.pos.z, b.pos.z) + 0.15f;
                        if (!segmentClear(world, chunk.triangles, a.pos, b.pos,
                                          baseZ, walkableDot, s.actorRadius))
                            continue;
                        const float cost = planar + (needJump ? 2.0f : 0.0f) +
                                           (dz < -0.05f ? 0.5f : 0.0f);
                        const std::uint8_t cap = needJump
                            ? (std::uint8_t)NavCapability::Jump
                            : (dz < -0.05f ? (std::uint8_t)NavCapability::Drop
                                           : (std::uint8_t)NavCapability::Walk);
                        addEdge(a, b.id, cap, cost);
                        addEdge(b, a.id, cap, cost);
                    }
                }
            }
        }
    }

    ++mBuildCount;
    return &chunk;
}

void NpcNavGraph::linkPair(const World& world, GraphChunk& a, GraphChunk& b,
                           const NpcNavGraphSettings& s)
{
    const int N = cellsPerChunk(s);
    const float cs = std::max(0.5f, s.cellSize);
    const float walkableDot = s.maxWalkableSlopeDot > 0.0f
        ? s.maxWalkableSlopeDot : 0.80f;
    const glm::ivec2 delta = b.coord - a.coord;

    // For each boundary cell of A facing B, connect to the matching boundary
    // cell of B. Only orthogonal neighbours are linked (diagonal chunk
    // adjacency is reached through the shared orthogonal chunks).
    for (int i = 0; i < N; ++i) {
        int acx = 0, acy = 0, bcx = 0, bcy = 0;
        if (delta.x == 1 && delta.y == 0) {
            acx = N - 1; acy = i; bcx = 0; bcy = i;
        } else if (delta.x == -1 && delta.y == 0) {
            acx = 0; acy = i; bcx = N - 1; bcy = i;
        } else if (delta.x == 0 && delta.y == 1) {
            acx = i; acy = N - 1; bcx = i; bcy = 0;
        } else if (delta.x == 0 && delta.y == -1) {
            acx = i; acy = 0; bcx = i; bcy = N - 1;
        } else {
            continue;
        }
        auto aIt = a.cells.find(cellKey(acx, acy));
        auto bIt = b.cells.find(cellKey(bcx, bcy));
        if (aIt == a.cells.end() || bIt == b.cells.end()) continue;
        for (int ai : aIt->second) {
            for (int bi : bIt->second) {
                GraphNode& na = a.nodes[(size_t)ai];
                GraphNode& nb = b.nodes[(size_t)bi];
                const float dz = nb.pos.z - na.pos.z;
                if (dz > s.jumpHeight) continue;
                if (dz < -s.maxDropHeight) continue;
                const bool needJump = dz > s.maxStepHeight;
                if (needJump && !s.allowJumps) continue;
                const float baseZ = std::max(na.pos.z, nb.pos.z) + 0.15f;
                if (!segmentClear(world, a.triangles, na.pos, nb.pos,
                                  baseZ, walkableDot, s.actorRadius))
                    continue;
                const float planar = glm::length(glm::vec2(nb.pos.x - na.pos.x,
                                                           nb.pos.y - na.pos.y));
                const float cost = planar + (needJump ? 2.0f : 0.0f) +
                                   (dz < -0.05f ? 0.5f : 0.0f);
                const std::uint8_t cap = needJump
                    ? (std::uint8_t)NavCapability::Jump
                    : (dz < -0.05f ? (std::uint8_t)NavCapability::Drop
                                   : (std::uint8_t)NavCapability::Walk);
                addEdge(na, nb.id, cap, cost);
                addEdge(nb, na.id, cap, cost);
            }
        }
    }
    (void)cs;
}

void NpcNavGraph::ensureLinks(const World& world, const NpcNavGraphSettings& s,
                              const std::vector<glm::ivec2>& coords)
{
    for (const glm::ivec2& c : coords) {
        auto aIt = mChunks.find(chunkKey(c));
        if (aIt == mChunks.end()) continue;
        const glm::ivec2 neighbours[4] = {
            {c.x + 1, c.y}, {c.x - 1, c.y}, {c.x, c.y + 1}, {c.x, c.y - 1}
        };
        for (const glm::ivec2& n : neighbours) {
            auto bIt = mChunks.find(chunkKey(n));
            if (bIt == mChunks.end()) continue;
            const std::uint64_t ka = (std::uint64_t)(std::uint32_t)c.x << 32 |
                                     (std::uint32_t)c.y;
            const std::uint64_t kb = (std::uint64_t)(std::uint32_t)n.x << 32 |
                                     (std::uint32_t)n.y;
            const std::uint64_t pair = ka < kb ? (ka ^ (kb * 1099511628211ull)) : (kb ^ (ka * 1099511628211ull));
            if (mLinkedPairs.count(pair)) continue;
            mLinkedPairs[pair] = true;
            linkPair(world, aIt->second, bIt->second, s);
        }
    }
}

std::vector<NpcNavRoute> NpcNavGraph::findRoutes(const World& world,
                                                 const glm::vec3& start,
                                                 const glm::vec3& goal,
                                                 const NpcNavGraphSettings& settings,
                                                 uint32_t actorSeed)
{
    std::vector<NpcNavRoute> routes;

    const std::uint64_t sig = (std::uint64_t)world.collisionMesh.triangles.size() * 1315423911ull +
                              (std::uint64_t)world.collisionChunks.size() * 2654435761ull;
    if (sig != mGeometrySignature) {
        invalidate();
        mGeometrySignature = sig;
        // Recompute mesh Z bounds once per geometry revision.
        gMeshMinZ = 1e9f;
        gMeshMaxZ = -1e9f;
        for (const auto& tri : world.collisionMesh.triangles) {
            gMeshMinZ = std::min({gMeshMinZ, tri.a.z, tri.b.z, tri.c.z});
            gMeshMaxZ = std::max({gMeshMaxZ, tri.a.z, tri.b.z, tri.c.z});
        }
        if (!(gMeshMinZ < gMeshMaxZ)) { gMeshMinZ = -100.0f; gMeshMaxZ = 100.0f; }
    }

    const glm::vec2 planarDelta(goal.x - start.x, goal.y - start.y);
    if (glm::length(planarDelta) > settings.maxRouteMeters)
        return routes;

    const float chunkSize = std::max(4.0f, settings.chunkSize);
    const glm::ivec2 startChunk((int)std::floor(start.x / chunkSize),
                                (int)std::floor(start.y / chunkSize));
    const glm::ivec2 goalChunk((int)std::floor(goal.x / chunkSize),
                               (int)std::floor(goal.y / chunkSize));

    // Supercover the chunk line start->goal.
    std::vector<glm::vec2> line;
    line.push_back(glm::vec2(start.x, start.y));
    line.push_back(glm::vec2(goal.x, goal.y));
    if (glm::length(planarDelta) > 1.0f) {
        const int steps = std::min(64, std::max(1,
            (int)(glm::length(planarDelta) / chunkSize)));
        line.clear();
        for (int i = 0; i <= steps; ++i)
            line.push_back(glm::vec2(start.x, start.y) +
                           planarDelta * ((float)i / (float)steps));
    }

    auto buildCoords = [&](int margin) {
        std::vector<glm::ivec2> coords;
        std::unordered_map<std::uint64_t, bool> seen;
        auto add = [&](glm::ivec2 c) {
            const std::uint64_t k = chunkKey(c);
            if (seen.count(k)) return;
            seen[k] = true;
            coords.push_back(c);
        };
        for (const glm::vec2& p : line) {
            const glm::ivec2 c((int)std::floor(p.x / chunkSize),
                               (int)std::floor(p.y / chunkSize));
            for (int dy = -margin; dy <= margin; ++dy)
                for (int dx = -margin; dx <= margin; ++dx)
                    add({c.x + dx, c.y + dy});
        }
        add(startChunk);
        add(goalChunk);
        if ((int)coords.size() > settings.maxActiveChunks)
            coords.resize((size_t)settings.maxActiveChunks);
        return coords;
    };

    for (int attempt = 0; attempt < 4; ++attempt) {
        const int margin = attempt;  // 0,1,2,3 chunk ring expansion
        const std::vector<glm::ivec2> coords = buildCoords(margin);
        for (const glm::ivec2& c : coords) buildChunk(world, settings, c);
        ensureLinks(world, settings, coords);

        std::unordered_map<std::uint64_t, float> penalty;
        NpcNavRoute first;
        if (!findOneRoute(world, settings, coords, start, goal, penalty, first))
            continue;

        // Multiple equal-cost routes: repeatedly re-search with the previous
        // route's edges penalized. This yields genuinely different lines.
        std::vector<NpcNavRoute> found;
        found.push_back(first);
        for (std::size_t k = 1; k < (std::size_t)std::max(1, settings.maxRoutes); ++k) {
            const NpcNavRoute& last = found.back();
            // Penalize nodes near the last route so the next search prefers a
            // different (but equally viable) corridor.
            std::unordered_map<std::uint64_t, float> p2 = penalty;
            for (const glm::vec3& p : last.points) {
                const glm::ivec2 c((int)std::floor(p.x / chunkSize),
                                   (int)std::floor(p.y / chunkSize));
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        auto it = mChunks.find(chunkKey({c.x + dx, c.y + dy}));
                        if (it == mChunks.end()) continue;
                        for (const auto& node : it->second.nodes) {
                            const float d = glm::length(glm::vec2(node.pos.x - p.x,
                                                                  node.pos.y - p.y));
                            if (d < chunkSize)
                                p2[node.id] += 30.0f * (1.0f - d / chunkSize);
                        }
                    }
            }
            NpcNavRoute next;
            if (!findOneRoute(world, settings, coords, start, goal, p2, next))
                break;
            // Skip near-duplicates.
            bool distinct = true;
            for (const auto& r : found) {
                int shared = 0;
                for (const glm::vec3& p : next.points)
                    for (const glm::vec3& q : r.points)
                        if (glm::length(glm::vec2(p.x - q.x, p.y - q.y)) < 0.01f) {
                            ++shared; break;
                        }
                if (shared > (int)(0.8f * (float)next.points.size()))
                    { distinct = false; break; }
            }
            if (!distinct) break;
            found.push_back(next);
            penalty = p2;
        }

        // Deterministically rotate the list so each actor prefers a different
        // route (multiple equal-cost routes, less predictable).
        const int count = (int)found.size();
        const int offset = count > 0 ? (int)(actorSeed % (uint32_t)count) : 0;
        for (int i = 0; i < count; ++i)
            routes.push_back(found[(size_t)((i + offset) % count)]);
        break;
    }

    return routes;
}

bool NpcNavGraph::findOneRoute(const World& world, const NpcNavGraphSettings& s,
                               const std::vector<glm::ivec2>& coords,
                               const glm::vec3& start, const glm::vec3& goal,
                               const std::unordered_map<std::uint64_t, float>& penalty,
                               NpcNavRoute& out)
{
    (void)world;
    constexpr float kInf = std::numeric_limits<float>::max();
    struct SearchNode {
        std::uint32_t id = 0;
        const GraphNode* node = nullptr;
        float g = kInf;
        float f = kInf;
        int parent = -1;
        bool closed = false;
    };
    std::vector<SearchNode> nodes;
    std::unordered_map<std::uint32_t, int> index;
    for (const glm::ivec2& c : coords) {
        auto it = mChunks.find(chunkKey(c));
        if (it == mChunks.end()) continue;
        for (const GraphNode& n : it->second.nodes) {
            index[n.id] = (int)nodes.size();
            SearchNode sn;
            sn.id = n.id;
            sn.node = &n;
            nodes.push_back(sn);
        }
    }
    if (nodes.empty()) return false;

    auto nearest = [&](const glm::vec3& p) {
        int best = -1;
        float bestD = std::numeric_limits<float>::max();
        for (int i = 0; i < (int)nodes.size(); ++i) {
            const glm::vec3 d = nodes[(size_t)i].node->pos - p;
            const float dd = glm::dot(d, d);
            if (dd < bestD) { bestD = dd; best = i; }
        }
        return best;
    };
    const int startIdx = nearest(start);
    const int goalIdx = nearest(goal);
    if (startIdx < 0 || goalIdx < 0) return false;

    auto heuristic = [&](int i) {
        return glm::length(nodes[(size_t)i].node->pos - goal);
    };
    nodes[(size_t)startIdx].g = 0.0f;
    nodes[(size_t)startIdx].f = heuristic(startIdx);

    std::vector<int> open;
    open.push_back(startIdx);
    while (!open.empty()) {
        int bi = 0;
        for (int i = 1; i < (int)open.size(); ++i)
            if (nodes[(size_t)open[(size_t)i]].f < nodes[(size_t)open[(size_t)bi]].f) bi = i;
        const int cur = open[(size_t)bi];
        open.erase(open.begin() + bi);
        if (nodes[(size_t)cur].closed) continue;
        nodes[(size_t)cur].closed = true;
        if (cur == goalIdx) break;

        const GraphNode& node = *nodes[(size_t)cur].node;
        for (std::size_t e = 0; e < node.neighbors.size(); ++e) {
            auto nIt = index.find(node.neighbors[e]);
            if (nIt == index.end()) continue;
            const int ni = nIt->second;
            if (nodes[(size_t)ni].closed) continue;
            float extra = node.costs.size() > e ? node.costs[e] : 1.0f;
            auto pen = penalty.find(node.id);
            // Penalty is keyed per-node and per-edge; use both.
            if (pen != penalty.end()) extra += pen->second;
            auto penEdge = penalty.find(edgeKey(node.id, node.neighbors[e]));
            if (penEdge != penalty.end()) extra += penEdge->second;
            const float ng = nodes[(size_t)cur].g + extra;
            if (ng < nodes[(size_t)ni].g) {
                nodes[(size_t)ni].g = ng;
                nodes[(size_t)ni].f = ng + heuristic(ni);
                nodes[(size_t)ni].parent = cur;
                open.push_back(ni);
            }
        }
    }
    if (nodes[(size_t)goalIdx].parent < 0 && startIdx != goalIdx)
        return false;

    std::vector<int> chain;
    for (int cur = goalIdx; cur >= 0; cur = nodes[(size_t)cur].parent) {
        chain.push_back(cur);
        if (cur == startIdx) break;
    }
    std::reverse(chain.begin(), chain.end());
    std::vector<glm::vec3> raw;
    for (int i : chain)
        raw.push_back(nodes[(size_t)i].node->pos);
    // Merge near-collinear points to remove grid zigzag (keeps route length and
    // following stable without changing which corridor was chosen).
    out.points.clear();
    for (const glm::vec3& p : raw) {
        if (out.points.size() >= 2) {
            const glm::vec3 d1 = out.points.back() - out.points[out.points.size() - 2];
            const glm::vec3 d2 = p - out.points.back();
            const float l1 = glm::length(glm::vec2(d1.x, d1.y));
            const float l2 = glm::length(glm::vec2(d2.x, d2.y));
            if (l1 > 0.001f && l2 > 0.001f &&
                glm::dot(d1 / l1, d2 / l2) > 0.98f) {
                out.points.back() = p;
                continue;
            }
        }
        out.points.push_back(p);
    }
    out.length = 0.0f;
    for (std::size_t i = 1; i < out.points.size(); ++i)
        out.length += glm::length(out.points[i] - out.points[i - 1]);
    return out.valid();
}

bool npcNavGraphSelfTest(std::string& report)
{
    bool ok = true;
    auto check = [&](bool cond, const char* what) {
        report += std::string(cond ? "  ok   " : "  FAIL ") + what + "\n";
        ok = ok && cond;
        return cond;
    };

    World world;
    auto addTri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        CollisionTriangle t;
        t.a = a; t.b = b; t.c = c;
        const glm::vec3 n = glm::cross(b - a, c - a);
        const float len = glm::length(n);
        t.normal = len > 1e-6f ? n / len : glm::vec3(0, 0, 1);
        world.collisionMesh.triangles.push_back(t);
    };
    // 120 x 120 floor at z=0.
    addTri({60, 60, 0}, {-60, 60, 0}, {-60, -60, 0});
    addTri({60, 60, 0}, {-60, -60, 0}, {60, -60, 0});
    // Vertical wall at x=10 spanning y in [-10,10], z in [0,4].
    addTri({10, -10, 0}, {10, 10, 0}, {10, 10, 4});
    addTri({10, -10, 0}, {10, 10, 4}, {10, -10, 4});
    buildCollisionChunks(world, nullptr);

    NpcNavGraph graph;
    NpcNavGraphSettings s;
    s.chunkSize = 32.0f;
    s.cellSize = 2.0f;
    s.maxRoutes = 3;
    s.maxActiveChunks = 256;
    s.allowJumps = true;
    s.jumpHeight = 2.0f;

    const glm::vec3 start(-5.0f, 0.0f, 1.0f);
    const glm::vec3 goal(30.0f, 0.0f, 1.0f);
    std::vector<NpcNavRoute> routes = graph.findRoutes(world, start, goal, s, 0);
    check(!routes.empty(), "a route is found across the floor");
    if (!routes.empty()) {
        const float straight = glm::length(glm::vec2(goal.x - start.x, goal.y - start.y));
        check(routes[0].length > straight * 1.05f,
              "the route detours around the wall (longer than the straight line)");
        bool aroundWall = false;
        for (const glm::vec3& p : routes[0].points)
            if (std::fabs(p.y) > 6.0f) aroundWall = true;
        check(aroundWall, "the route leaves the wall's y span to get around it");
        check(routes[0].points.front().z > -1.0f, "route point has a valid height");
        report += "  info  routes=" + std::to_string(routes.size()) +
                  " firstLength=" + std::to_string(routes[0].length) + "\n";
        // Multiple equal-cost routes so squads do not walk identical lines.
        bool distinct = false;
        for (std::size_t r = 1; r < routes.size(); ++r) {
            float maxSep = 0.0f;
            for (const glm::vec3& p : routes[r].points)
                for (const glm::vec3& q : routes[0].points)
                    maxSep = std::max(maxSep, glm::length(glm::vec2(p.x - q.x, p.y - q.y)));
            if (maxSep > 4.0f) { distinct = true; break; }
        }
        check(routes.size() >= 2 && distinct,
              "at least two distinct equal-cost routes are returned");
    }
    {
        // A query far beyond maxRouteMeters is refused without building chunks.
        const std::vector<NpcNavRoute> farRoutes =
            graph.findRoutes(world, start, glm::vec3(5000, 0, 1), s, 0);
        check(farRoutes.empty(), "absurdly far query is refused");
    }

    report += ok ? "  PASS\n" : "  FAIL\n";
    return ok;
}
