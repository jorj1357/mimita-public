// 09 14 2026
/* purpose
* Implements the headless generic actor team/role/profile/target self-test.
* Does NOT own simulation, gameplay policy, or the renderer.
*/
#include "network/npc-actor-state-selftest.h"

#include <cstring>
#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "network/actor-state.h"
#include "network/dynamic-replication.h"

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

DynamicComponentRecord schemaDescriptor(std::uint64_t typeId, std::uint64_t hash,
                                        std::uint32_t size, std::uint32_t align,
                                        const char* name)
{
    DynamicComponentRecord d;
    d.op = 2;
    d.typeId = typeId;
    d.schemaHash = hash;
    d.schemaVersion = 1;
    d.networkPolicy = GAME_NET_ALL;
    d.schemaSize = size;
    d.schemaAlign = align;
    std::strncpy(d.schemaName, name, sizeof(d.schemaName) - 1);
    return d;
}

} // namespace

bool runNpcActorStateSelfTest(std::string& report)
{
    bool ok = true;
    DynamicComponentStore& store = DynamicComponentStore::instance();
    EntityRegistry& registry = EntityRegistry::instance();
    RelationshipStore& relations = RelationshipStore::instance();
    store.clear();
    relations.clear();
    registry.destroyAll();
    actorStateEnsureSchemas();

    const EntityId npc = registry.createGeneric(EntityRealm::Server);
    ok &= check(actorStateWriteTeam(static_cast<std::uint64_t>(npc), 1) &&
                    actorStateWriteRole(static_cast<std::uint64_t>(npc), "sniper") &&
                    actorStateWriteProfile(static_cast<std::uint64_t>(npc), "sprint",
                                           "aggressive"),
                "generic actor team/role/profile written", report);

    std::int32_t team = -9;
    std::uint64_t roleHash = 0, moveHash = 0, behaviorHash = 0;
    actorStateReadTeam(static_cast<std::uint64_t>(npc), &team);
    actorStateReadRoleHash(static_cast<std::uint64_t>(npc), &roleHash);
    actorStateReadProfile(static_cast<std::uint64_t>(npc), &moveHash, &behaviorHash);
    ok &= check(team == 1 && roleHash == gameHash("sniper") &&
                    moveHash == gameHash("sprint") &&
                    behaviorHash == gameHash("aggressive") &&
                    std::string(actorStateRoleIdForHash(roleHash)) == "sniper",
                "generic team/role/profile read back authoritatively", report);

    const EntityId target = registry.createGeneric(EntityRealm::Server);
    ok &= check(actorStateSetTarget(static_cast<std::uint64_t>(npc),
                                    static_cast<std::uint64_t>(target)),
                "generic target relationship written", report);
    std::uint64_t resolvedTarget = 0;
    ok &= check(actorStateGetTarget(static_cast<std::uint64_t>(npc), &resolvedTarget) &&
                    resolvedTarget == static_cast<std::uint64_t>(target),
                "generic target relationship resolves to an EntityId", report);

    // Generic replication: schema descriptors + component upserts + CREATE +
    // target edge.
    {
        std::vector<DynamicComponentRecord> records;
        std::vector<RelationshipRecord> relRecords;
        std::vector<EntityLifecycleRecord> lifecycles;
        dynamicReplicationCollectServer(records, relRecords, lifecycles);
        std::vector<DynamicComponentRecord> outbound;
        outbound.push_back(schemaDescriptor(gameHash("ActorTeamState"),
                                            gameHash("ActorTeamState.v1"),
                                            sizeof(ActorTeamStateV1), 4,
                                            "ActorTeamState"));
        outbound.push_back(schemaDescriptor(gameHash("ActorRoleState"),
                                            gameHash("ActorRoleState.v1"),
                                            sizeof(ActorRoleStateV1), 8,
                                            "ActorRoleState"));
        outbound.push_back(schemaDescriptor(gameHash("ActorProfileState"),
                                            gameHash("ActorProfileState.v1"),
                                            sizeof(ActorProfileStateV1), 8,
                                            "ActorProfileState"));
        for (const auto& r : records)
            if (r.typeId == gameHash("ActorTeamState") ||
                r.typeId == gameHash("ActorRoleState") ||
                r.typeId == gameHash("ActorProfileState"))
                outbound.push_back(r);

        const std::vector<std::uint8_t> bytes =
            dynamicReplicationEncode(outbound, relRecords, lifecycles);
        std::vector<DynamicComponentRecord> decoded;
        std::vector<RelationshipRecord> decodedRelations;
        std::vector<EntityLifecycleRecord> decodedLifecycles;
        std::string error;
        ok &= check(dynamicReplicationDecode(bytes.data(), bytes.size(), decoded,
                                             decodedRelations, decodedLifecycles,
                                             error),
                    "actor state replication envelope decodes", report);
        store.clear();
        relations.clear();
        ok &= check(dynamicReplicationApply(decoded, decodedRelations, decodedLifecycles,
                                            store, relations, error) &&
                        registry.alive(npc),
                    "client materialized the actor generically", report);
        std::int32_t clientTeam = -9;
        std::uint64_t clientRole = 0, clientBehavior = 0;
        actorStateReadTeam(static_cast<std::uint64_t>(npc), &clientTeam);
        actorStateReadRoleHash(static_cast<std::uint64_t>(npc), &clientRole);
        actorStateReadProfile(static_cast<std::uint64_t>(npc), nullptr, &clientBehavior);
        std::uint64_t clientTarget = 0;
        ok &= check(clientTeam == 1 && clientRole == gameHash("sniper") &&
                        clientBehavior == gameHash("aggressive") &&
                        actorStateGetTarget(static_cast<std::uint64_t>(npc),
                                            &clientTarget) &&
                        clientTarget == static_cast<std::uint64_t>(target),
                    "client received team/role/profile + target generically", report);
    }

    // Hot AI consumer reads the generic state.
    {
        HotReloadSystem::instance().startup();
        GenericRuntime& runtime = GenericRuntime::instance();
        runtime.runDomain(GAME_DOMAIN_GAMEPLAY, 1, 1.0f / 60.0f,
                          LiveBehavior::hostContext(1));
        ok &= check(store.has(npc, gameHash("NpcAiSelection")),
                    "hot AI system derived state from the generic actor components",
                    report);
        runtime.deactivate();
        HotReloadSystem::instance().unloadGameDLL();
    }

    // Stale update after destroy cannot resurrect.
    {
        registry.destroy(npc);
        std::vector<DynamicComponentRecord> records;
        std::vector<RelationshipRecord> relRecords;
        std::vector<EntityLifecycleRecord> lifecycles;
        dynamicReplicationCollectServer(records, relRecords, lifecycles);
        std::vector<DynamicComponentRecord> outbound;
        outbound.push_back(schemaDescriptor(gameHash("ActorTeamState"),
                                            gameHash("ActorTeamState.v1"),
                                            sizeof(ActorTeamStateV1), 4,
                                            "ActorTeamState"));
        const std::vector<std::uint8_t> bytes =
            dynamicReplicationEncode(outbound, relRecords, lifecycles);
        std::vector<DynamicComponentRecord> decoded;
        std::vector<RelationshipRecord> decodedRelations;
        std::vector<EntityLifecycleRecord> decodedLifecycles;
        std::string error;
        dynamicReplicationDecode(bytes.data(), bytes.size(), decoded, decodedRelations,
                                 decodedLifecycles, error);
        dynamicReplicationApply(decoded, decodedRelations, decodedLifecycles, store,
                                relations, error);
        ok &= check(!registry.alive(npc), "client removed the actor entity", report);

        DynamicComponentRecord late;
        late.op = 0;
        late.entity = static_cast<std::uint64_t>(npc);
        late.typeId = gameHash("ActorTeamState");
        late.schemaHash = gameHash("ActorTeamState.v1");
        late.schemaVersion = 1;
        ActorTeamStateV1 revived{2, 0};
        late.payload.assign(reinterpret_cast<const std::uint8_t*>(&revived),
                            reinterpret_cast<const std::uint8_t*>(&revived) +
                                sizeof(revived));
        dynamicReplicationApply({late}, {}, {}, store, relations, error);
        ok &= check(!store.has(npc, gameHash("ActorTeamState")),
                    "stale actor state cannot resurrect a destroyed entity", report);
    }

    // Runtime monster-like composition: same generic path, no MonsterType.
    {
        const EntityId monster = registry.createGeneric(EntityRealm::Server);
        actorStateWriteTeam(static_cast<std::uint64_t>(monster), 0);
        actorStateWriteRole(static_cast<std::uint64_t>(monster), "juggernaut");
        actorStateWriteProfile(static_cast<std::uint64_t>(monster), "", "aggressive");
        std::uint64_t mRole = 0, mBehavior = 0;
        actorStateReadRoleHash(static_cast<std::uint64_t>(monster), &mRole);
        actorStateReadProfile(static_cast<std::uint64_t>(monster), nullptr, &mBehavior);
        ok &= check(mRole == gameHash("juggernaut") &&
                        mBehavior == gameHash("aggressive"),
                    "runtime monster-like entity composes generic actor state", report);
    }

    store.clear();
    relations.clear();
    registry.destroyAll();
    return ok;
}
