// 09 14 2026
/* purpose
* Headless self-test for the generic dynamic entity/component lifecycle.
* Uses the recovered kernel store directly; it does not depend on the retired
* live-code runtime wrappers.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#include "ecs/dynamic-lifecycle-selftest.h"

#include <cstring>
#include <string>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/relationship-store.h"

using namespace MimitaRuntime;

namespace {

constexpr std::uint64_t hashName(const char* text)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
         p && *p; ++p) {
        hash ^= *p;
        hash *= 1099511628211ull;
    }
    return hash;
}

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

const std::uint64_t kBanana = hashName("BananaComponent");
const std::uint64_t kBananaRel = hashName("relationship.banana.seed");

struct BananaV1 {
    float ripeness;
    std::uint32_t peeled;
};

struct BananaV2 {
    float ripeness;
    std::uint32_t peeled;
    float age;
};

bool migrateV1toV2(const void* oldState, std::size_t oldSize,
                   void* newState, std::size_t newSize)
{
    if (!oldState || !newState || oldSize < sizeof(BananaV1) ||
        newSize < sizeof(BananaV2)) {
        return false;
    }
    const auto* oldValue = static_cast<const BananaV1*>(oldState);
    auto* newValue = static_cast<BananaV2*>(newState);
    newValue->ripeness = oldValue->ripeness;
    newValue->peeled = oldValue->peeled;
    newValue->age = 0.0f;
    return true;
}

bool failMigration(const void*, std::size_t, void*, std::size_t)
{
    return false;
}

DynamicComponentSchema schema(std::uint64_t typeId, std::uint32_t version,
                              std::uint32_t size)
{
    DynamicComponentSchema value;
    value.typeId = typeId;
    value.schemaHash = hashName(version == 1 ? "BananaComponent.v1" : "BananaComponent.v2");
    value.version = version;
    value.size = size;
    value.align = alignof(float);
    value.name = "BananaComponent";
    return value;
}

}  // namespace

bool runDynamicLifecycleSelfTest(std::string& report)
{
    bool ok = true;
    EntityRegistry& registry = EntityRegistry::instance();
    DynamicComponentStore& store = DynamicComponentStore::instance();
    RelationshipStore& relationships = RelationshipStore::instance();

    registry.destroyAll();
    store.clear();
    relationships.clear();

    DynamicComponentSchema v1 = schema(kBanana, 1, sizeof(BananaV1));
    store.registerSchema(v1);
    const EntityId entity = registry.createGeneric(EntityRealm::Server);
    BananaV1 banana{0.25f, 0u};
    ok &= check(store.write(entity, kBanana, &banana, sizeof(banana)),
                "dynamic component write", report);

    BananaV1 read{};
    ok &= check(store.read(entity, kBanana, &read, sizeof(read)) &&
                    read.ripeness == 0.25f,
                "dynamic component read round-trips", report);

    read.ripeness = 0.75f;
    ok &= check(store.write(entity, kBanana, &read, sizeof(read)),
                "dynamic component write updates", report);

    EntityId found[2]{};
    const std::size_t count = store.enumerate(kBanana, found, 2);
    ok &= check(count == 1 && found[0] == entity,
                "dynamic component enumeration", report);

    relationships.add(kBananaRel, entity, entity, 7u);
    EntityId to[4]{};
    std::uint64_t values[4]{};
    ok &= check(relationships.query(kBananaRel, entity, to, values, 4) == 1 &&
                    to[0] == entity && values[0] == 7u,
                "relationship add/query", report);
    relationships.remove(kBananaRel, entity, entity);
    ok &= check(relationships.query(kBananaRel, entity, to, values, 4) == 0,
                "relationship remove", report);

    store.registerMigration(kBanana, 1, 2, &migrateV1toV2);
    std::string error;
    const std::vector<DynamicComponentSchema> v2Set{
        schema(kBanana, 2, sizeof(BananaV2))};
    ok &= check(store.applySchemaUpdate(v2Set, error),
                "schema v2 migration applied", report);
    BananaV2 migrated{};
    ok &= check(store.read(entity, kBanana, &migrated, sizeof(migrated)) &&
                    migrated.ripeness == 0.75f && migrated.age == 0.0f &&
                    store.versionOf(entity, kBanana) == 2,
                "migration preserved bytes and version", report);

    store.registerMigration(kBanana, 2, 3, &failMigration);
    const std::vector<DynamicComponentSchema> v3Set{
        schema(kBanana, 3, sizeof(BananaV2))};
    error.clear();
    const bool rejected = !store.applySchemaUpdate(v3Set, error);
    ok &= check(rejected && store.schema(kBanana) &&
                    store.schema(kBanana)->version == 2 &&
                    store.versionOf(entity, kBanana) == 2,
                "failed migration rejects schema and keeps old state", report);

    const std::vector<std::uint8_t> a = store.serializeType(kBanana);
    const std::vector<std::uint8_t> b = store.serializeType(kBanana);
    ok &= check(!a.empty() && a == b, "serializeType is deterministic", report);
    ok &= check(store.worldHash() == store.worldHash(), "worldHash is deterministic", report);

    registry.destroy(entity);
    ok &= check(!registry.alive(entity) && !store.has(entity, kBanana) &&
                    relationships.query(kBananaRel, entity, to, values, 4) == 0,
                "destroy purges dynamic components and relationships", report);
    return ok;
}
