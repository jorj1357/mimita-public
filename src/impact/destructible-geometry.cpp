// 2026-09-28
/* purpose
* Implement the destructible-geometry owner: signed-distance helpers, spherical
* cut storage, and lazy planar surface rebuilds.
* Generated triangles are a cache written back into PhysicalEntity::localTriangles
* by the caller; the authoritative truth is the box minus the stored spheres.
* Does NOT decide cut size/damage (ImpactSystem) or own rigid-body motion.
*/

#include "impact/destructible-geometry.h"

#include <algorithm>
#include <cmath>

#include "impact/box-surface.h"

namespace MimitaImpact {

// ── Signed-distance helpers ─────────────────────────────────────────────
float boxDistance(glm::vec3 p, glm::vec3 halfSize)
{
    const glm::vec3 q = glm::abs(p) - halfSize;
    const float outside = glm::length(glm::max(q, glm::vec3(0.0f)));
    const float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
    return outside + inside;
}

float sphereDistance(glm::vec3 p, glm::vec3 center, float radius)
{
    return glm::length(p - center) - radius;
}

float destructibleCrateDistance(const DestructibleGeometry& geometry,
                                glm::vec3 localPoint)
{
    float result = boxDistance(localPoint, geometry.halfExtents);
    for (const DestructionCutSphere& cut : geometry.sphereCuts)
    {
        const float hole = sphereDistance(localPoint, cut.localCenter, cut.radius);
        // Subtract the sphere from the solid: inside a sphere becomes empty.
        result = std::max(result, -hole);
    }
    return result;
}

DestructibleGeometrySystem& DestructibleGeometrySystem::instance()
{
    static DestructibleGeometrySystem system;
    return system;
}

void DestructibleGeometrySystem::initialize(DestructibleGeometry& geometry,
                                            glm::vec3 halfExtents)
{
    geometry.enabled = true;
    geometry.broken = false;
    geometry.halfExtents = halfExtents;
    geometry.localOrigin = -halfExtents;
    geometry.localExtent = halfExtents * 2.0f;
    geometry.sphereCuts.clear();
    geometry.nextCutId = 1;
    geometry.geometryRevision = 0;
    geometry.renderVertices.clear();
    geometry.collisionTriangles.clear();
}

void DestructibleGeometrySystem::rebuild(DestructibleGeometry& geometry)
{
    buildDestructibleBoxSurface(geometry.halfExtents, geometry.sphereCuts,
                                geometry.renderVertices, geometry.collisionTriangles);
    ++geometry.geometryRevision;
}

int DestructibleGeometrySystem::addCut(DestructibleGeometry& geometry,
                                       const DestructionCutSphere& cut)
{
    if (!geometry.enabled)
        return 0;

    DestructionCutSphere stored = cut;
    if (stored.cutId == 0)
        stored.cutId = geometry.nextCutId++;
    geometry.sphereCuts.push_back(stored);

    rebuild(geometry);
    return 1;
}

int DestructibleGeometrySystem::rebuildAll(DestructibleGeometry& geometry)
{
    if (!geometry.enabled)
        return 0;
    rebuild(geometry);
    return 1;
}

} // namespace MimitaImpact
