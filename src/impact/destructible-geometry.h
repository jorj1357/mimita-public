// 2026-09-28
/* purpose
* Define the authoritative destructible-geometry record and its owner.
* A shape is stored as an original signed-distance box minus a list of spherical
* cuts; generated triangles are a rebuildable cache, never the source of truth.
* Does NOT render, send packets, or own rigid-body motion (PhysicalEntitySystem
* still owns collision + transforms).
* Does NOT decide whether a cut is created (ImpactSystem owns that).
*/

#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "map/map_common.h"          // Vertex
#include "physics/physics-types.h"   // CollisionTriangle, AABB

namespace MimitaImpact {

// One subtractive sphere cut, stored in the object's local space.
struct DestructionCutSphere
{
    uint64_t cutId = 0;
    glm::vec3 localCenter{0.0f};
    float radius = 0.0f;
    float damage = 0.0f;
    float energy = 0.0f;
    uint32_t materialId = 0;
    uint32_t sourceEntityId = 0;
};

// A spatial partition of the object. Only overlapping chunks are remeshed.
struct DestructionChunk
{
    AABB localBounds{};
    bool dirty = true;
    std::vector<Vertex> renderVertices;
    std::vector<CollisionTriangle> collisionTriangles;
};

// Contiguous slice of DestructibleGeometry::collisionTriangles owned by one
// chunk. Lets collision queries touch only chunks near the query instead of
// every generated triangle.
struct DestructionTriangleRange
{
    uint32_t first = 0;
    uint32_t count = 0;
};

// Authoritative compact destruction state embedded in PhysicalEntity.
struct DestructibleGeometry
{
    bool enabled = false;
    bool broken = false;
    float health = 100.0f;
    float maxHealth = 100.0f;

    // Source signed-distance box half size (local space).
    glm::vec3 halfExtents{0.5f};

    glm::vec3 localOrigin{0.0f};   // AABB min
    glm::vec3 localExtent{1.0f};   // AABB size

    std::vector<DestructionCutSphere> sphereCuts;
    std::vector<DestructionChunk> chunks;

    // One entry per chunk, indexing into collisionTriangles (set on refresh).
    std::vector<DestructionTriangleRange> chunkTriangleRanges;

    int chunkCountPerAxis = 3;
    int cellsPerChunkAxis = 8;     // marching-cubes cells per chunk axis

    uint64_t geometryRevision = 0;
    uint64_t nextCutId = 1;
    uint32_t materialId = 0;

    // Cached concatenation consumed by collision + rendering.
    std::vector<Vertex> renderVertices;
    std::vector<CollisionTriangle> collisionTriangles;
};

// Output of one mesher pass (per chunk or whole object).
struct GeneratedDestructionMesh
{
    std::vector<Vertex> renderVertices;
    std::vector<CollisionTriangle> collisionTriangles;
    uint64_t sourceRevision = 0;
    uint32_t solidSamples = 0;
    uint32_t totalSamples = 0;
    bool budgetExceeded = false;
};

// ── Signed-distance helpers (solid < 0, empty > 0) ──────────────────────
float boxDistance(glm::vec3 p, glm::vec3 halfSize);
float sphereDistance(glm::vec3 p, glm::vec3 center, float radius);

// Original box minus every stored sphere cut.
float destructibleCrateDistance(const DestructibleGeometry& geometry,
                                glm::vec3 localPoint);

// ── Owner ───────────────────────────────────────────────────────────────
// Thin orchestrator: builds the initial chunks, stores cuts, and rebuilds only
// dirty chunks. All geometry math lives in marching-cubes.*.
class DestructibleGeometrySystem
{
public:
    static DestructibleGeometrySystem& instance();

    // Builds the initial box mesh + chunk grid. Safe to call on a fresh record.
    void initialize(DestructibleGeometry& geometry, glm::vec3 halfExtents);

    // Stores one cut, marks overlapping chunks dirty, and rebuilds them.
    // Returns the number of chunks rebuilt (0 if nothing overlapped).
    int addCut(DestructibleGeometry& geometry, const DestructionCutSphere& cut);

    // Rebuilds only chunks flagged dirty, then refreshes the cached arrays.
    int rebuildDirtyChunks(DestructibleGeometry& geometry);
    int rebuildAll(DestructibleGeometry& geometry);

    // Appends the world-space triangles of every chunk whose (transformed) bounds
    // overlap queryWorld. This is the cached broadphase used by actor collision
    // and projectiles so a near query never scans the whole generated mesh.
    void collectWorldTriangles(const DestructibleGeometry& geometry,
                               const glm::mat4& transform,
                               const AABB& queryWorld,
                               std::vector<CollisionTriangle>& out) const;

    // Same chunk broadphase, but appends the (untransformed) local-space
    // triangles so a rigid-body solver can sweep them with its own transforms.
    void collectLocalTriangles(const DestructibleGeometry& geometry,
                               const glm::mat4& transform,
                               const AABB& queryWorld,
                               std::vector<CollisionTriangle>& out) const;

    // Safety cap: stop adding triangles once a single object exceeds this.
    size_t maxTrianglesPerEntity = 30000;

private:
    DestructibleGeometrySystem() = default;

    void refreshCachedArrays(DestructibleGeometry& geometry);
};

} // namespace MimitaImpact
