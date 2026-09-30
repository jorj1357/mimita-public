// 2026-09-30
/* purpose
* Implement the destructible-geometry owner: signed-distance helpers, cut
* history storage, and surface rebuilds via the MiMITA boolean wrapper.
* Generated triangles are a cache written back into the entity collision mesh by
* the caller; the authoritative truth is the base mesh minus the stored cutters.
* Does NOT decide cut size/damage (ImpactSystem) or own rigid-body motion.
*/

#include "impact/destructible-geometry.h"

#include <algorithm>
#include <cmath>

#include "physics/mesh-mass-properties.h"

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

static float capsuleDistance(glm::vec3 p, const BooleanCutter& cutter)
{
    glm::vec3 dir = cutter.localDirection;
    const float dirLen = glm::length(dir);
    dir = dirLen > 1e-6f ? dir / dirLen : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 a = cutter.localCenter - dir * (cutter.length * 0.5f);
    const glm::vec3 b = cutter.localCenter + dir * (cutter.length * 0.5f);
    const glm::vec3 ab = b - a;
    const float denom = glm::dot(ab, ab);
    const float t = denom > 1e-9f
        ? glm::clamp(glm::dot(p - a, ab) / denom, 0.0f, 1.0f)
        : 0.0f;
    return glm::length(p - (a + ab * t)) - cutter.radius;
}

float destructibleCrateDistance(const DestructibleGeometry& geometry,
                                glm::vec3 localPoint)
{
    float result = boxDistance(localPoint, geometry.halfExtents);
    for (const DestructionCut& cut : geometry.cuts)
    {
        const float hole = cut.cutter.type == BooleanCutterType::Capsule
            ? capsuleDistance(localPoint, cut.cutter)
            : sphereDistance(localPoint, cut.cutter.localCenter, cut.cutter.radius);
        // Subtract the cutter from the solid: inside a cutter becomes empty.
        result = std::max(result, -hole);
    }
    return result;
}

namespace {

uint64_t gNextSessionId = 1;

// Fills the render/collision cache from one boolean surface.
void fillSurface(DestructibleGeometry& geometry, const BooleanMesh& mesh)
{
    geometry.renderVertices.clear();
    geometry.collisionTriangles.clear();
    geometry.renderVertices.reserve(mesh.vertices.size());
    geometry.collisionTriangles.reserve(mesh.triangleCount());

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const BooleanMeshVertex& v0 = mesh.vertices[mesh.indices[i]];
        const BooleanMeshVertex& v1 = mesh.vertices[mesh.indices[i + 1]];
        const BooleanMeshVertex& v2 = mesh.vertices[mesh.indices[i + 2]];

        geometry.collisionTriangles.push_back(
            {v0.position, v1.position, v2.position, v0.normal});
        geometry.renderVertices.push_back({v0.position, v0.normal, v0.uv});
        geometry.renderVertices.push_back({v1.position, v1.normal, v1.uv});
        geometry.renderVertices.push_back({v2.position, v2.normal, v2.uv});
    }
}

// Integrates the cached surface and stores the mass properties, once per
// revision. Signed integrals subtract inward-wound cavity walls.
MeshMassProperties applyMassFromTriangles(DestructibleGeometry& geometry,
                                          const std::vector<CollisionTriangle>& triangles)
{
    const MeshMassProperties mass = computeMeshMassProperties(triangles);
    if (mass.valid)
    {
        geometry.massCenterOfMass = mass.centerOfMass;
        geometry.unitInertiaDiagonal = mass.unitInertiaDiagonal;
    }
    else
    {
        geometry.massCenterOfMass = glm::vec3(0.0f);
        geometry.unitInertiaDiagonal = glm::vec3(1.0f);
    }
    return mass;
}

} // namespace

DestructibleGeometrySystem& DestructibleGeometrySystem::instance()
{
    static DestructibleGeometrySystem system;
    return system;
}

