// 09 13 2026
/* purpose
* Implements dynamic component storage.
* Does NOT own typed ECS components, rendering, or gameplay policy.
*/
#include "ecs/dynamic-components.h"

#include <algorithm>
#include <cstring>

namespace MimitaRuntime {

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
}

void DynamicComponentStore::registerSchema(const DynamicComponentSchema& schema)
{
    if (schema.typeId == 0)
        return;
    schemas_[schema.typeId] = schema;
}

const DynamicComponentSchema* DynamicComponentStore::schema(std::uint64_t typeId) const
{
    auto it = schemas_.find(typeId);
    return it == schemas_.end() ? nullptr : &it->second;
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
    return erased;
}

void DynamicComponentStore::eraseEntity(EntityId entity)
{
    auto typesIt = entityTypes_.find(entity);
    if (typesIt == entityTypes_.end())
        return;
    for (std::uint64_t typeId : typesIt->second) {
        auto typeIt = data_.find(typeId);
        if (typeIt != data_.end())
            typeIt->second.erase(entity);
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

} // namespace MimitaRuntime
