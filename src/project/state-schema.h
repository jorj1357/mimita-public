// 09 12 2026
/* purpose
* Kernel registry for versioned state schemas and package-supplied migrations.
* A generation can run migrations before new code observes state.
* Does NOT own component storage or execute gameplay.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "project/project-types.h"

namespace Project {

// Copies/migrates oldState into newState. Returns false to abort activation.
using MigrationFn = bool (*)(const void* oldState, std::size_t oldSize,
                             void* newState, std::size_t newSize);

class StateSchemaRegistry {
public:
    static StateSchemaRegistry& instance();

    // Registers a schema. If schema.hash is empty it is derived from
    // `canonical` (canonical serialization of the fields).
    void registerSchema(StateSchema schema);

    bool registerMigration(std::uint32_t typeId, std::uint32_t fromVersion,
                           std::uint32_t toVersion, MigrationFn fn);

    const StateSchema* find(std::uint32_t typeId, std::uint32_t version) const;
    bool canMigrate(std::uint32_t typeId, std::uint32_t fromVersion,
                    std::uint32_t toVersion) const;
    bool migrate(std::uint32_t typeId, std::uint32_t fromVersion,
                 std::uint32_t toVersion, const void* oldState, std::size_t oldSize,
                 void* newState, std::size_t newSize) const;

    std::size_t schemaCount() const { return schemas_.size(); }
    std::size_t migrationCount() const { return migrations_.size(); }

private:
    StateSchemaRegistry() = default;

    static std::uint64_t schemaKey(std::uint32_t typeId, std::uint32_t version);
    static std::uint64_t migrationKey(std::uint32_t typeId, std::uint32_t fromVersion,
                                      std::uint32_t toVersion);

    std::map<std::uint64_t, StateSchema> schemas_;
    std::map<std::uint64_t, MigrationFn> migrations_;
};

} // namespace Project
