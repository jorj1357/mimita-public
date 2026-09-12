// 09 12 2026
/* purpose
* Stable entity identity with sparse per-component storage.
* One registry holds arbitrary component types keyed by EntityId; adding a new
* component never changes the entity identity or core storage.
* Does NOT own systems, gameplay rules, or rendering.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "ecs/components.h"
#include "ecs/entity-types.h"

class EntityRegistry {
public:
    static EntityRegistry& instance();

    // Idempotent: creating the same (realm, domain, legacyId) returns the
    // existing stable id and never resets its components.
    EntityId create(EntityRealm realm, EntityDomain domain, std::uint32_t legacyId,
                    std::uint16_t generation = 0);
    void destroy(EntityId id);
    void destroyAll();
    bool alive(EntityId id) const;
    EntityId find(EntityRealm realm, EntityDomain domain, std::uint32_t legacyId) const;
    std::size_t count() const { return mIdentities.size(); }

    const EntityIdentity* identity(EntityId id) const;
    std::vector<EntityId> all() const;

    template <typename T>
    T* tryGet(EntityId id)
    {
        if (!alive(id))
            return nullptr;
        auto& s = store<T>().data;
        auto it = s.find(id);
        return it == s.end() ? nullptr : &it->second;
    }

    template <typename T>
    const T* tryGet(EntityId id) const
    {
        auto* s = findStore<T>();
        if (!s)
            return nullptr;
        auto it = s->data.find(id);
        return it == s->data.end() ? nullptr : &it->second;
    }

    template <typename T>
    T& add(EntityId id, const T& value = T{})
    {
        return store<T>().data[id] = value;
    }

    template <typename T>
    void remove(EntityId id)
    {
        store<T>().data.erase(id);
    }

    template <typename T>
    bool has(EntityId id) const
    {
        const auto* s = findStore<T>();
        return s && s->data.find(id) != s->data.end();
    }

    template <typename T>
    std::unordered_map<EntityId, T>& storage()
    {
        return store<T>().data;
    }

private:
    struct IComponentStore {
        virtual ~IComponentStore() = default;
        virtual void erase(EntityId id) = 0;
    };

    template <typename T>
    struct ComponentStore : IComponentStore {
        std::unordered_map<EntityId, T> data;
        void erase(EntityId id) override { data.erase(id); }
    };

    template <typename T>
    ComponentStore<T>& store()
    {
        const std::type_index key(typeid(T));
        auto it = mStores.find(key);
        if (it == mStores.end()) {
            auto created = std::make_unique<ComponentStore<T>>();
            ComponentStore<T>* raw = created.get();
            mStores.emplace(key, std::move(created));
            return *raw;
        }
        return static_cast<ComponentStore<T>&>(*it->second);
    }

    template <typename T>
    const ComponentStore<T>* findStore() const
    {
        auto it = mStores.find(std::type_index(typeid(T)));
        if (it == mStores.end())
            return nullptr;
        return static_cast<const ComponentStore<T>*>(it->second.get());
    }

    std::unordered_map<EntityId, EntityIdentity> mIdentities;
    std::unordered_map<std::uint64_t, EntityId> mLookup;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStore>> mStores;
};
