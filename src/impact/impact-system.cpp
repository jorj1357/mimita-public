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

#include <glm/gtc/quaternion.hpp>

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

// Deterministic network id for fracture fragment `pieceIndex` (1-based) of a
// parent. Both the server and every client derive the same value from the
// parent's network id and the deterministic piece order, so no extra packet is
// needed to name the pieces.
uint32_t fragmentNetworkId(uint32_t parentNetworkId, uint32_t pieceIndex)
{
    // Set a high bit so a generated fragment id cannot collide with the small
    // runtime entity ids the server otherwise assigns. Deterministic on both
    // sides (parent network id + piece order), so no packet is needed.
    return 0x40000000u | ((parentNetworkId & 0x7fffffu) << 4) | (pieceIndex & 0xfu);
}

} // anonymous namespace

// Splits `entity` into independent rigid bodies after a cut disconnected or
// unbalanced it. The hit entity keeps the largest piece (stable id/network id);
// each other piece becomes a new Dynamic PhysicalEntity that inherits material,
// density, mesh-derived mass, world pose, and a velocity seeded from the parent
// body's motion at that piece's centroid. Shared by the authoritative server
// and by clients reproducing a replicated cut. `serverDriven` marks the spawned
// children as non-authoritative mirrors. Returns the spawned entity ids.
std::vector<uint32_t> ImpactSystem::applyFracture(PhysicalEntity& entity,
                                                  ImpactResult& result,
                                                  bool serverDriven)
{
    std::vector<uint32_t> spawned;
    DestructibleGeometrySystem& geometrySystem = DestructibleGeometrySystem::instance();
    std::vector<BooleanPiece> pieces = geometrySystem.decomposePieces(entity.destructible);
    if (pieces.size() < 2)
        return spawned;

    const uint32_t maxFragments =
        std::max(1u, geometrySystem.fractureTuning.maxFragmentsPerEvent);
    const float minFraction =
        std::max(0.0f, geometrySystem.fractureTuning.minPieceVolumeFraction);
    const float minVolume =
        minFraction * std::max(entity.destructible.baseVolume, 1e-6f);

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    const uint32_t parentId = entity.id;
    const glm::mat4 parentTransform = entity.transform;
    const glm::quat parentOrientation = entity.orientation;
    const glm::vec3 parentVelocity = entity.velocity;
    const glm::vec3 parentAngular = entity.angularVelocity;
    const glm::vec3 parentCom = entity.destructible.massCenterOfMass;
    const glm::vec3 parentHalfExtents = entity.halfExtents;
    const PhysicalEntityShape parentShape = entity.shape;
    const float parentDensity = entity.density;
    const float parentFriction = entity.friction;
    const float parentRestitution = entity.restitution;
    const float parentLinearDamping = entity.linearDamping;
    const float parentAngularDamping = entity.angularDamping;
    const float parentMaxAngularSpeed = entity.maxAngularSpeed;
    const bool parentCollidesWithActors = entity.collidesWithActors;
    const std::string parentTexturePath = entity.texturePath;
    const std::string parentModelPath = entity.modelPath;

    // pieces[0] is the largest and keeps the original entity.
    const uint32_t materialId = effectiveMaterialId(entity);
    const uint32_t parentNetworkId = entity.networkId != 0 ? entity.networkId
                                                           : entity.id;

    uint32_t spawnedCount = 0;
    for (size_t i = 1; i < pieces.size() && spawnedCount < maxFragments; ++i)
    {
        const BooleanPiece& piece = pieces[i];
        if (piece.volume < minVolume || piece.mesh.empty())
            continue;

        // World pose of the piece: the parent transform already carries the
        // parent's translation; the piece geometry is in parent-local space, so
        // the child starts coincident and simply needs its own transform.
        const uint32_t childId = system.add(
            std::vector<CollisionTriangle>{}, parentTransform,
            PhysicalEntityMotion::Dynamic, materialId);
        PhysicalEntity* child = system.find(childId);
        if (!child)
            continue;

        child->networkId = fragmentNetworkId(parentNetworkId, (uint32_t)i);
        child->serverDriven = serverDriven;
        child->shape = parentShape;
        child->materialId = materialId;
        child->density = parentDensity;
        child->friction = parentFriction;
        child->restitution = parentRestitution;
        child->linearDamping = parentLinearDamping;
        child->angularDamping = parentAngularDamping;
        child->maxAngularSpeed = parentMaxAngularSpeed;
        child->collidesWithActors = parentCollidesWithActors;
        child->texturePath = parentTexturePath;
        child->modelPath = parentModelPath;

        ImpactSystem::instance().initializeEntityFromMesh(
            *child, piece.mesh, parentHalfExtents, materialId);

        // Velocity at the piece centroid from the parent's rigid-body motion:
        // v = v_cm + omega x (r - com). Off-center pieces fly outward, which is
        // what makes a burst look physical.
        const glm::vec3 r = parentOrientation * (piece.centroid - parentCom);
        child->velocity = parentVelocity + glm::cross(parentAngular, r);
        child->angularVelocity = parentAngular;

        if (spawned.size() < 16)
            result.fragmentEntityIds[spawned.size()] = childId;
        spawned.push_back(childId);
        ++spawnedCount;
    }

    // Keep only the primary piece on the original entity: rebuild it from the
    // largest shell so the hit entity never retains a disconnected fragment.
    // `system.add` above may have reallocated storage, so re-fetch the entity
    // by id rather than trusting the pointer.
    if (spawnedCount > 0)
    {
        PhysicalEntity* primary = system.find(parentId);
        if (primary)
        {
            ImpactSystem::instance().initializeEntityFromMesh(
                *primary, pieces[0].mesh, primary->halfExtents, materialId);
            primary->localTriangles = primary->destructible.collisionTriangles;
        }
    }
    return spawned;
}

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

    // The hole is driven by two independent, config-controlled terms:
    //   force: how much kinetic energy reaches the material (cbrt-compressed so
    //          the radius grows sensibly across a huge energy range), and
    //   size:  the source cross-section scaled by the weapon's `sizeScale`.
    // A small projectile with huge force still cuts a big hole; a large
    // projectile with little force cuts a small one. The projectile radius is
    // the floor, so a hole is never thinner than the thing that made it.
    const float sourceRadius = std::max(impact.radius, 0.0f);
    const float forceRadius =
        std::cbrt(std::max(effectiveEnergy, 0.0f)) * kCutRadiusEnergyScale;
    const float sizeRadius = sourceRadius * std::max(impact.sizeScale, 0.0f);
    const float radius = sourceRadius + forceRadius + sizeRadius;

    return glm::clamp(radius, sourceRadius,
                      std::max(sourceRadius, material.maxCutRadius));
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

    // A listen server and its local client can both submit the same shot, and
    // the same physical spot can resolve to a slightly different local centre
    // each time (sweep contact point, surface roughened by earlier cuts). Dedup
    // by source identity plus a tolerance scaled to the cut size, not a fixed
    // 1e-4 that rejects almost nothing real. A shared predictionKey is exact.
    // Only a shot with a real source identity can be a duplicate. A zero
    // source is an untracked impact and must always cut.
    if (cut.sourceEntityId != 0)
    {
        for (const DestructionCut& existing : entity->destructible.cuts)
        {
            if (existing.sourceEntityId != cut.sourceEntityId)
                continue;
            const bool sameKey = cut.predictionKey != 0 &&
                                 existing.predictionKey == cut.predictionKey;
            const float matchRadius =
                0.25f * std::max(existing.cutter.radius, cut.cutter.radius);
            const bool sameSpot =
                glm::length(existing.cutter.localCenter - cut.cutter.localCenter) <=
                std::max(matchRadius, 0.01f);
            if (sameKey || sameSpot)
            {
                result.applied = true;
                result.cutCreated = false;
                result.cutRadius = existing.cutter.radius;
                return result;
            }
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

    // Fracture: if the cut disconnected the object or left it hanging off its
    // support, split it into independent bodies. The trigger was recorded by
    // the rebuild; this owns spawning and the primary-piece keep rule.
    if (rebuilt != 0 &&
        entity->destructible.lastFractureReason != FractureReason::None)
    {
        std::vector<uint32_t> fragments = applyFracture(*entity, result, false);
        if (!fragments.empty())
        {
            result.fractured = true;
            result.fragmentCount = (uint32_t)fragments.size();
            Debug::log(Debug::Category::General,
                "[FRACTURE] entity=%u reason=%u pieces=%zu vol=%.4f\n",
                event.targetEntityId, (unsigned)entity->destructible.lastFractureReason,
                fragments.size() + 1, entity->destructible.remainingVolume);
        }
    }

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
