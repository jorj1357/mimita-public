// 09 14 2026
/* purpose
* Implements the headless generic NPC/monster-like entity lifecycle + health
* self-test. Does NOT own simulation, gameplay policy, or the renderer.
*/
#include "network/npc-entity-selftest.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/actor-health.h"
#include "network/dynamic-replication.h"
#include "network/packets.h"
#include "network/server-context.h"
#include "network/server.h"

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

DynamicComponentRecord healthSchemaDescriptor()
{
    DynamicComponentRecord d;
    d.op = 2;
    d.typeId = gameHash("ActorHealthState");
    d.schemaHash = gameHash("ActorHealthState.v1");
    d.schemaVersion = 1;
    d.networkPolicy = GAME_NET_ALL;
    d.schemaSize = sizeof(ActorHealthStateV1);
    d.schemaAlign = 4;
    std::strncpy(d.schemaName, "ActorHealthState", sizeof(d.schemaName) - 1);
    return d;
}

} // namespace

bool runNpcEntitySelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    EntityRegistry& registry = EntityRegistry::instance();
    store.clear();
    registry.destroyAll();

    // Minimal authoritative server context so the damage capability resolves.
    std::unordered_map<std::uint32_t, ServerPlayer> players;
    std::unordered_map<std::uint32_t, ServerNpc> npcs;
    std::uint32_t nextProjectileId = 1;
    std::uint32_t tick = 1;
    std::uint64_t totalPacketsOut = 0;
    ServerContextV1 context;
    context.players = &players;
    context.npcs = &npcs;
    context.nextProjectileId = &nextProjectileId;
    context.tick = &tick;
    context.totalPacketsOut = &totalPacketsOut;
    setActiveServerContext(&context);

    HotReloadSystem::instance().startup();
    GameplayContextV1* host = LiveBehavior::hostContext(1);
    GameDamageApplyFn applyDamage = host && host->resolveCapability
        ? reinterpret_cast<GameDamageApplyFn>(
              host->resolveCapability(host->host, GAME_CAP_DAMAGE_APPLY))
        : nullptr;
    ok &= check(applyDamage != nullptr, "generic damage.apply capability resolves",
                report);

    // 1–3. Runtime NPC/monster-like entity created after startup + health state.
    const EntityId npc = registry.createGeneric(EntityRealm::Server);
    actorHealthInit(static_cast<std::uint64_t>(npc), 100);
    registry.add<HealthComponent>(npc, HealthComponent{100, 100, false});
    std::int32_t current = 0;
    ok &= check(actorHealthHas(static_cast<std::uint64_t>(npc)) &&
                    actorHealthRead(static_cast<std::uint64_t>(npc), &current, nullptr,
                                    nullptr) &&
                    current == 100,
                "NPC-like entity owns authoritative ActorHealthState", report);

    // 4–5. Generic damage.apply mutates the component, not a typed NPC struct.
    GameDamageApplyV1 req{};
    req.victimEntity = static_cast<std::uint64_t>(npc);
    req.amount = 25;
    req.sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
    if (applyDamage)
        applyDamage(host->host, &req);
    actorHealthRead(static_cast<std::uint64_t>(npc), &current, nullptr, nullptr);
    ok &= check(req.applied && req.healthAfter == 75 && current == 75,
                "damage.apply mutated generic component health", report);

    // 6. Generic component replication carries the health to a client.
    {
        std::vector<DynamicComponentRecord> records;
        std::vector<RelationshipRecord> relations;
        std::vector<EntityLifecycleRecord> lifecycles;
        dynamicReplicationCollectServer(records, relations, lifecycles);
        std::vector<DynamicComponentRecord> outbound;
        outbound.push_back(healthSchemaDescriptor());
        for (const auto& r : records)
            if (r.typeId == gameHash("ActorHealthState"))
                outbound.push_back(r);
        const std::vector<std::uint8_t> bytes =
            dynamicReplicationEncode(outbound, relations, lifecycles);
        std::vector<DynamicComponentRecord> decoded;
        std::vector<RelationshipRecord> decodedRelations;
        std::vector<EntityLifecycleRecord> decodedLifecycles;
        std::string error;
        ok &= check(dynamicReplicationDecode(bytes.data(), bytes.size(), decoded,
                                             decodedRelations, decodedLifecycles, error),
                    "health replication envelope decodes", report);
        store.clear();
        RelationshipStore& clientRelations = RelationshipStore::instance();
        clientRelations.clear();
        ok &= check(dynamicReplicationApply(decoded, decodedRelations, decodedLifecycles,
                                            store, clientRelations, error) &&
                        registry.alive(npc),
                    "client materialized the NPC entity generically", report);
        std::int32_t replicated = 0;
        ok &= check(actorHealthRead(static_cast<std::uint64_t>(npc), &replicated, nullptr,
                                    nullptr) &&
                        replicated == 75,
                    "client received NPC health via generic replication", report);
    }

    // 7–8. Lethal damage -> dead state (generic death fact path).
    GameDamageApplyV1 lethal{};
    lethal.victimEntity = static_cast<std::uint64_t>(npc);
    lethal.amount = 999;
    lethal.sourceKind = GAME_DAMAGE_SOURCE_EXPLOSION;
    if (applyDamage)
        applyDamage(host->host, &lethal);
    bool dead = false;
    actorHealthRead(static_cast<std::uint64_t>(npc), &current, nullptr, &dead);
    ok &= check(lethal.applied && lethal.killed == 1 && dead && current == 0,
                "lethal generic damage yields a dead health component", report);

    // 9. Generic destroy reaches the client; 10. stale update cannot resurrect.
    registry.destroy(npc);
    {
        std::vector<DynamicComponentRecord> records;
        std::vector<RelationshipRecord> relations;
        std::vector<EntityLifecycleRecord> lifecycles;
        dynamicReplicationCollectServer(records, relations, lifecycles);
        std::vector<DynamicComponentRecord> outbound;
        outbound.push_back(healthSchemaDescriptor());
        for (const auto& r : records)
            if (r.typeId == gameHash("ActorHealthState"))
                outbound.push_back(r);
        bool hasDestroy = false;
        for (const auto& l : lifecycles)
            if (l.op == 1 && l.entity == static_cast<std::uint64_t>(npc))
                hasDestroy = true;
        ok &= check(hasDestroy, "generic DESTROY is emitted for the NPC entity", report);
        const std::vector<std::uint8_t> bytes =
            dynamicReplicationEncode(outbound, relations, lifecycles);
        std::vector<DynamicComponentRecord> decoded;
        std::vector<RelationshipRecord> decodedRelations;
        std::vector<EntityLifecycleRecord> decodedLifecycles;
        std::string error;
        dynamicReplicationDecode(bytes.data(), bytes.size(), decoded, decodedRelations,
                                 decodedLifecycles, error);
        RelationshipStore& clientRelations = RelationshipStore::instance();
        dynamicReplicationApply(decoded, decodedRelations, decodedLifecycles, store,
                                clientRelations, error);
        ok &= check(!registry.alive(npc), "client removed the NPC entity", report);

        // Stale update after destroy must not resurrect the entity state.
        DynamicComponentRecord late;
        late.op = 0;
        late.entity = static_cast<std::uint64_t>(npc);
        late.typeId = gameHash("ActorHealthState");
        late.schemaHash = gameHash("ActorHealthState.v1");
        late.schemaVersion = 1;
        ActorHealthStateV1 revived{100, 100, 0, 0};
        late.payload.assign(reinterpret_cast<const std::uint8_t*>(&revived),
                            reinterpret_cast<const std::uint8_t*>(&revived) +
                                sizeof(revived));
        dynamicReplicationApply({late}, {}, {}, store, clientRelations, error);
        ok &= check(!store.has(npc, gameHash("ActorHealthState")),
                    "stale update after DESTROY cannot resurrect the NPC", report);
    }

    // 8. A second runtime monster-like entity uses the same path (no MonsterType).
    {
        const EntityId monster = registry.createGeneric(EntityRealm::Server);
        actorHealthInit(static_cast<std::uint64_t>(monster), 60);
        GameDamageApplyV1 hit{};
        hit.victimEntity = static_cast<std::uint64_t>(monster);
        hit.amount = 20;
        hit.sourceKind = GAME_DAMAGE_SOURCE_MELEE;
        if (applyDamage)
            applyDamage(host->host, &hit);
        std::int32_t hp = 0;
        actorHealthRead(static_cast<std::uint64_t>(monster), &hp, nullptr, nullptr);
        ok &= check(hit.applied && hp == 40,
                    "runtime monster-like entity uses generic health/damage", report);
    }

    store.clear();
    registry.destroyAll();
    setActiveServerContext(nullptr);
    GenericRuntime::instance().deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
