// 2026-09-28
/* purpose
* Implement the single generalized impact entry point.
* Computes kinetic energy, angle factor, material response, health damage, and a
* spherical destructive cut; routes the cut through DestructibleGeometrySystem
* and refreshes the entity's mass, center of mass, and inertia for the material
* that remains.
* Does NOT own projectile flight, actor damage authority, or rendering.
*/

#include "impact/impact-system.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "config/material-config.h"
#include "debug/debug-log.h"
#include "impact/destructible-geometry.h"
#include "physics/physical-entity.h"

namespace MimitaImpact {
namespace {

// Gameplay-stable damage scale: converts effective destructive energy into
// nominal health damage. Tunable; not presented as physically exact.
constexpr float kDamagePerEnergy = 0.01f;

// Effective cut energy formula constant (cbrt(energy) * scale + base radius).
constexpr float kCutRadiusEnergyScale = 0.02f;

uint32_t effectiveMaterialId(const PhysicalEntity& entity)
{
    return entity.destructible.materialId != 0
        ? entity.destructible.materialId
        : entity.materialId;
}

} // anonymous namespace

ImpactSystem& ImpactSystem::instance()
{
    static ImpactSystem system;
    return system;
}

float ImpactSystem::kineticEnergy(float mass, float speed)
{
    return 0.5f * mass * speed * speed;
}

float ImpactSystem::impactAngleFactor(const glm::vec3& projectileDirection,
                                      const glm::vec3& surfaceNormal)
{
    const float dirLen = glm::length(projectileDirection);
    const float nrmLen = glm::length(surfaceNormal);
    if (dirLen < 1e-6f || nrmLen < 1e-6f)
        return 0.0f;
    const float directness =
        glm::dot(-projectileDirection / dirLen, surfaceNormal / nrmLen);
    return glm::clamp(directness, 0.0f, 1.0f);
}

float ImpactSystem::calculateCutRadius(const ImpactEvent& impact,
                                       const MaterialDefinition& material)
{
    const float effectiveEnergy =
        impact.energy *
        impactAngleFactor(impact.worldDirection, impact.worldNormal) *
        impact.cutScale *
        material.holeEnergyScale;

    const float radius =
        impact.radius +
        std::cbrt(std::max(effectiveEnergy, 0.0f)) * kCutRadiusEnergyScale;

    return glm::clamp(radius, impact.radius, std::max(impact.radius, material.maxCutRadius));
}

void ImpactSystem::initializeEntity(PhysicalEntity& entity, uint32_t materialId,
                                    glm::vec3 halfExtents)
{
    const MaterialDefinition& material = MaterialConfig::instance().find(materialId);

    entity.destructible.enabled = true;
    entity.destructible.health = material.strength;
    entity.destructible.maxHealth = material.strength;
    entity.destructible.materialId = materialId;

    // Lazy: no geometry until the first cut, so the caller's authored box mesh
    // (already in entity.localTriangles) stays authoritative at spawn.
    DestructibleGeometrySystem::instance().initialize(entity.destructible, halfExtents);
}

void ImpactSystem::initializeEntityFromMesh(PhysicalEntity& entity,
                                            BooleanMesh baseMesh,
                                            glm::vec3 halfExtents,
                                            uint32_t materialId)
{
    const MaterialDefinition& material = MaterialConfig::instance().find(materialId);

    entity.destructible.enabled = true;
    entity.destructible.health = material.strength;
    entity.destructible.maxHealth = material.strength;
    entity.destructible.materialId = materialId;

    DestructibleGeometrySystem::instance().initializeFromMesh(
        entity.destructible, std::move(baseMesh), halfExtents);
    entity.localTriangles = entity.destructible.collisionTriangles;
    refreshEntityMassProperties(entity);
}

ImpactResult ImpactSystem::submit(const ImpactEvent& event)
{
    ImpactResult result;

    if (event.target != ImpactTarget::PhysicalEntity)
    {
        if ((mIgnoredLogCounter++ % 240u) == 0u)
            Debug::log(Debug::Category::General,
                "[IMPACT] ignored target kind=%u source=%u (adapter not implemented)\n",
                (unsigned)event.target, (unsigned)event.source);
        return result;
    }

    PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(event.targetEntityId);
    if (!entity || !entity->destructible.enabled)
    {
        if ((mIgnoredLogCounter++ % 240u) == 0u)
            Debug::log(Debug::Category::General,
                "[IMPACT] ignored: target entity %u not destructible\n",
                event.targetEntityId);
        return result;
    }

    // A remote client's mirror is not an authority. Predicting a cut on it
    // would desync the ordered cut history; the reliable server cut event
    // applies it instead.
    if (entity->serverDriven)
    {
        if ((mIgnoredLogCounter++ % 240u) == 0u)
            Debug::log(Debug::Category::General,
                "[IMPACT] deferred: target entity %u is a replicated mirror\n",
                event.targetEntityId);
        return result;
    }

    const MaterialDefinition& material =
        MaterialConfig::instance().find(effectiveMaterialId(*entity));

    ImpactEvent resolved = event;
    resolved.energy = kineticEnergy(resolved.mass, resolved.speed);

    const float angle =
        impactAngleFactor(resolved.worldDirection, resolved.worldNormal);
    const float effectiveEnergy =
        resolved.energy * angle * resolved.cutScale * material.holeEnergyScale;
    const float damage = effectiveEnergy * kDamagePerEnergy;
    const float radius = calculateCutRadius(resolved, material);

    // Convert the world-space hit into the entity's local space.
    const glm::mat4 inverse = glm::inverse(entity->transform);
    const glm::vec3 localPoint =
        glm::vec3(inverse * glm::vec4(resolved.worldPoint, 1.0f));

    DestructionCut cut;
    cut.cutter.type = BooleanCutterType::Sphere;
    cut.cutter.localCenter = localPoint;
    cut.cutter.radius = radius;
    cut.damage = damage;
    cut.energy = effectiveEnergy;
    cut.materialId = material.id.empty() ? 0u : materialIdForName(material.id);
    cut.sourceEntityId = resolved.sourceEntityId;
    cut.predictionKey = resolved.eventId;

    // A listen server and its local client can observe the same predicted
    // projectile. The authoritative cut must not be added twice when both
    // sides submit the same impact. Identity is (source, local position), not
    // approximate world position.
    for (const DestructionCut& existing : entity->destructible.cuts)
    {
        if (existing.sourceEntityId == cut.sourceEntityId &&
            glm::length(existing.cutter.localCenter - cut.cutter.localCenter) < 0.0001f)
        {
            result.applied = true;
            result.cutCreated = false;
            result.cutRadius = existing.cutter.radius;
            return result;
        }
    }

    const auto booleanStart = std::chrono::steady_clock::now();
    const int rebuilt =
        DestructibleGeometrySystem::instance().addCut(entity->destructible, cut);
    const float booleanMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - booleanStart).count();

