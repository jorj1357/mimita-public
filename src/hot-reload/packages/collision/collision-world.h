// 09 17 2026
/* purpose
* Hot-side cached world collision geometry and spatial broadphase for the
* collision package. Fetches the map triangles once through the `world.collision`
* capability, builds a multi-resolution cell index, and answers one union-AABB
* candidate gather per movement region.
* Does NOT own collision response; see `collision-solver.cpp`.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace HotCollisionPackage {

struct WorldTri {
    glm::vec3 a;
    glm::vec3 b;
    glm::vec3 c;
};

// One owner of the hot world geometry cache and its index. Triangles are placed
// in every fine cell their AABB overlaps; triangles too large for the fine grid
// fall back to a coarser grid, and only pathological geometry is always tested,
// so a query never scans the whole map.
struct WorldCache {
    std::vector<WorldTri> tris;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> fine;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> coarse;
    std::vector<std::uint32_t> always;
    std::vector<std::uint32_t> visitStamp;
    std::uint32_t stamp = 0;
    std::uint32_t total = 0;
    std::uint64_t sampleHash = 0;
    bool ready = false;
};

WorldCache& worldCache();

// Load/refresh from `world.collision`. A cheap total + sample hash detects a map
// change without re-reading every triangle each tick.
bool ensureWorld(void* host);

// Install a synthetic triangle set (self-test / explicit world override) and
// rebuild the index.
void installWorld(const WorldTri* triangles, std::uint32_t count);

// The one broadphase gather for a region. Appends candidate triangle indices to
// `out` without duplicates and without scanning non-overlapping geometry.
void gatherCandidates(const glm::vec3& min, const glm::vec3& max,
                      std::vector<std::uint32_t>& out);

const WorldTri& triangle(std::uint32_t index);

// Closest point on a triangle to p (Ericson, Real-Time Collision Detection).
glm::vec3 closestPointOnTri(const glm::vec3& p, const WorldTri& t);

// Narrowphase: append every triangle in `candidates` the sphere overlaps.
struct SphereHit {
    std::int32_t triangle;
    glm::vec3 point;
    glm::vec3 normal;
    float penetration;
};
int gatherSphereHits(const glm::vec3& center, float radius,
                     const std::vector<std::uint32_t>& candidates,
                     SphereHit* out, int maxOut);

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
