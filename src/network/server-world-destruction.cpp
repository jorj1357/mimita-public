// 2026-10-30
/* purpose
* Resolve shared world impacts against imported closed GLB solids. A struck
* solid becomes one static PhysicalEntity only when first hit, so the existing
* Boolean, fragment, and replication owners handle subsequent cuts.
* The headless map retains its authored triangles for round restoration.
* All sources feed this one persistent entry point; no weapon-specific wall
* damage path exists.
*/

#include "network/server.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "impact/destructible-world-config.h"
#include "config/material-config.h"
#include "debug/structured-log.h"
#include "gamemode/gamemode.h"
#include "impact/destructible-mesh-loader.h"
#include "impact/impact-system.h"
#include "network/server-gamemode.h"
#include "physics/physical-entity.h"

namespace MimitaNet {
namespace {

uint32_t nearestSolid(const HeadlessWorld& world,
                      const MimitaImpact::ImpactEvent& impact)
{
    if (impact.worldSolidId != 0)
        return impact.worldSolidId;

    if (impact.targetEntityId != 0 &&
        impact.targetEntityId <= world.solidIds.size())
    {
        uint32_t id = world.solidIds[impact.targetEntityId - 1];
        if (id == kRemovedCollisionSolidId &&
            impact.targetEntityId <= world.destructionBaselineSolidIds.size())
            id = world.destructionBaselineSolidIds[impact.targetEntityId - 1];
        if (id != 0 && id != kRemovedCollisionSolidId)
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
        if (id == kRemovedCollisionSolidId &&
            (size_t)index < world.destructionBaselineSolidIds.size())
            id = world.destructionBaselineSolidIds[index];
        if (id == 0 || id == kRemovedCollisionSolidId)
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

struct ExplosionCandidate
{
    const HeadlessGLBObject* object = nullptr;
    float distance = 0.0f;
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
};

// Impacts that could not activate their solid within this tick's activation
// budget. They retain their original order and are merged first next tick.
std::unordered_map<const HeadlessWorld*, std::vector<MimitaImpact::ImpactEvent>>
    gDeferredWorldImpacts;

} // namespace

uint32_t serverWorldSolidIdAtTriangle(const HeadlessWorld& world, size_t index)
{
    if (index >= world.solidIds.size())
        return 0;
    uint32_t id = world.solidIds[index];
    if (id == kRemovedCollisionSolidId && index < world.destructionBaselineSolidIds.size())
        id = world.destructionBaselineSolidIds[index];
    return id == kRemovedCollisionSolidId ? 0u : id;
}

void submitExplosionWorldImpacts(const HeadlessWorld& world,
                                 const glm::vec3& center, float radius,
                                 float energyJ, float sourceMassKg,
                                 float sourceRadiusM, float penetrationM,
                                 uint32_t sourceEntityId, uint32_t tick)
{
    if (radius <= 0.0f || energyJ <= 0.0f || sourceMassKg <= 0.0f)
        return;

    std::vector<ExplosionCandidate> candidates;
    candidates.reserve(world.glbObjects.size());
    for (const HeadlessGLBObject& object : world.glbObjects)
    {
        if (object.solidId == 0 || object.nodeIndex < 0)
            continue;
        if (world.rejectedDestructibleNodes.count(object.nodeIndex) != 0)
            continue;
        const glm::vec3 point =
            glm::clamp(center, object.boundsMin, object.boundsMax);
        const glm::vec3 delta = point - center;
        const float distance = glm::length(delta);
        if (distance > radius)
            continue;
        glm::vec3 normal = distance > 1e-4f ? delta / distance
                                            : glm::vec3(0.0f, 0.0f, 1.0f);
        if (distance <= 1e-4f)
        {
            const glm::vec3 mid = (object.boundsMin + object.boundsMax) * 0.5f;
            normal = mid - center;
            if (glm::length(normal) < 1e-4f)
                normal = glm::vec3(0.0f, 0.0f, 1.0f);
            normal = glm::normalize(normal);
        }
        candidates.push_back({&object, distance, point, normal});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const ExplosionCandidate& a, const ExplosionCandidate& b) {
                  return a.distance < b.distance;
              });
    const uint32_t maxNodes =
        MimitaImpact::DestructibleWorldConfig::instance().explosionMaxNodesPerEvent();
    const bool capped = candidates.size() > maxNodes;
    if (capped)
        candidates.resize(maxNodes);

    uint32_t submitted = 0;
    uint32_t occluded = 0;
    for (const ExplosionCandidate& candidate : candidates)
    {
        if (!candidate.object)
            continue;
        if (candidate.distance > 0.05f)
        {
            glm::vec3 hitPos;
            glm::vec3 hitNormal;
            uint32_t hitEntityId = 0;
            uint32_t hitSolidId = 0;
            if (serverRaycastWorld(center, candidate.normal, candidate.distance,
                                   world, hitPos, hitNormal, &hitEntityId,
                                   &hitSolidId))
            {
                const float hitDistance = glm::length(hitPos - center);
                if (hitEntityId == 0 &&
                    hitSolidId != candidate.object->solidId &&
                    hitDistance < candidate.distance - 0.05f)
                {
                    ++occluded;
                    continue;
                }
            }
        }

        const float falloff =
            glm::clamp(1.0f - candidate.distance / radius, 0.0f, 1.0f);
        const float impactEnergy = energyJ * falloff * falloff;
        if (impactEnergy <= 1e-4f)
            continue;
        const float speed =
            std::sqrt(2.0f * impactEnergy / std::max(sourceMassKg, 1e-6f));

        uint32_t activeEntityId = 0;
        const auto active =
            world.activeDestructibleNodes.find(candidate.object->nodeIndex);
        if (active != world.activeDestructibleNodes.end())
            activeEntityId = active->second;

        MimitaImpact::ImpactEvent impact;
        impact.simulationTick = tick;
        impact.source = MimitaImpact::ImpactSource::Explosion;
        impact.target = activeEntityId != 0
            ? MimitaImpact::ImpactTarget::PhysicalEntity
            : MimitaImpact::ImpactTarget::WorldGeometry;
        impact.sourceEntityId = sourceEntityId;
        impact.eventId = (uint64_t(sourceEntityId) << 32) ^
            (uint64_t(tick) * 1000003ull + candidate.object->solidId);
        impact.targetEntityId = activeEntityId;
        impact.worldSolidId = candidate.object->solidId;
        impact.worldPoint = candidate.point;
        impact.worldNormal = candidate.normal;
        impact.worldDirection = candidate.normal;
        impact.speed = speed;
        impact.mass = sourceMassKg;
        impact.radius = sourceRadiusM;
        impact.boreLength = penetrationM;
        MimitaImpact::ImpactSystem::instance().submit(impact);
        ++submitted;
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world.destructible-explosion", std::to_string(sourceEntityId),
        "radial explosion submitted map-solid impacts", tick,
        nlohmann::json{
            {"source_entity", sourceEntityId},
            {"radius", radius},
            {"energy_j", energyJ},
            {"source_mass_kg", sourceMassKg},
            {"candidates", candidates.size()},
            {"submitted", submitted},
            {"occluded", occluded},
            {"capped", capped},
            {"max_nodes", maxNodes},
            {"center", {center.x, center.y, center.z}}});
}

void flushServerWorldImpacts(HeadlessWorld& world, World& npcWorld,
                             const std::string& mapId, uint32_t tick)
{
    // The dedicated server does not run the client's config-polling pass. Load
    // and hot-reload the destruction switch here so global JSON changes take
    // effect without a restart.
    MimitaImpact::DestructibleWorldConfig& destructionConfig =
        MimitaImpact::DestructibleWorldConfig::instance();
    if (destructionConfig.revision() == 0)
        destructionConfig.load();
    else
        destructionConfig.pollReload();
    const bool globalEnabled = destructionConfig.worldEnabled();
    const Gamemode& gamemode =
        GamemodeRegistry::instance().get(serverGamemodeState().matchMode);
    const bool enabled = gamemode.features.destructibleWorld < 0
        ? globalEnabled
        : gamemode.features.destructibleWorld != 0;
    if (!enabled)
    {
        MimitaImpact::ImpactSystem::instance().takeWorldImpacts();
        return;
    }

    std::vector<MimitaImpact::ImpactEvent> impacts =
        MimitaImpact::ImpactSystem::instance().takeWorldImpacts();
    auto& deferred = gDeferredWorldImpacts[&world];
    if (!impacts.empty() && !deferred.empty())
    {
        impacts.insert(impacts.begin(),
            std::make_move_iterator(deferred.begin()),
            std::make_move_iterator(deferred.end()));
        deferred.clear();
    }
    else if (impacts.empty() && !deferred.empty())
    {
        impacts.swap(deferred);
        deferred.clear();
    }
    if (impacts.empty())
        return;

    const auto flushStart = std::chrono::steady_clock::now();
    const size_t impactCount = impacts.size();
    const uint32_t maxActivations =
        MimitaImpact::DestructibleWorldConfig::instance().maxActivationsPerFlush();
    uint32_t activated = 0;
    uint32_t submitted = 0;

    if (MimitaImpact::MaterialConfig::instance().revision() == 0)
        MimitaImpact::MaterialConfig::instance().load();

    bool collisionChanged = false;
    for (size_t impactIndex = 0; impactIndex < impacts.size(); ++impactIndex)
    {
        MimitaImpact::ImpactEvent& impact = impacts[impactIndex];
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
        if (world.rejectedDestructibleNodes.count(object->nodeIndex) != 0)
            continue;

        uint32_t entityId = 0;
        const auto active = world.activeDestructibleNodes.find(object->nodeIndex);
        if (active != world.activeDestructibleNodes.end())
            entityId = active->second;
        else
        {
            if (activated >= maxActivations)
            {
                deferred.insert(deferred.end(),
                    std::make_move_iterator(impacts.begin() + impactIndex),
                    std::make_move_iterator(impacts.end()));
                break;
            }
            const auto loadStart = std::chrono::steady_clock::now();
            const std::string model = "assets/maps/" + mapId + ".glb#node:" +
                                      std::to_string(object->nodeIndex);
            MimitaImpact::DestructibleMeshLoad mesh =
                MimitaImpact::loadDestructibleMeshFromGLB(model);
            const double loadMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - loadStart).count();
            if (!mesh.success)
            {
                world.rejectedDestructibleNodes.insert(object->nodeIndex);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "world.destructible-rejected", object->id,
                    mesh.error, tick,
                    {{"map", mapId}, {"node", object->nodeIndex},
                     {"load_ms", loadMs}});
                continue;
            }
            const bool floor = mesh.halfExtents.x >= 1.0f &&
                               mesh.halfExtents.y >= 1.0f &&
                               mesh.halfExtents.z * 4.0f <
                                   std::min(mesh.halfExtents.x, mesh.halfExtents.y);
            const std::string materialName = object->materialName.empty()
                ? (floor ? "foundation" : "concrete")
                : object->materialName;
            const uint32_t materialId =
                MimitaImpact::materialIdForName(materialName);
            const auto initStart = std::chrono::steady_clock::now();
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
            const double initMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - initStart).count();
            if (world.destructionBaselineSolidIds.empty())
                world.destructionBaselineSolidIds = world.solidIds;
            world.activeDestructibleNodes.emplace(object->nodeIndex, entityId);
            for (uint32_t& id : world.solidIds)
                if (id == solidId)
                    id = UINT32_MAX;
            collisionChanged = true;
            ++activated;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "world.destructible-activated", object->id,
                "closed map solid moved to shared destruction owner", tick,
                {{"map", mapId}, {"node", object->nodeIndex},
                 {"solid", solidId}, {"entity", entityId},
                 {"material", materialName},
                 {"mesh_tris", entity->localTriangles.size()},
                 {"load_ms", loadMs}, {"init_ms", initMs}});
        }
        impact.target = MimitaImpact::ImpactTarget::PhysicalEntity;
        impact.targetEntityId = entityId;
        impact.worldSolidId = solidId;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Verbose,
            "world.destructible-impact", std::to_string(impact.sourceEntityId),
            "world impact accepted by map destruction owner", tick,
            {{"source", (int)impact.source},
             {"source_entity", impact.sourceEntityId},
             {"target_entity", entityId},
             {"solid", solidId},
             {"mass_kg", impact.mass},
             {"density_kg_m3", impact.density},
             {"speed_mps", impact.speed},
             {"radius_m", impact.radius},
             {"bore_m", impact.boreLength}});
        MimitaImpact::ImpactSystem::instance().submit(impact);
        ++submitted;
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
    // Hidden map triangles stay in the broadphase but are skipped by the
    // query paths, so activation never pays a full headless collision rebuild.
    // The NPC snapshot is separate and much more expensive; rebuild it only
    // when the mode/config explicitly asks for immediate NPC navigation.
    double collisionMs = 0.0;
    double npcMs = 0.0;
    if ((collisionChanged || revision != world.lastNpcDestructionRevision) &&
        MimitaImpact::DestructibleWorldConfig::instance()
            .npcCollisionRebuildOnWorldCut())
    {
        const auto npcStart = std::chrono::steady_clock::now();
        buildNpcWorldCollision(npcWorld, world);
        npcMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - npcStart).count();
        world.lastNpcDestructionRevision = revision;
    }
    else if (revision != world.lastNpcDestructionRevision)
    {
        world.lastNpcDestructionRevision = revision;
    }
    const double totalMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - flushStart).count();
    if (activated > 0 || submitted > 0 || totalMs > 1.0)
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Verbose,
            "world.destructible-flush", "map",
            "world impact flush completed", tick,
            {{"impacts", impactCount},
             {"activated", activated},
             {"submitted", submitted},
             {"collision_ms", collisionMs},
             {"npc_collision_ms", npcMs},
             {"total_ms", totalMs},
             {"deferred", deferred.size()},
             {"max_activations", maxActivations},
             {"active_nodes", world.activeDestructibleNodes.size()}});
    }
}

