// 09 13 2026
/* purpose
* Kernel-owned dynamic component storage: package-declared component schemas
* (by content hash) stored per entity without a compile-time C++ type or a giant
* enum. Typed C++ components keep their fast-path stores; dynamic schemas are
* the general path for concepts introduced after startup.
* Does NOT own typed ECS components, rendering, or gameplay policy.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/entity-types.h"

namespace MimitaRuntime {

struct DynamicComponentSchema {
    std::uint64_t typeId = 0;       // gameHash("Component")
    std::uint64_t schemaHash = 0;
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

    // Registers (or replaces) a package-declared component schema.
    void registerSchema(const DynamicComponentSchema& schema);
    const DynamicComponentSchema* schema(std::uint64_t typeId) const;
    std::size_t schemaCount() const { return schemas_.size(); }

    // Byte-blob read/write for one entity + schema. Size must match the schema.
    bool write(EntityId entity, std::uint64_t typeId, const void* data, std::uint32_t size);
    bool read(EntityId entity, std::uint64_t typeId, void* out, std::uint32_t size) const;
    bool has(EntityId entity, std::uint64_t typeId) const;
    bool remove(EntityId entity, std::uint64_t typeId);
    void eraseEntity(EntityId entity);

    std::size_t componentCount() const;

private:
    DynamicComponentStore() = default;

    struct Blob {
        std::vector<std::uint8_t> bytes;
    };

    std::unordered_map<std::uint64_t, DynamicComponentSchema> schemas_;
    // typeId -> (entity -> blob)
    std::unordered_map<std::uint64_t, std::unordered_map<EntityId, Blob>> data_;
    // entity -> typeIds present (for eraseEntity)
    std::unordered_map<EntityId, std::vector<std::uint64_t>> entityTypes_;
};

} // namespace MimitaRuntime
