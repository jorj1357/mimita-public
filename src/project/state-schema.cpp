// 09 12 2026
/* purpose
* Implements the state schema registry and migration dispatch.
* Does NOT own component storage.
*/
#include "project/state-schema.h"

#include "live-code/code-hash.h"

#include <cstring>

namespace Project {

StateSchemaRegistry& StateSchemaRegistry::instance()
{
    static StateSchemaRegistry registry;
    return registry;
}

std::uint64_t StateSchemaRegistry::schemaKey(std::uint32_t typeId, std::uint32_t version)
{
    return ((std::uint64_t)typeId << 32) | (std::uint64_t)version;
}

std::uint64_t StateSchemaRegistry::migrationKey(std::uint32_t typeId,
                                                std::uint32_t fromVersion,
                                                std::uint32_t toVersion)
{
    return ((std::uint64_t)typeId << 32) |
           ((std::uint64_t)fromVersion << 16) |
           (std::uint64_t)toVersion;
}

void StateSchemaRegistry::registerSchema(StateSchema schema)
{
    if (schema.hash.empty())
        schema.hash = LiveCodeHash::sha256Bytes(schema.canonical.data(),
                                                schema.canonical.size());
    schemas_[schemaKey(schema.typeId, schema.version)] = std::move(schema);
}

bool StateSchemaRegistry::registerMigration(std::uint32_t typeId,
                                            std::uint32_t fromVersion,
                                            std::uint32_t toVersion, MigrationFn fn)
{
    if (!fn)
        return false;
    migrations_[migrationKey(typeId, fromVersion, toVersion)] = fn;
    return true;
}

const StateSchema* StateSchemaRegistry::find(std::uint32_t typeId,
                                             std::uint32_t version) const
{
    auto it = schemas_.find(schemaKey(typeId, version));
    return it == schemas_.end() ? nullptr : &it->second;
}

bool StateSchemaRegistry::canMigrate(std::uint32_t typeId, std::uint32_t fromVersion,
                                     std::uint32_t toVersion) const
{
    if (fromVersion == toVersion)
        return true;
    return migrations_.find(migrationKey(typeId, fromVersion, toVersion)) !=
           migrations_.end();
}

bool StateSchemaRegistry::migrate(std::uint32_t typeId, std::uint32_t fromVersion,
                                  std::uint32_t toVersion, const void* oldState,
                                  std::size_t oldSize, void* newState,
                                  std::size_t newSize) const
{
    if (fromVersion == toVersion) {
        if (oldState && newState && oldSize <= newSize)
            std::memcpy(newState, oldState, oldSize);
        return true;
    }
    auto it = migrations_.find(migrationKey(typeId, fromVersion, toVersion));
    if (it == migrations_.end())
        return false;
    return it->second(oldState, oldSize, newState, newSize);
}

} // namespace Project