void clearServerWorldDestruction(HeadlessWorld& world)
{
    gDeferredWorldImpacts.erase(&world);
    for (const auto& item : world.activeDestructibleNodes)
        PhysicalEntitySystem::instance().remove(item.second);
    world.activeDestructibleNodes.clear();
    world.destructionBaselineSolidIds.clear();
    world.rejectedDestructibleNodes.clear();
    world.lastDestructionMatchId = 0;
    world.lastNpcDestructionRevision = 0;
    std::vector<uint32_t> fragments;
    for (const PhysicalEntity& entity : PhysicalEntitySystem::instance().entities())
        if (entity.isFragment)
            fragments.push_back(entity.id);
    for (uint32_t id : fragments)
        PhysicalEntitySystem::instance().remove(id);
}

void resetServerWorldDestruction(HeadlessWorld& world, World& npcWorld,
                                 uint32_t tick)
{
    gDeferredWorldImpacts.erase(&world);
    if (world.destructionBaselineSolidIds.empty())
        return;
    world.solidIds = world.destructionBaselineSolidIds;
    size_t removedNodes = 0;
    for (const auto& item : world.activeDestructibleNodes)
    {
        if (PhysicalEntitySystem::instance().remove(item.second))
            ++removedNodes;
    }
    world.activeDestructibleNodes.clear();
    world.rejectedDestructibleNodes.clear();
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
        {{"removed_nodes", removedNodes},
         {"removed_fragments", fragments.size()}});
}

} // namespace MimitaNet
