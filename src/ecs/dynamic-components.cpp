// 09 14 2026
/* purpose
* Implements dynamic component storage, activation-time schema migration, and
* deterministic serialization.
* Does NOT own typed ECS components, rendering, or gameplay policy.
*/
#include "ecs/dynamic-components.h"

#include <algorithm>
#include <cstring>

namespace MimitaRuntime {

namespace {

std::uint64_t fnv1a(std::uint64_t hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<std::uint64_t>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

void appendBytes(std::vector<std::uint8_t>& out, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

template <typename T>
void appendPod(std::vector<std::uint8_t>& out, const T& value)
{
    appendBytes(out, &value, sizeof(T));
}

} // namespace

DynamicComponentStore& DynamicComponentStore::instance()
{
    static DynamicComponentStore store;
    return store;
}

void DynamicComponentStore::clear()
{
    schemas_.clear();
    data_.clear();
    entityTypes_.clear();
    migrations_.clear();
    removals_.clear();
    revision_ = 0;
}

bool DynamicComponentStore::MigrationKey::operator<(const MigrationKey& other) const
{
    if (typeId != other.typeId)
        return typeId < other.typeId;
    if (fromVersion != other.fromVersion)
        return fromVersion < other.fromVersion;
    return toVersion < other.toVersion;
}

void DynamicComponentStore::registerSchema(const DynamicComponentSchema& schema)
{
    if (schema.typeId == 0)
        return;
    DynamicComponentSchema normalized = schema;
    if (normalized.version == 0)
        normalized.version = 1;
    if (normalized.align == 0)
        normalized.align = 1;
    schemas_[normalized.typeId] = std::move(normalized);
}

const DynamicComponentSchema* DynamicComponentStore::schema(std::uint64_t typeId) const
{
    auto it = schemas_.find(typeId);
    return it == schemas_.end() ? nullptr : &it->second;
}

void DynamicComponentStore::registerMigration(std::uint64_t typeId,
                                              std::uint32_t fromVersion,
                                              std::uint32_t toVersion,
                                              DynamicMigrationFn fn)
{
    if (typeId == 0 || !fn || fromVersion == toVersion)
        return;
    migrations_[MigrationKey{typeId, fromVersion, toVersion}] = fn;
}

bool DynamicComponentStore::hasMigration(std::uint64_t typeId,
                                         std::uint32_t fromVersion,
                                         std::uint32_t toVersion) const
{
    if (typeId == 0 || fromVersion == toVersion)
        return false;
    return migrations_.find(MigrationKey{typeId, fromVersion, toVersion}) !=
        migrations_.end();
}

std::uint32_t DynamicComponentStore::maxStoredVersion(std::uint64_t typeId) const
{
    const auto it = data_.find(typeId);
    if (it == data_.end())
        return 0;
    std::uint32_t maxVersion = 0;
    for (const auto& kv : it->second)
        maxVersion = std::max(maxVersion, kv.second.version);
    return maxVersion;
}

bool DynamicComponentStore::applySchemaUpdate(
    const std::vector<DynamicComponentSchema>& schemas, std::string& error)
{
    // Normalize + validate the incoming schemas first.
    std::unordered_map<std::uint64_t, DynamicComponentSchema> incoming;
    for (const DynamicComponentSchema& raw : schemas) {
        if (raw.typeId == 0)
            continue;
        DynamicComponentSchema s = raw;
        if (s.version == 0)
            s.version = 1;
        if (s.align == 0)
            s.align = 1;
        incoming[s.typeId] = std::move(s);
    }

    // Stage migrated blobs so a failure leaves storage untouched.
    struct StagedBlob {
        Blob blob;
    };
    std::unordered_map<std::uint64_t, std::unordered_map<EntityId, StagedBlob>> staged;

    for (const auto& typeEntry : data_) {
        const std::uint64_t typeId = typeEntry.first;
        auto newSchemaIt = incoming.find(typeId);
        const DynamicComponentSchema* target =
            newSchemaIt != incoming.end() ? &newSchemaIt->second : schema(typeId);
        if (!target)
            continue;  // no schema known for this data; leave it untouched
        const std::uint32_t targetVersion = target->version;

        for (const auto& entityEntry : typeEntry.second) {
            const EntityId entity = entityEntry.first;
            const Blob& blob = entityEntry.second;
            if (blob.version == targetVersion)
                continue;

            const DynamicMigrationFn fn =
                migrations_[MigrationKey{typeId, blob.version, targetVersion}];
            if (!fn) {
                error = "missing migration typeId=" + std::to_string(typeId) +
                        " from=" + std::to_string(blob.version) +
                        " to=" + std::to_string(targetVersion);
                return false;
            }

            const std::size_t outSize =
                target->size != 0 ? target->size : blob.bytes.size();
            std::vector<std::uint8_t> migrated(outSize);
            if (!fn(blob.bytes.data(), blob.bytes.size(), migrated.data(), outSize)) {
                error = "migration failed typeId=" + std::to_string(typeId) +
                        " from=" + std::to_string(blob.version) +
                        " to=" + std::to_string(targetVersion);
                return false;
            }
            StagedBlob sb;
            sb.blob.bytes = std::move(migrated);
            sb.blob.version = targetVersion;
            staged[typeId][entity] = std::move(sb);
        }
    }

    // Commit: schemas first, then staged blobs.
    for (auto& entry : incoming)
        schemas_[entry.first] = std::move(entry.second);
    for (auto& typeEntry : staged) {
        auto& typeData = data_[typeEntry.first];
        for (auto& entityEntry : typeEntry.second)
            typeData[entityEntry.first] = std::move(entityEntry.second.blob);
    }
    return true;
}

bool DynamicComponentStore::write(EntityId entity, std::uint64_t typeId,
                                  const void* data, std::uint32_t size)
{
    if (entity == kInvalidEntityId || !data)
        return false;
    const DynamicComponentSchema* s = schema(typeId);
    if (!s || (s->size != 0 && s->size != size))
        return false;
    Blob& blob = data_[typeId][entity];
    blob.bytes.assign(static_cast<const std::uint8_t*>(data),
                      static_cast<const std::uint8_t*>(data) + size);
    blob.version = s->version;
    blob.changeVersion = ++revision_;
    auto& types = entityTypes_[entity];
    if (std::find(types.begin(), types.end(), typeId) == types.end())
        types.push_back(typeId);
    return true;
}

bool DynamicComponentStore::read(EntityId entity, std::uint64_t typeId,
                                 void* out, std::uint32_t size) const
{
    if (!out)
        return false;
    auto typeIt = data_.find(typeId);
    if (typeIt == data_.end())
        return false;
    auto entityIt = typeIt->second.find(entity);
    if (entityIt == typeIt->second.end())
        return false;
    if (entityIt->second.bytes.size() < size)
        return false;
    std::memcpy(out, entityIt->second.bytes.data(), size);
    return true;
}

bool DynamicComponentStore::has(EntityId entity, std::uint64_t typeId) const
{
    auto typeIt = data_.find(typeId);
    if (typeIt == data_.end())
        return false;
    return typeIt->second.find(entity) != typeIt->second.end();
}

std::uint32_t DynamicComponentStore::versionOf(EntityId entity, std::uint64_t typeId) const
{
    auto typeIt = data_.find(typeId);
    if (typeIt == data_.end())
        return 0;
    auto entityIt = typeIt->second.find(entity);
    return entityIt == typeIt->second.end() ? 0 : entityIt->second.version;
}

bool DynamicComponentStore::remove(EntityId entity, std::uint64_t typeId)
{
    auto typeIt = data_.find(typeId);
    if (typeIt == data_.end())
        return false;
    const bool erased = typeIt->second.erase(entity) > 0;
    auto typesIt = entityTypes_.find(entity);
    if (typesIt != entityTypes_.end()) {
        auto& v = typesIt->second;
        v.erase(std::remove(v.begin(), v.end(), typeId), v.end());
    }
    if (erased)
        removals_.push_back(
            Removal{entity, typeId, static_cast<std::uint32_t>(++revision_)});
    return erased;
}

void DynamicComponentStore::eraseEntity(EntityId entity)
{
    auto typesIt = entityTypes_.find(entity);
    if (typesIt == entityTypes_.end())
        return;
    for (std::uint64_t typeId : typesIt->second) {
        auto typeIt = data_.find(typeId);
        if (typeIt != data_.end() && typeIt->second.erase(entity) > 0)
            removals_.push_back(
                Removal{entity, typeId, static_cast<std::uint32_t>(++revision_)});
    }
    entityTypes_.erase(typesIt);
}

std::size_t DynamicComponentStore::componentCount() const
{
    std::size_t count = 0;
    for (const auto& entry : data_)
        count += entry.second.size();
    return count;
}

std::size_t DynamicComponentStore::enumerate(std::uint64_t typeId, EntityId* out,
                                             std::size_t maxOut) const
{
    if (!out || maxOut == 0)
        return 0;
    auto typeIt = data_.find(typeId);
    if (typeIt == data_.end())
        return 0;
    std::vector<EntityId> ids;
    ids.reserve(typeIt->second.size());
    for (const auto& entry : typeIt->second)
        ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    const std::size_t count = std::min(maxOut, ids.size());
    for (std::size_t i = 0; i < count; ++i)
        out[i] = ids[i];
    return count;
}

std::size_t DynamicComponentStore::componentsOnEntity(EntityId entity,
                                                      std::uint64_t* outTypeIds,
                                                      std::size_t maxOut) const
{
    if (!outTypeIds || maxOut == 0)
        return 0;
    auto it = entityTypes_.find(entity);
    if (it == entityTypes_.end())
        return 0;
    std::vector<std::uint64_t> ids = it->second;
    std::sort(ids.begin(), ids.end());
    const std::size_t count = std::min(maxOut, ids.size());
    for (std::size_t i = 0; i < count; ++i)
        outTypeIds[i] = ids[i];
    return count;
}

std::vector<std::uint8_t> DynamicComponentStore::serializeType(std::uint64_t typeId) const
{
    std::vector<std::uint8_t> out;
    const DynamicComponentSchema* s = schema(typeId);
    const std::uint32_t version = s ? s->version : 0;

    auto typeIt = data_.find(typeId);
    std::vector<EntityId> ids;
    if (typeIt != data_.end()) {
        ids.reserve(typeIt->second.size());
        for (const auto& entry : typeIt->second)
            ids.push_back(entry.first);
        std::sort(ids.begin(), ids.end());
    }

    appendPod(out, typeId);
    appendPod(out, version);
    const std::uint32_t count = static_cast<std::uint32_t>(ids.size());
    appendPod(out, count);
    for (EntityId id : ids) {
        const Blob& blob = typeIt->second.at(id);
        appendPod(out, id);
        const std::uint32_t size = static_cast<std::uint32_t>(blob.bytes.size());
        appendPod(out, size);
        appendBytes(out, blob.bytes.data(), blob.bytes.size());
    }
    return out;
}

std::vector<std::uint64_t> DynamicComponentStore::typeIds() const
{
    std::vector<std::uint64_t> ids;
    ids.reserve(schemas_.size());
    for (const auto& entry : schemas_)
        ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::uint8_t> DynamicComponentStore::blob(EntityId entity,
                                                      std::uint64_t typeId) const
{
    auto typeIt = data_.find(typeId);
    if (typeIt == data_.end())
        return {};
    auto entityIt = typeIt->second.find(entity);
    return entityIt == typeIt->second.end() ? std::vector<std::uint8_t>{}
                                            : entityIt->second.bytes;
}

std::uint32_t DynamicComponentStore::changeVersionOf(EntityId entity,
                                                     std::uint64_t typeId) const
{
    auto typeIt = data_.find(typeId);
    if (typeIt == data_.end())
        return 0;
    auto entityIt = typeIt->second.find(entity);
    return entityIt == typeIt->second.end() ? 0 : entityIt->second.changeVersion;
}

std::vector<DynamicComponentStore::Removal> DynamicComponentStore::consumeRemovals()
{
    std::vector<Removal> out = std::move(removals_);
    removals_.clear();
    return out;
}

std::uint64_t DynamicComponentStore::worldHash() const
{
    std::vector<std::uint64_t> typeIds;
    typeIds.reserve(schemas_.size());
    for (const auto& entry : schemas_)
        typeIds.push_back(entry.first);
    std::sort(typeIds.begin(), typeIds.end());

    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint64_t typeId : typeIds) {
        const std::vector<std::uint8_t> bytes = serializeType(typeId);
        hash = fnv1a(hash, bytes.data(), bytes.size());
    }
    return hash;
}

} // namespace MimitaRuntime
