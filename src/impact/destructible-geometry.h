// 2026-09-28
/* purpose
* Define the authoritative destructible-geometry record and its owner.
* A shape is stored as an original box minus a list of spherical cuts; generated
* triangles are a rebuildable cache, never the source of truth. Meshing is lazy:
* nothing is generated until the first cut, so an untouched object keeps the
* cheap 12-triangle box the caller authored.
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

// Authoritative compact destruction state embedded in PhysicalEntity.
struct DestructibleGeometry
{
    bool enabled = false;
    bool broken = false;
    float health = 100.0f;
    float maxHealth = 100.0f;

    // Source box half size (local space, centred at the origin).
    glm::vec3 halfExtents{0.5f};

    glm::vec3 localOrigin{0.0f};   // AABB min
    glm::vec3 localExtent{1.0f};   // AABB size

    std::vector<DestructionCutSphere> sphereCuts;

    uint64_t geometryRevision = 0;
    uint64_t nextCutId = 1;
    uint32_t materialId = 0;

    // Cached low-poly surface consumed by collision + rendering. Empty until the
    // first cut; the caller keeps its authored box mesh until then.
    std::vector<Vertex> renderVertices;
    std::vector<CollisionTriangle> collisionTriangles;
};

// ── Signed-distance helpers (solid < 0, empty > 0) ──────────────────────
float boxDistance(glm::vec3 p, glm::vec3 halfSize);
float sphereDistance(glm::vec3 p, glm::vec3 center, float radius);

// Original box minus every stored sphere cut.
float destructibleCrateDistance(const DestructibleGeometry& geometry,
                                glm::vec3 localPoint);

// ── Owner ───────────────────────────────────────────────────────────────
// Stores cuts and rebuilds the planar surface lazily. A cut rebuilds the whole
// surface from the stored spheres, which is what keeps the triangle count low
// and bounded (an uncut box is 12 triangles; each hole adds a fixed number).
class DestructibleGeometrySystem
{
public:
    static DestructibleGeometrySystem& instance();

    // Resets the record for a fresh box. Emits no geometry: the caller's box
    // mesh remains authoritative until addCut is called.
    void initialize(DestructibleGeometry& geometry, glm::vec3 halfExtents);

    // Stores one cut and rebuilds the surface. Returns 1 when the surface was
    // (re)generated, 0 when the record is disabled.
    int addCut(DestructibleGeometry& geometry, const DestructionCutSphere& cut);

    // Rebuilds the surface from the stored cuts (used after bulk changes).
    int rebuildAll(DestructibleGeometry& geometry);

    // Safety cap for the generated surface.
    size_t maxTrianglesPerEntity = 30000;

private:
    DestructibleGeometrySystem() = default;

    void rebuild(DestructibleGeometry& geometry);
};

} // namespace MimitaImpact
