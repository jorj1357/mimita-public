// 2026-10-30
/* purpose
* Resolve shared world impacts against imported closed GLB solids. A struck
* solid becomes one static PhysicalEntity only when first hit, so the existing
* Boolean, fragment, and replication owners handle subsequent cuts.
* The headless map retains its authored triangles for round restoration.
*/

#include "network/server.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

#include "config/material-config.h"
#include "debug/structured-log.h"
#include "impact/destructible-mesh-loader.h"
#include "impact/impact-system.h"
#include "physics/physical-entity.h"

namespace MimitaNet {
namespace {

uint32_t nearestSolid(const HeadlessWorld& world,
                      const MimitaImpact::ImpactEvent& impact)
{
    if (impact.targetEntityId != 0 &&
        impact.targetEntityId <= world.solidIds.size())
    {
        uint32_t id = world.solidIds[impact.targetEntityId - 1];
        if (id == UINT32_MAX &&
            impact.targetEntityId <= world.destructionBaselineSolidIds.size())
            id = world.destructionBaselineSolidIds[impact.targetEntityId - 1];
        if (id != 0 && id != UINT32_MAX)
            return id;
    }

    const float cs = world.collisionChunkSize;
    if (cs <= 0.0f)
        return 0;
    const glm::ivec3 cell = glm::ivec3(glm::floor(impact.worldPoint / cs));
    const auto it = world.collisionChunks.find(cell);
    if (it == world.collisionChunks.end())
        return 0;

    float best = std::numeric_limits<float>::max();
    uint32_t solid = 0;
    for (int index : it->second)
    {
        if (index < 0 || (size_t)index >= world.triangles.size() ||
            (size_t)index >= world.solidIds.size())
            continue;
        uint32_t id = world.solidIds[index];
        if (id == UINT32_MAX &&
            (size_t)index < world.destructionBaselineSolidIds.size())
            id = world.destructionBaselineSolidIds[index];
        if (id == 0 || id == UINT32_MAX)
            continue;
        const CollisionTriangle& tri = world.triangles[index];
        const float plane = std::abs(glm::dot(impact.worldPoint - tri.a,
                                             tri.normal));
        if (plane > std::max(0.5f, impact.radius + 0.2f))
            continue;
        const glm::vec3 lo = glm::min(tri.a, glm::min(tri.b, tri.c));
        const glm::vec3 hi = glm::max(tri.a, glm::max(tri.b, tri.c));
        const glm::vec3 miss = glm::max(glm::max(lo - impact.worldPoint,
                                                  impact.worldPoint - hi),
                                        glm::vec3(0.0f));
        const float score = plane + glm::length(miss);
        if (score < best)
        {
            best = score;
            solid = id;
        }
    }
    return solid;
}

} // namespace

void flushServerWorldImpacts(HeadlessWorld& world, World& npcWorld,
                             const std::string& mapId, uint32_t tick)
{
    std::vector<MimitaImpact::ImpactEvent> impacts =
        MimitaImpact::ImpactSystem::instance().takeWorldImpacts();
    bool collisionChanged = false;
    for (MimitaImpact::ImpactEvent& impact : impacts)
    {
        const uint32_t solidId = nearestSolid(world, impact);
        if (solidId == 0)
            continue;
        const HeadlessGLBObject* object = nullptr;
        for (const HeadlessGLBObject& candidate : world.glbObjects)
            if (candidate.solidId == solidId)
            {
                object = &candidate;
                break;
            }
        if (!object || object->nodeIndex < 0)
            continue;

        uint32_t entityId = 0;
        const auto active = world.activeDestructibleNodes.find(object->nodeIndex);
        if (active != world.activeDestructibleNodes.end())
            entityId = active->second;
        else
        {
            const std::string model = "assets/maps/" + mapId + ".glb#node:" +
                                      std::to_string(object->nodeIndex);
            MimitaImpact::DestructibleMeshLoad mesh =
                MimitaImpact::loadDestructibleMeshFromGLB(model);
            if (!mesh.success)
            {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "world.destructible-rejected", object->id,
                    mesh.error, tick,
                    {{"map", mapId}, {"node", object->nodeIndex}});
                continue;
            }
            if (MimitaImpact::MaterialConfig::instance().revision() == 0)
                MimitaImpact::MaterialConfig::instance().load();
            const bool floor = mesh.halfExtents.x >= 1.0f &&
                               mesh.halfExtents.y >= 1.0f &&
                               mesh.halfExtents.z * 4.0f <
                                   std::min(mesh.halfExtents.x, mesh.halfExtents.y);
            const uint32_t materialId = MimitaImpact::materialIdForName(
                floor ? "foundation" : "concrete");
            glm::mat4 transform = glm::translate(glm::mat4(1.0f),
                                                  mesh.sourceCenter);
            entityId = PhysicalEntitySystem::instance().add(
                {}, transform, PhysicalEntityMotion::Static, materialId);
            PhysicalEntity* entity = PhysicalEntitySystem::instance().find(entityId);
            if (!entity)
                continue;
            entity->modelPath = model;
            entity->halfExtents = mesh.halfExtents;
            entity->density = MimitaImpact::MaterialConfig::instance()
                                  .find(materialId).density;
            MimitaImpact::ImpactSystem::instance().initializeEntityFromMesh(
                *entity, std::move(mesh.mesh), entity->halfExtents, materialId);
            if (world.destructionBaselineSolidIds.empty())
                world.destructionBaselineSolidIds = world.solidIds;
            world.activeDestructibleNodes.emplace(object->nodeIndex, entityId);
            for (uint32_t& id : world.solidIds)
                if (id == solidId)
                    id = UINT32_MAX;
            collisionChanged = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "world.destructible-activated", object->id,
                "closed map solid moved to shared destruction owner", tick,
                {{"map", mapId}, {"node", object->nodeIndex},
                 {"entity", entityId}, {"material", floor ? "foundation" : "concrete"}});
        }
        impact.target = MimitaImpact::ImpactTarget::PhysicalEntity;
        impact.targetEntityId = entityId;
        MimitaImpact::ImpactSystem::instance().submit(impact);
    }
    uint64_t revision = world.activeDestructibleNodes.size();
    for (const auto& active : world.activeDestructibleNodes)
    {
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(active.second);
        if (entity)
            revision += entity->destructible.geometryRevision *
                (uint64_t)(active.first + 1);
    }
    if (collisionChanged)
        buildHeadlessCollisionChunks(world);
    if (collisionChanged || revision != world.lastNpcDestructionRevision)
    {
        buildNpcWorldCollision(npcWorld, world);
        world.lastNpcDestructionRevision = revision;
    }
}

void resetServerWorldDestruction(HeadlessWorld& world, World& npcWorld,
                                 uint32_t tick)
{
    if (world.destructionBaselineSolidIds.empty())
        return;
    world.solidIds = std::move(world.destructionBaselineSolidIds);
    for (const auto& item : world.activeDestructibleNodes)
        PhysicalEntitySystem::instance().remove(item.second);
    world.activeDestructibleNodes.clear();
    world.lastNpcDestructionRevision = 0;
    std::vector<uint32_t> fragments;
    for (const PhysicalEntity& entity : PhysicalEntitySystem::instance().entities())
        if (entity.isFragment)
            fragments.push_back(entity.id);
    for (uint32_t id : fragments)
        PhysicalEntitySystem::instance().remove(id);
    buildHeadlessCollisionChunks(world);
    buildNpcWorldCollision(npcWorld, world);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world.destructible-reset", "match", "authored map solids restored", tick,
        {{"removed_fragments", fragments.size()}});
}

} // namespace MimitaNet
