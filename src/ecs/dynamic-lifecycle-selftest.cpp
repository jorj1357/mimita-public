// 09 14 2026
/* purpose
* Implements the headless generic dynamic entity/component lifecycle self-test.
* Asserts invariants, determinism, and safe-failure behavior; never pins tuned
* gameplay constants.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#include "ecs/dynamic-lifecycle-selftest.h"

#include <cstring>
#include <string>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/relationship-store.h"
#include "hot-reload/generic-runtime.h"
#include "live-code/live-behavior.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

const std::uint64_t kBanana = gameHash("BananaComponent");
const std::uint64_t kBananaRel = gameHash("relationship.banana.seed");

struct BananaV1 {
    float ripeness;
    std::uint32_t peeled;
};

struct BananaV2 {
    float ripeness;
    std::uint32_t peeled;
    float age;
};

bool migrateBananaV1toV2(const void* oldState, std::size_t oldSize,
                         void* newState, std::size_t newSize)
{
    if (oldSize < sizeof(BananaV1) || newSize < sizeof(BananaV2))
        return false;
    BananaV1 old{};
    std::memcpy(&old, oldState, sizeof(BananaV1));
    BananaV2 out{};
    out.ripeness = old.ripeness;
    out.peeled = old.peeled;
    out.age = 0.0f;
    std::memcpy(newState, &out, sizeof(BananaV2));
    return true;
}

bool failMigration(const void*, std::size_t, void*, std::size_t)
{
    return false;
}

GameComponentSchemaDescriptorV1 makeSchema(std::uint64_t id, std::uint64_t hash,
                                           std::uint32_t size, std::uint32_t version,
                                           const char* name)
{
    GameComponentSchemaDescriptorV1 s{};
    s.id = id;
    s.schemaHash = hash;
    s.size = size;
    s.align = 4;
    s.copyPolicy = GAME_COPY_AUTHORING;
    s.networkPolicy = 0;
    s.name = name;
    s.version = version;
    return s;
}

GamePackageDescriptorV1 basePackage(std::uint64_t id, std::uint64_t logical, const char* name)
{
    GamePackageDescriptorV1 p{};
    p.structSize = sizeof(GamePackageDescriptorV1);
    p.abiVersion = MIMITA_PACKAGE_ABI_VERSION;
    p.packageId = id;
    p.logicalHash = logical;
    p.name = name;
    return p;
}

} // namespace

bool runDynamicLifecycleSelfTest(std::string& report)
{
    bool ok = true;
    EntityRegistry& registry = EntityRegistry::instance();
    DynamicComponentStore& store = DynamicComponentStore::instance();
    RelationshipStore& relationships = RelationshipStore::instance();

    registry.destroyAll();
    store.clear();
    relationships.clear();

    // A brand-new schema, unknown to the EXE at startup.
    GameComponentSchemaDescriptorV1 schemaV1 =
        makeSchema(kBanana, gameHash("BananaComponent.v1"), sizeof(BananaV1), 1,
                   "BananaComponent");
    GamePackageDescriptorV1 package1 =
        basePackage(gameHash("test.dynamic.v1"), gameHash("test.dynamic.v1.logical"),
                    "test.dynamic.v1");
    package1.componentSchemas = &schemaV1;
    package1.componentSchemaCount = 1;

    std::string error;
    ok &= check(GenericRuntime::instance().activate(&package1, error),
                "activate package with live schema v1" + (error.empty() ? "" : " [" + error + "]"),
                report);

    GameplayContextV1* ctx = LiveBehavior::hostContext(1);
    ok &= check(ctx && ctx->entityCreate && ctx->entityDestroy &&
                    ctx->dynamicReadComponent && ctx->dynamicWriteComponent &&
                    ctx->dynamicRemoveComponent && ctx->dynamicEnumerateComponent &&
                    ctx->dynamicComponentsOnEntity && ctx->dynamicComponentInfo &&
                    ctx->relationshipAdd && ctx->relationshipRemove &&
                    ctx->relationshipQuery,
                "lifecycle capabilities present", report);
    if (!ctx)
        return false;

    std::uint64_t entity = 0;
    ok &= check(ctx->entityCreate(ctx->host, static_cast<std::uint32_t>(EntityRealm::Server),
                                  &entity) &&
                    entity != 0,
                "entity.create allocates a stable id", report);

    BananaV1 banana{0.25f, 0u};
    ok &= check(ctx->dynamicWriteComponent(ctx->host, entity, kBanana, &banana,
                                           sizeof(banana)),
                "component.attach via dynamic write", report);

    BananaV1 read{};
    ok &= check(ctx->dynamicReadComponent(ctx->host, entity, kBanana, &read,
                                          sizeof(read)) &&
                    read.ripeness == 0.25f,
                "component.read round-trips", report);

    read.ripeness = 0.75f;
    ok &= check(ctx->dynamicWriteComponent(ctx->host, entity, kBanana, &read,
                                           sizeof(read)),
                "component.write applies", report);
    read = BananaV1{};
    ctx->dynamicReadComponent(ctx->host, entity, kBanana, &read, sizeof(read));
    ok &= check(read.ripeness == 0.75f, "component.write persisted", report);

    std::uint64_t found[8]{};
    std::uint32_t count = ctx->dynamicEnumerateComponent(ctx->host, kBanana, found, 8);
    ok &= check(count == 1 && found[0] == entity, "component.enumerate finds entity", report);

    std::uint64_t types[8]{};
    count = ctx->dynamicComponentsOnEntity(ctx->host, entity, types, 8);
    ok &= check(count == 1 && types[0] == kBanana, "component.typesOnEntity finds schema",
                report);

    GameDynamicComponentInfoV1 info{};
    ok &= check(ctx->dynamicComponentInfo(ctx->host, kBanana, &info) &&
                    info.version == 1 && info.size == sizeof(BananaV1),
                "component.schema info reports version/size", report);

    // ── Relationships ──
    ok &= check(ctx->relationshipAdd(ctx->host, kBananaRel, entity, entity, 7u),
                "relationship.add", report);
    std::uint64_t to[4]{};
    std::uint64_t value[4]{};
    count = ctx->relationshipQuery(ctx->host, kBananaRel, entity, to, value, 4);
    ok &= check(count == 1 && to[0] == entity && value[0] == 7u, "relationship.query",
                report);
    ok &= check(ctx->relationshipRemove(ctx->host, kBananaRel, entity, entity),
                "relationship.remove", report);
    count = ctx->relationshipQuery(ctx->host, kBananaRel, entity, to, value, 4);
    ok &= check(count == 0, "relationship removed", report);

    // ── Remove component, then re-attach for the migration test ──
    ok &= check(ctx->dynamicRemoveComponent(ctx->host, entity, kBanana) &&
                    !store.has(entity, kBanana),
                "component.remove detaches", report);
    ok &= check(ctx->dynamicWriteComponent(ctx->host, entity, kBanana, &banana,
                                           sizeof(banana)),
                "component re-attach", report);

    // ── Schema v2 with a v1 -> v2 migration ──
    GameComponentSchemaDescriptorV1 schemaV2 =
        makeSchema(kBanana, gameHash("BananaComponent.v2"), sizeof(BananaV2), 2,
                   "BananaComponent");
    GameMigrationDescriptorV1 migration{};
    migration.typeId = kBanana;
    migration.fromVersion = 1;
    migration.toVersion = 2;
    migration.migrate = reinterpret_cast<void*>(&migrateBananaV1toV2);
    GamePackageDescriptorV1 package2 =
        basePackage(gameHash("test.dynamic.v2"), gameHash("test.dynamic.v2.logical"),
                    "test.dynamic.v2");
    package2.componentSchemas = &schemaV2;
    package2.componentSchemaCount = 1;
    package2.migrations = &migration;
    package2.migrationCount = 1;

    error.clear();
    ok &= check(GenericRuntime::instance().activate(&package2, error),
                "activate schema v2 with migration" + (error.empty() ? "" : " [" + error + "]"),
                report);
    BananaV2 migrated{};
    ok &= check(ctx->dynamicReadComponent(ctx->host, entity, kBanana, &migrated,
                                          sizeof(migrated)) &&
                    migrated.ripeness == 0.25f && migrated.age == 0.0f,
                "migration v1->v2 preserved data", report);
    ok &= check(store.versionOf(entity, kBanana) == 2, "migrated blob stamped version 2",
                report);

    // ── A failing migration must reject the candidate and keep v2 state ──
    GameComponentSchemaDescriptorV1 schemaV3 =
        makeSchema(kBanana, gameHash("BananaComponent.v3"), 16, 3, "BananaComponent");
    GameMigrationDescriptorV1 badMigration{};
    badMigration.typeId = kBanana;
    badMigration.fromVersion = 2;
    badMigration.toVersion = 3;
    badMigration.migrate = reinterpret_cast<void*>(&failMigration);
    GamePackageDescriptorV1 package3 =
        basePackage(gameHash("test.dynamic.v3"), gameHash("test.dynamic.v3.logical"),
                    "test.dynamic.v3");
    package3.componentSchemas = &schemaV3;
    package3.componentSchemaCount = 1;
    package3.migrations = &badMigration;
    package3.migrationCount = 1;

    error.clear();
    const bool activated3 = GenericRuntime::instance().activate(&package3, error);
    ok &= check(!activated3, "failing migration rejects the candidate", report);
    ok &= check(store.schema(kBanana) && store.schema(kBanana)->version == 2,
                "previous schema remains after rejection", report);
    ok &= check(store.versionOf(entity, kBanana) == 2 && store.has(entity, kBanana),
                "previous bytes remain after rejection", report);

    // ── Deterministic serialization / hash ──
    const std::vector<std::uint8_t> a = store.serializeType(kBanana);
    const std::vector<std::uint8_t> b = store.serializeType(kBanana);
    ok &= check(!a.empty() && a == b, "serializeType is deterministic", report);
    ok &= check(store.worldHash() == store.worldHash(), "worldHash is deterministic", report);

    // ── Destroy purges dynamic state and relationships ──
    ok &= check(ctx->relationshipAdd(ctx->host, kBananaRel, entity, entity, 9u),
                "relationship.add before destroy", report);
    ok &= check(ctx->entityDestroy(ctx->host, entity), "entity.destroy", report);
    ok &= check(!registry.alive(static_cast<EntityId>(entity)) && !store.has(entity, kBanana) &&
                    relationships.query(kBananaRel, entity, to, value, 4) == 0,
                "destroy purges dynamic components and relationships", report);

    GenericRuntime::instance().deactivate();
    return ok;
}
