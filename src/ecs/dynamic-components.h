// 09 14 2026
/* purpose
* Kernel-owned dynamic component storage: package-declared component schemas
* (by content hash) stored per entity without a compile-time C++ type or a giant
* enum. Typed C++ components keep their fast-path stores; dynamic schemas are
* the general path for concepts introduced after startup.
* Owns schema versioning and activation-time migration for dynamic components
* (64-bit type ids). Typed/project state migrations remain in
* project/state-schema.*. Does NOT own typed ECS components, rendering, or
* gameplay policy.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/entity-types.h"

namespace MimitaRuntime {

// Same call shape as Project::MigrationFn so a package migration descriptor can
// serve either storage class.
using DynamicMigrationFn = bool (*)(const void* oldState, std::size_t oldSize,
                                    void* newState, std::size_t newSize);

struct DynamicComponentSchema {
    std::uint64_t typeId = 0;       // gameHash("Component")
    std::uint64_t schemaHash = 0;
    std::uint32_t version = 1;
    std::uint32_t size = 0;
    std::uint32_t align = 1;
    std::uint32_t copyPolicy = 0;   // GameCopyPolicy
    std::uint32_t networkPolicy = 0;
    std::string name;
};

class DynamicComponentStore {
public:
    static DynamicComponentStore& instance();

    void clear();

    // Registers (or replaces) a package-declared component schema. Does not
    // migrate existing bytes; activation uses applySchemaUpdate for that.
    void registerSchema(const DynamicComponentSchema& schema);
    const DynamicComponentSchema* schema(std::uint64_t typeId) const;
    std::size_t schemaCount() const { return schemas_.size(); }
    std::size_t migrationCount() const { return migrations_.size(); }

    // Registers a migration used by applySchemaUpdate. Keyed by the full 64-bit
    // type id (the project state-schema registry keys by 32 bits).
    void registerMigration(std::uint64_t typeId, std::uint32_t fromVersion,
                           std::uint32_t toVersion, DynamicMigrationFn fn);

    // Atomically applies a new schema set. Every stored blob whose version
    // differs from its new schema version is migrated first; on any failure
    // nothing is mutated and `error` explains why. This is the activation-time
    // transaction that keeps last-good state alive on a rejected candidate.
    bool applySchemaUpdate(const std::vector<DynamicComponentSchema>& schemas,
                           std::string& error);

    // Byte-blob read/write for one entity + schema. Size must match the schema.
    bool write(EntityId entity, std::uint64_t typeId, const void* data, std::uint32_t size);
    bool read(EntityId entity, std::uint64_t typeId, void* out, std::uint32_t size) const;
    bool has(EntityId entity, std::uint64_t typeId) const;
    std::uint32_t versionOf(EntityId entity, std::uint64_t typeId) const;
    bool remove(EntityId entity, std::uint64_t typeId);
    void eraseEntity(EntityId entity);

    std::size_t componentCount() const;

    // Deterministic (sorted by entity id) enumeration.
    std::size_t enumerate(std::uint64_t typeId, EntityId* out, std::size_t maxOut) const;
    std::size_t componentsOnEntity(EntityId entity, std::uint64_t* outTypeIds,
                                   std::size_t maxOut) const;

    // Deterministic serialization of all blobs for one type, and a fold of every
    // type for replay/world hashing.
    std::vector<std::uint8_t> serializeType(std::uint64_t typeId) const;
    std::uint64_t worldHash() const;

    // ── Generic change tracking for replication ─────────────────────
    std::vector<std::uint64_t> typeIds() const;
    std::vector<std::uint8_t> blob(EntityId entity, std::uint64_t typeId) const;
    std::uint32_t changeVersionOf(EntityId entity, std::uint64_t typeId) const;
    std::uint64_t revision() const { return revision_; }
    struct Removal {
        EntityId entity = kInvalidEntityId;
        std::uint64_t typeId = 0;
        std::uint32_t changeVersion = 0;
    };
    // Append-only removal log; the replication owner drains it.
    std::vector<Removal> consumeRemovals();

private:
    DynamicComponentStore() = default;

    struct Blob {
        std::vector<std::uint8_t> bytes;
        std::uint32_t version = 1;
        std::uint32_t changeVersion = 0;
    };

    struct MigrationKey {
        std::uint64_t typeId;
        std::uint32_t fromVersion;
        std::uint32_t toVersion;
        bool operator<(const MigrationKey& other) const;
    };

    std::unordered_map<std::uint64_t, DynamicComponentSchema> schemas_;
    // typeId -> (entity -> blob)
    std::unordered_map<std::uint64_t, std::unordered_map<EntityId, Blob>> data_;
    // entity -> typeIds present (for eraseEntity / inspect)
    std::unordered_map<EntityId, std::vector<std::uint64_t>> entityTypes_;
    std::map<MigrationKey, DynamicMigrationFn> migrations_;
    std::uint64_t revision_ = 0;
    std::vector<Removal> removals_;
};

} // namespace MimitaRuntime
