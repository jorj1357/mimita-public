// 2026-09-30
/* purpose
* Apply replicated destructible physical entities on a remote client. Spawn
* rebuilds the same canonical base the server used (box half extents or a GLB
* import), the cut event appends the same ordered cut, the state packet drives
* the visible transform, and despawn removes the mirror.
* Mirrors are marked serverDriven so the fixed tick never simulates them.
* Does NOT own authority, send packets, or predict cuts on mirrors.
*/

#include "network/multiplayer-context.h"

#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "config/material-config.h"
#include "impact/destructible-mesh-loader.h"
#include "impact/impact-system.h"
#include "network/packets.h"
#include "physics/physical-entity.h"

namespace MimitaNet {

void mpProcessPhysicalEntitySpawnEventPacket(
    MultiplayerContext&, const PhysicalEntitySpawnEventPacket* event)
{
    const glm::vec3 halfExtents(event->halfExtents[0], event->halfExtents[1],
                                event->halfExtents[2]);

    std::vector<CollisionTriangle> base;
    MimitaImpact::BooleanMesh baseMesh;
    bool haveMeshBase = false;

    if (event->sourceKind == PHYSICAL_ENTITY_SOURCE_GLB && event->modelPath[0] != '\0')
    {
        const MimitaImpact::DestructibleMeshLoad load =
            MimitaImpact::loadDestructibleMeshFromGLB(event->modelPath);
        if (!load.success)
        {
            printf("[PHYS CLIENT] spawn rejected model=%s reason=%s\n",
                   event->modelPath, load.error.c_str());
            return;
        }
        baseMesh = load.mesh;
        haveMeshBase = true;
    }
    else
    {
        buildBoxCollisionTriangles(base, glm::vec3(0.0f), halfExtents);
    }

    const glm::quat orientation(event->orientation[0], event->orientation[1],
                                event->orientation[2], event->orientation[3]);
    glm::mat4 transform(1.0f);
    transform = glm::mat4_cast(glm::normalize(orientation));
    transform[3] = glm::vec4(event->position[0], event->position[1],
                             event->position[2], 1.0f);

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    const uint32_t localId = system.addReplicated(
        event->networkId, base, transform, (PhysicalEntityMotion)event->motion,
        event->materialId);
    if (localId == 0)
    {
        // Already present (for example a re-sent reliable spawn): refresh pose.
        if (PhysicalEntity* existing = system.findByNetworkId(event->networkId))
        {
            existing->transform = transform;
            existing->previousTransform = transform;
            existing->orientation = glm::normalize(orientation);
        }
        return;
    }

    PhysicalEntity* e = system.find(localId);
    if (!e)
        return;
    e->halfExtents = halfExtents;
    e->density = event->density > 0.0f ? event->density : 1.0f;
    e->velocity = glm::vec3(event->velocity[0], event->velocity[1],
                            event->velocity[2]);
    e->angularVelocity = glm::vec3(event->angularVelocity[0],
                                   event->angularVelocity[1],
                                   event->angularVelocity[2]);
    e->texturePath = event->texturePath;

    if (MimitaImpact::MaterialConfig::instance().revision() == 0)
        MimitaImpact::MaterialConfig::instance().load();

    if (haveMeshBase)
        MimitaImpact::ImpactSystem::instance().initializeEntityFromMesh(
            *e, std::move(baseMesh), halfExtents, event->materialId);
    else
        MimitaImpact::ImpactSystem::instance().initializeEntity(*e, event->materialId,
                                                                halfExtents);

    printf("[PHYS CLIENT] spawn networkId=%u localId=%u tris=%zu half=(%.2f %.2f %.2f)\n",
           event->networkId, localId, e->localTriangles.size(),
           halfExtents.x, halfExtents.y, halfExtents.z);
}

void mpProcessPhysicalEntityDespawnEventPacket(
    MultiplayerContext&, const PhysicalEntityDespawnEventPacket* event)
{
    if (PhysicalEntitySystem::instance().removeByNetworkId(event->networkId))
        printf("[PHYS CLIENT] despawn networkId=%u\n", event->networkId);
}

void mpProcessPhysicalEntityStatePacket(MultiplayerContext&,
                                        const PhysicalEntityStatePacket* event)
{
    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    const uint16_t count = event->entityCount > MAX_PHYSICAL_ENTITY_STATE_ENTRIES
        ? MAX_PHYSICAL_ENTITY_STATE_ENTRIES
        : event->entityCount;
    for (uint16_t i = 0; i < count; ++i)
    {
        const PhysicalEntityStateEntry& entry = event->entities[i];
        PhysicalEntity* e = system.findByNetworkId(entry.networkId);
        if (!e)
            continue; // State can arrive before the reliable spawn; spawn sets it.

        const glm::quat orientation(entry.orientation[0], entry.orientation[1],
                                    entry.orientation[2], entry.orientation[3]);
        // The interpolated render pose uses previousTransform -> transform, so
        // keep the last pose as previous before writing the new one.
        e->previousTransform = e->transform;
        e->transform = glm::mat4_cast(glm::normalize(orientation));
        e->transform[3] = glm::vec4(entry.position[0], entry.position[1],
                                    entry.position[2], 1.0f);
        e->orientation = glm::normalize(orientation);
        e->velocity = glm::vec3(entry.velocity[0], entry.velocity[1],
                                entry.velocity[2]);
        e->angularVelocity = glm::vec3(entry.angularVelocity[0],
                                       entry.angularVelocity[1],
                                       entry.angularVelocity[2]);
    }
}

void mpProcessEntityCutEventPacket(MultiplayerContext&,
                                   const PhysicalEntityCutEventPacket* event)
{
    PhysicalEntity* e =
        PhysicalEntitySystem::instance().findByNetworkId(event->networkId);
    if (!e)
    {
        printf("[PHYS CLIENT] cut for unknown networkId=%u\n", event->networkId);
        return;
    }
    if (!e->destructible.enabled)
        return;

    // Explicit ordered identity: the server sends cuts in order and never
    // re-sends one. Ignore anything already present so a retransmit cannot
    // corrupt the authoritative history.
    for (const MimitaImpact::DestructionCut& existing : e->destructible.cuts)
        if (existing.cutId == event->cutId)
            return;

    MimitaImpact::DestructionCut cut;
    cut.cutId = event->cutId;
    cut.cutter.type = (MimitaImpact::BooleanCutterType)event->cutterType;
    cut.cutter.localCenter = glm::vec3(event->localCenter[0], event->localCenter[1],
                                       event->localCenter[2]);
    cut.cutter.localDirection = glm::vec3(event->localDirection[0],
                                          event->localDirection[1],
                                          event->localDirection[2]);
    cut.cutter.radius = event->radius;
    cut.cutter.length = event->length;
    cut.damage = event->damage;
    cut.energy = event->energy;
    cut.materialId = event->materialId;
    cut.sourceEntityId = event->sourceEntityId;
    cut.predictionKey = event->predictionKey;

    if (MimitaImpact::DestructibleGeometrySystem::instance().addCut(
            e->destructible, cut) == 0)
    {
        printf("[PHYS CLIENT] cut rejected networkId=%u cutId=%u err=%u\n",
               event->networkId, event->cutId, (unsigned)e->destructible.lastError);
        return;
    }

    e->localTriangles = e->destructible.collisionTriangles;
    e->destructible.health =
        std::max(0.0f, e->destructible.health - event->damage);
    refreshEntityMassProperties(*e);

    printf("[PHYS CLIENT] cut networkId=%u cutId=%u tris=%zu vol=%.4f\n",
           event->networkId, event->cutId, e->localTriangles.size(),
           e->destructible.remainingVolume);
}

} // namespace MimitaNet