// 09 14 2026
/* purpose
* Implements the headless generic dynamic-component replication self-test.
* Does NOT own simulation, gameplay policy, or the dynamic store.
*/
#include "network/dynamic-replication-selftest.h"

#include <cstring>
#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"
#include "hot-reload/hot-projectile.h"
#include "network/dynamic-replication.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

const std::uint64_t kRep = gameHash("RepTestState");
const std::uint64_t kRepHashV1 = gameHash("RepTestState.v1");
const EntityId kRepEntity = 100;
const EntityId kProjectileEntity = 200;

struct RepTestState {
    std::int32_t value;
    std::int32_t counter;
};

struct RepTestStateV2 {
    std::int32_t value;
    std::int32_t counter;
    std::int32_t extra;
};

bool migrateRepV1toV2(const void* oldState, std::size_t oldSize, void* newState,
                      std::size_t newSize)
{
    if (oldSize < sizeof(RepTestState) || newSize < sizeof(RepTestStateV2))
        return false;
    RepTestState old{};
    std::memcpy(&old, oldState, sizeof(RepTestState));
    RepTestStateV2 next{};
    next.value = old.value;
    next.counter = old.counter;
    next.extra = 42;
    std::memcpy(newState, &next, sizeof(RepTestStateV2));
    return true;
}

MimitaNet::DynamicComponentRecord schemaDescriptor(std::uint64_t typeId,
                                                   std::uint64_t hash,
                                                   std::uint32_t version,
                                                   std::uint32_t size,
                                                   const char* name)
{
    MimitaNet::DynamicComponentRecord d;
    d.op = 2;
    d.typeId = typeId;
    d.schemaHash = hash;
    d.schemaVersion = version;
    d.networkPolicy = GAME_NET_ALL;
    d.schemaSize = size;
    d.schemaAlign = 4;
    std::strncpy(d.schemaName, name, sizeof(d.schemaName) - 1);
    return d;
}

} // namespace

