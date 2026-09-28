// 2026-09-28
/* purpose
* Implement the single generalized impact entry point.
* Computes kinetic energy, angle factor, material response, health damage, and a
* spherical destructive cut; routes the cut through DestructibleGeometrySystem.
* Does NOT own projectile flight, actor damage authority, or rendering.
*/

#include "impact/impact-system.h"

#include <algorithm>
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

    DestructibleGeometrySystem::instance().initialize(entity.destructible, halfExtents);

    // Generated triangles become the entity's collision mesh.
    entity.localTriangles = entity.destructible.collisionTriangles;
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

    DestructionCutSphere cut;
    cut.localCenter = localPoint;
    cut.radius = radius;
    cut.damage = damage;
    cut.energy = effectiveEnergy;
    cut.materialId = material.id.empty() ? 0u : materialIdForName(material.id);
    cut.sourceEntityId = resolved.sourceEntityId;

    const int rebuilt =
        DestructibleGeometrySystem::instance().addCut(entity->destructible, cut);

    entity->destructible.health =
        std::max(0.0f, entity->destructible.health - damage);
    entity->localTriangles = entity->destructible.collisionTriangles;

    result.applied = true;
    result.cutCreated = true;
    result.cutRadius = radius;
    result.damage = damage;
    result.chunksRebuilt = rebuilt;
    return result;
}

} // namespace MimitaImpact