void DestructibleGeometrySystem::initialize(DestructibleGeometry& geometry,
                                            glm::vec3 halfExtents)
{
    booleanSessionRelease(geometry.booleanSessionId);
    geometry.booleanSessionId = gNextSessionId++;
    geometry.enabled = true;
    geometry.broken = false;
    geometry.halfExtents = halfExtents;
    geometry.baseMesh = buildBooleanBoxMesh(halfExtents, geometry.materialId);
    geometry.cuts.clear();
    geometry.nextCutId = 1;
    geometry.geometryRevision = 0;
    geometry.baseVolume = 8.0f * halfExtents.x * halfExtents.y * halfExtents.z;
    geometry.remainingVolume = geometry.baseVolume;
    geometry.shellCount = 0;
    geometry.componentCount = 0;
    geometry.lastError = BooleanError::None;
    geometry.renderVertices.clear();
    geometry.collisionTriangles.clear();
    geometry.massFromMesh = false;

    // Pre-cut mass properties of the authored box at unit density.
    const glm::vec3 dim = glm::max(halfExtents * 2.0f, glm::vec3(0.001f));
    const float m = geometry.baseVolume;
    geometry.massCenterOfMass = glm::vec3(0.0f);
    geometry.unitInertiaDiagonal = glm::vec3(
        m * (dim.y * dim.y + dim.z * dim.z) / 12.0f,
        m * (dim.x * dim.x + dim.z * dim.z) / 12.0f,
        m * (dim.x * dim.x + dim.y * dim.y) / 12.0f);
}

void DestructibleGeometrySystem::initializeFromMesh(DestructibleGeometry& geometry,
                                                    BooleanMesh baseMesh,
                                                    glm::vec3 halfExtents)
{
    booleanSessionRelease(geometry.booleanSessionId);
    geometry.booleanSessionId = gNextSessionId++;
    geometry.enabled = true;
    geometry.broken = false;
    geometry.halfExtents = glm::max(halfExtents, glm::vec3(1e-3f));
    geometry.baseMesh = std::move(baseMesh);
    geometry.cuts.clear();
    geometry.nextCutId = 1;
    geometry.geometryRevision = 0;
    geometry.shellCount = 0;
    geometry.componentCount = 0;
    geometry.lastError = BooleanError::None;

    fillSurface(geometry, geometry.baseMesh);
    const MeshMassProperties mass =
        applyMassFromTriangles(geometry, geometry.collisionTriangles);
    geometry.massFromMesh = true;
    geometry.remainingVolume = mass.valid ? mass.volume : 0.0f;
    geometry.baseVolume = geometry.remainingVolume;
}

void DestructibleGeometrySystem::release(DestructibleGeometry& geometry)
{
    booleanSessionRelease(geometry.booleanSessionId);
    geometry.booleanSessionId = 0;
    geometry.enabled = false;
}

bool DestructibleGeometrySystem::rebuild(DestructibleGeometry& geometry)
{
    std::vector<BooleanCutter> cutters;
    cutters.reserve(geometry.cuts.size());
    for (const DestructionCut& cut : geometry.cuts)
        cutters.push_back(cut.cutter);

    const BooleanCutResult result = booleanSubtractIncremental(
        geometry.booleanSessionId, geometry.baseMesh, cutters);
    geometry.lastError = result.error;
    if (!result.success)
        return false;
    if (result.triangleCount > maxTrianglesPerEntity)
    {
        geometry.lastError = BooleanError::ResultTooLarge;
        return false;
    }

    fillSurface(geometry, result.mesh);

    geometry.remainingVolume = result.remainingVolume;
    geometry.shellCount = result.shellCount;
    geometry.componentCount = result.componentCount;

    // Mass properties follow the material that remains. Integrated from the
    // generated surface here, once per rebuild, so the per-tick physics refresh
    // only reads cached values. Cavity walls are wound inward, so the signed
    // integrals subtract them.
    applyMassFromTriangles(geometry, geometry.collisionTriangles);

    ++geometry.geometryRevision;
    return true;
}

int DestructibleGeometrySystem::addCut(DestructibleGeometry& geometry,
                                       const DestructionCut& cut)
{
    if (!geometry.enabled)
        return 0;

    DestructionCut stored = cut;
    if (stored.cutId == 0)
        stored.cutId = geometry.nextCutId++;
    geometry.cuts.push_back(stored);

    if (!rebuild(geometry))
    {
        // Roll back so an invalid cut never corrupts the authoritative history.
        geometry.cuts.pop_back();
        if (stored.cutId != 0)
            geometry.nextCutId = stored.cutId;
        return 0;
    }
    return 1;
}

int DestructibleGeometrySystem::rebuildAll(DestructibleGeometry& geometry)
{
    if (!geometry.enabled)
        return 0;
    return rebuild(geometry) ? 1 : 0;
}

} // namespace MimitaImpact