    entity->destructible.health =
        std::max(0.0f, entity->destructible.health - damage);
    entity->localTriangles = entity->destructible.collisionTriangles;
    // Material was removed: follow it with the rigid-body mass, center of mass,
    // and inertia immediately (the per-tick owner reads the same cached values).
    if (rebuilt != 0)
        refreshEntityMassProperties(*entity);

    // Bounded, categorized diagnostics at the boolean owner. Failures always
    // record; successful cuts are rate-limited so a long burst stays readable.
    if (rebuilt == 0)
    {
        Debug::error(Debug::Category::General,
            "[BOOLEAN] FAILED entity=%u src=%u radius=%.3f cuts=%zu triLimit=%zu "
            "bool=%.2fms err=%u\n",
            event.targetEntityId, resolved.sourceEntityId, radius,
            entity->destructible.cuts.size(),
            DestructibleGeometrySystem::instance().maxTrianglesPerEntity,
            booleanMs, (unsigned)entity->destructible.lastError);
    }
    else if ((mCutLogCounter++ % 4u) == 0u)
    {
        Debug::log(Debug::Category::General,
            "[BOOLEAN] entity=%u src=%u radius=%.3f cuts=%zu tris=%zu vol=%.4f "
            "mass=%.1f com=(%.3f %.3f %.3f) comps=%u shells=%u bool=%.2fms\n",
            event.targetEntityId, resolved.sourceEntityId, radius,
            entity->destructible.cuts.size(), entity->localTriangles.size(),
            entity->destructible.remainingVolume, entity->mass,
            entity->centerOfMass.x, entity->centerOfMass.y, entity->centerOfMass.z,
            entity->destructible.componentCount,
            entity->destructible.shellCount, booleanMs);
    }

    result.applied = true;
    result.cutCreated = rebuilt != 0;
    result.cutRadius = radius;
    result.damage = damage;
    result.chunksRebuilt = rebuilt;
    result.triangleCount = (uint32_t)entity->localTriangles.size();
    result.componentCount = entity->destructible.componentCount;
    result.remainingVolume = entity->destructible.remainingVolume;
    result.error = entity->destructible.lastError;
    return result;
}

} // namespace MimitaImpact
