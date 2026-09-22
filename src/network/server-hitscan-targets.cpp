// 09 22 2026
/* purpose
* Implements the rewound hitscan hitbox bridge (see the header).
* Does NOT own weapon definitions, damage, ammo, or networking.
*/
#include "network/server-hitscan-targets.h"

#include <cstddef>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/game-api.h"
#include "hot-reload/hot-hitscan-target.h"

namespace MimitaNet {

namespace {

void ensureSchema()
{
    // Always (re)register: a store clear must not leave the schema missing.
    MimitaRuntime::DynamicComponentSchema schema;
    schema.typeId = hotHitscanTargetComponentId();
    schema.schemaHash = hotHitscanTargetSchemaHash();
    schema.version = HOT_HITSCAN_TARGET_VERSION;
    schema.size = sizeof(HotHitscanTargetV1);
    schema.align = 4;
    schema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
    schema.name = "HitscanTargetBoxes";
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(schema);
}

} // namespace

void publishHitscanTargets(
    const std::vector<std::uint64_t>& entities,
    const std::vector<WeaponExecution::PlayerTarget>& targets)
{
    ensureSchema();
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    const std::size_t count = std::min(entities.size(), targets.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        if (entities[i] == 0)
            continue;
        HotHitscanTargetV1 component{};
        component.version = HOT_HITSCAN_TARGET_VERSION;
        component.spawnGeneration = targets[i].spawnGeneration;
        std::uint32_t partCount = 0;
        for (const auto& part : targets[i].bodyParts)
        {
            if (partCount >= (std::uint32_t)HOT_HITSCAN_MAX_PARTS)
                break;
            HotHitscanPartV1& dst = component.parts[partCount++];
            dst.center[0] = part.center.x;
            dst.center[1] = part.center.y;
            dst.center[2] = part.center.z;
            dst.half[0] = part.half.x;
            dst.half[1] = part.half.y;
            dst.half[2] = part.half.z;
            dst.bodyPart = (std::uint32_t)part.bodyPart;
        }
        component.partCount = partCount;
        store.write(static_cast<EntityId>(entities[i]),
                    hotHitscanTargetComponentId(), &component, sizeof(component));
    }
}

void clearHitscanTargets(const std::vector<std::uint64_t>& entities)
{
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    const std::uint64_t id = hotHitscanTargetComponentId();
    for (std::uint64_t entity : entities)
    {
        if (entity != 0)
            store.remove(static_cast<EntityId>(entity), id);
    }
}

} // namespace MimitaNet