bool runDynamicReplicationSelfTest(std::string& report)
{
    bool ok = true;
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    store.clear();

    // Server: register replicated schemas and attach state. One is a brand-new
    // test component, the other is a real gameplay component.
    MimitaRuntime::DynamicComponentSchema rep;
    rep.typeId = kRep;
    rep.schemaHash = kRepHashV1;
    rep.version = 1;
    rep.size = sizeof(RepTestState);
    rep.align = 4;
    rep.networkPolicy = GAME_NET_ALL;
    rep.name = "RepTestState";
    store.registerSchema(rep);

    MimitaRuntime::DynamicComponentSchema hot;
    hot.typeId = HOT_PROJECTILE_COMPONENT;
    hot.schemaHash = gameHash("HotProjectileState.v1");
    hot.version = 1;
    hot.size = sizeof(HotProjectileStateV1);
    hot.align = 8;
    hot.networkPolicy = GAME_NET_ALL;
    hot.name = "HotProjectileState";
    store.registerSchema(hot);

    RepTestState server{7, 1};
    store.write(kRepEntity, kRep, &server, sizeof(server));
    HotProjectileStateV1 projectile{};
    projectile.velocity[0] = 3.0f;
    projectile.lifetime = 2.0f;
    store.write(kProjectileEntity, HOT_PROJECTILE_COMPONENT, &projectile,
                sizeof(projectile));

    // A relationship type unknown to the EXE at startup, created server-side.
    const std::uint64_t kRepRelation = gameHash("relationship.rep-test");
    const EntityId kRepRelationTarget = 101;
    MimitaRuntime::RelationshipStore& serverRelations =
        MimitaRuntime::RelationshipStore::instance();
    serverRelations.clear();
    serverRelations.add(kRepRelation, kRepEntity, kRepRelationTarget, 7u);

    std::vector<MimitaNet::DynamicComponentRecord> serverRecords;
    std::vector<MimitaNet::RelationshipRecord> serverRelationRecords;
    std::vector<MimitaNet::EntityLifecycleRecord> serverLifecycles;
    MimitaNet::dynamicReplicationCollectServer(serverRecords, serverRelationRecords,
                                               serverLifecycles);
    bool hasRep = false, hasHot = false;
    for (const auto& r : serverRecords) {
        if (r.typeId == kRep) hasRep = true;
        if (r.typeId == HOT_PROJECTILE_COMPONENT) hasHot = true;
    }
    ok &= check(hasRep, "new runtime component is collected for replication", report);
    ok &= check(hasHot,
                "real gameplay component (HotProjectileState) uses the generic path",
                report);
    bool hasNewRelation = false;
    for (const auto& r : serverRelationRecords)
        if (r.typeId == kRepRelation) hasNewRelation = true;
    ok &= check(hasNewRelation,
                "new runtime relationship type is collected without registration",
                report);

    std::vector<MimitaNet::DynamicComponentRecord> outbound;
    outbound.push_back(schemaDescriptor(kRep, kRepHashV1, 1, sizeof(RepTestState),
                                        "RepTestState"));
    outbound.push_back(schemaDescriptor(HOT_PROJECTILE_COMPONENT,
                                        gameHash("HotProjectileState.v1"), 1,
                                        sizeof(HotProjectileStateV1),
                                        "HotProjectileState"));
    for (const auto& r : serverRecords)
        if (r.op == 0)
            outbound.push_back(r);

    const std::vector<std::uint8_t> bytes =
        MimitaNet::dynamicReplicationEncode(outbound, serverRelationRecords,
                                            serverLifecycles);
    std::vector<MimitaNet::DynamicComponentRecord> decoded;
    std::vector<MimitaNet::RelationshipRecord> decodedRelations;
    std::vector<MimitaNet::EntityLifecycleRecord> decodedLifecycles;
    std::string error;
    ok &= check(MimitaNet::dynamicReplicationDecode(
                    bytes.data(), bytes.size(), decoded, decodedRelations,
                    decodedLifecycles, error),
                "generic envelope decodes", report);
    ok &= check(decoded.size() == outbound.size() &&
                    decodedRelations.size() == serverRelationRecords.size() &&
                    decodedLifecycles.size() == serverLifecycles.size(),
                "record count round-trips", report);

    // Simulate a fresh client: clear and apply the received records.
    store.clear();
    MimitaRuntime::RelationshipStore& clientRelations =
        MimitaRuntime::RelationshipStore::instance();
    clientRelations.clear();
    ok &= check(MimitaNet::dynamicReplicationApply(decoded, decodedRelations,
                                                   decodedLifecycles, store,
                                                   clientRelations, error),
                "client applies replicated records: " + error, report);
    ok &= check(decodedLifecycles.size() >= 2 &&
                    EntityRegistry::instance().alive(static_cast<EntityId>(kRepEntity)),
                "client learned the entities' generic lifecycle (CREATE)", report);
    ok &= check(clientRelations.has(kRepRelation, kRepEntity, kRepRelationTarget),
                "client received the new relationship edge", report);
    RepTestState received{};
    ok &= check(store.schema(kRep) &&
                    store.read(kRepEntity, kRep, &received, sizeof(received)) &&
                    received.value == 7 && received.counter == 1,
                "client received the new component's schema + state", report);
    HotProjectileStateV1 receivedProjectile{};
    ok &= check(store.read(kProjectileEntity, HOT_PROJECTILE_COMPONENT,
                           &receivedProjectile, sizeof(receivedProjectile)),
                "client received the real gameplay component state", report);

    // Update propagates.
    RepTestState updated{9, 2};
    std::vector<MimitaNet::DynamicComponentRecord> update;
    {
        MimitaNet::DynamicComponentRecord r;
        r.op = 0;
        r.entity = kRepEntity;
        r.typeId = kRep;
        r.schemaHash = kRepHashV1;
        r.schemaVersion = 1;
        r.changeVersion = 5;
        r.payload.assign(reinterpret_cast<const std::uint8_t*>(&updated),
                         reinterpret_cast<const std::uint8_t*>(&updated) +
                             sizeof(updated));
        update.push_back(r);
    }
    MimitaNet::dynamicReplicationApply(update, {}, {}, store, clientRelations, error);
    store.read(kRepEntity, kRep, &received, sizeof(received));
    ok &= check(received.value == 9 && received.counter == 2,
                "component update propagates", report);

    // Remove propagates.
    std::vector<MimitaNet::DynamicComponentRecord> removal;
    {
        MimitaNet::DynamicComponentRecord r;
        r.op = 1;
        r.entity = kRepEntity;
        r.typeId = kRep;
        removal.push_back(r);
    }
    ok &= check(MimitaNet::dynamicReplicationApply(removal, {}, {}, store,
                                                   clientRelations, error) &&
                    !store.has(kRepEntity, kRep),
                "component remove propagates", report);

    // Re-add works.
    store.write(kRepEntity, kRep, &server, sizeof(server));
    ok &= check(store.has(kRepEntity, kRep), "component re-add works", report);

    // Relationship update and remove propagate through the same envelope.
    {
        std::vector<MimitaNet::RelationshipRecord> relUpdate;
        MimitaNet::RelationshipRecord r;
        r.op = 3;
        r.source = kRepEntity;
        r.target = kRepRelationTarget;
        r.typeId = kRepRelation;
        r.value = 11u;
        r.changeVersion = 9u;
        relUpdate.push_back(r);
        MimitaNet::dynamicReplicationApply({}, relUpdate, {}, store, clientRelations,
                                           error);
        EntityId outTo = 0;
        std::uint64_t value = 0;
        clientRelations.query(kRepRelation, kRepEntity, &outTo, &value, 1);
        ok &= check(outTo == kRepRelationTarget && value == 11u,
                    "relationship update propagates", report);
    }
    {
        std::vector<MimitaNet::RelationshipRecord> relRemove;
        MimitaNet::RelationshipRecord r;
        r.op = 4;
        r.source = kRepEntity;
        r.target = kRepRelationTarget;
        r.typeId = kRepRelation;
        relRemove.push_back(r);
        MimitaNet::dynamicReplicationApply({}, relRemove, {}, store, clientRelations,
                                           error);
        ok &= check(!clientRelations.has(kRepRelation, kRepEntity, kRepRelationTarget),
                    "relationship remove propagates", report);
    }

    // A payload whose size does not match the schema is rejected, not reinterpreted.
    std::vector<MimitaNet::DynamicComponentRecord> bad;
    {
        MimitaNet::DynamicComponentRecord r;
        r.op = 0;
        r.entity = kRepEntity;
        r.typeId = kRep;
        r.schemaHash = kRepHashV1;
        r.schemaVersion = 1;
        r.payload.assign(3, 0x7f);
        bad.push_back(r);
    }
    const bool badApplied =
        MimitaNet::dynamicReplicationApply(bad, {}, {}, store, clientRelations, error);
    store.read(kRepEntity, kRep, &received, sizeof(received));
    ok &= check(!badApplied && received.value == 7,
                "mismatched payload rejected and state preserved", report);

    // Schema migration v1 -> v2 arrives over the same envelope.
    store.registerMigration(kRep, 1, 2, &migrateRepV1toV2);
    std::vector<MimitaNet::DynamicComponentRecord> migration;
    migration.push_back(schemaDescriptor(kRep, gameHash("RepTestState.v2"), 2,
                                         sizeof(RepTestStateV2), "RepTestState"));
    error.clear();
    ok &= check(MimitaNet::dynamicReplicationApply(migration, {}, {}, store,
                                                   clientRelations, error),
                "client applies v2 schema with migration", report);
    RepTestStateV2 migrated{};
    ok &= check(store.read(kRepEntity, kRep, &migrated, sizeof(migrated)) &&
                    migrated.value == 7 && migrated.extra == 42,
                "migration preserves compatible state", report);

    // A version with no migration is rejected and last-good state remains.
    std::vector<MimitaNet::DynamicComponentRecord> badMigration;
    badMigration.push_back(schemaDescriptor(kRep, gameHash("RepTestState.v3"), 3, 16,
                                            "RepTestState"));
    error.clear();
    const bool migratedV3 = MimitaNet::dynamicReplicationApply(
        badMigration, {}, {}, store, clientRelations, error);
    store.read(kRepEntity, kRep, &migrated, sizeof(migrated));
    ok &= check(!migratedV3 && migrated.value == 7,
                "failed schema migration preserves last-good state", report);

    // ── Generic entity lifecycle + falsification ─────────────────────
    {
        std::string lifeError;
        std::vector<MimitaNet::EntityLifecycleRecord> create;
        MimitaNet::EntityLifecycleRecord c;
        c.op = 0;
        c.entity = kRepEntity;
        c.generation = 0;
        create.push_back(c);
        create.push_back(c);  // duplicate CREATE is idempotent
        ok &= check(MimitaNet::dynamicReplicationApply({}, {}, create, store,
                                                       clientRelations, lifeError) &&
                        EntityRegistry::instance().alive(
                            static_cast<EntityId>(kRepEntity)),
                    "client creates the entity from a generic CREATE record", report);

        store.write(kRepEntity, kRep, &server, sizeof(server));
        std::vector<MimitaNet::EntityLifecycleRecord> destroy;
        MimitaNet::EntityLifecycleRecord d;
        d.op = 1;
        d.entity = kRepEntity;
        destroy.push_back(d);
        destroy.push_back(d);  // duplicate DESTROY is a no-op
        ok &= check(MimitaNet::dynamicReplicationApply({}, {}, destroy, store,
                                                       clientRelations, lifeError) &&
                        !EntityRegistry::instance().alive(
                            static_cast<EntityId>(kRepEntity)) &&
                        !store.has(kRepEntity, kRep),
                    "client destroys the entity and clears its state", report);

        RepTestState stale{123, 1};
        std::vector<MimitaNet::DynamicComponentRecord> late;
        MimitaNet::DynamicComponentRecord lr;
        lr.op = 0;
        lr.entity = kRepEntity;
        lr.typeId = kRep;
        lr.schemaHash = kRepHashV1;
        lr.schemaVersion = 1;
        lr.payload.assign(reinterpret_cast<const std::uint8_t*>(&stale),
                          reinterpret_cast<const std::uint8_t*>(&stale) +
                              sizeof(stale));
        late.push_back(lr);
        MimitaNet::dynamicReplicationApply(late, {}, {}, store, clientRelations,
                                           lifeError);
        ok &= check(!store.has(kRepEntity, kRep),
                    "stale UPDATE after DESTROY cannot resurrect the entity", report);

        const EntityId regen = makeEntityId(
            EntityRealm::Server, EntityDomain::None,
            entityLegacyId(static_cast<EntityId>(kRepEntity)), 3);
        std::vector<MimitaNet::EntityLifecycleRecord> recreate;
        MimitaNet::EntityLifecycleRecord rc;
        rc.op = 0;
        rc.entity = regen;
        rc.generation = 3;
        recreate.push_back(rc);
        ok &= check(MimitaNet::dynamicReplicationApply({}, {}, recreate, store,
                                                       clientRelations, lifeError) &&
                        EntityRegistry::instance().alive(regen),
                    "id reuse with a new generation adopts the new identity", report);
    }

    store.clear();
    return ok;
}

