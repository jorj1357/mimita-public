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
    // Allocates a fresh identity for a package/dynamic concept that has no
    // compile-time domain. Uses EntityDomain::None plus a kernel-owned monotonic
    // id, so it never collides with player/npc/projectile/world-object ids and
    // requires no new enum. Pass a non-zero legacyId to request a stable id.
    EntityId createGeneric(EntityRealm realm, std::uint32_t legacyId = 0);
    // The single legacy-id allocator for typed actors (one per (realm, domain)).
    // Subsystems that need a new NPC/player/projectile id must call this instead
    // of keeping a private counter, so two subsystems can never collide.
    std::uint32_t allocateLegacyId(EntityRealm realm, EntityDomain domain);
    // Registers the exact packed EntityId (used by generic replication so a
    // client materializes the server's identity verbatim). If a different
    // generation currently owns the same (realm, domain, legacyId) key, the old
    // identity is retired first so the new generation cannot alias it.
    EntityId adopt(EntityId id);
    void destroy(EntityId id);
    // Append-only destroyed-id log drained by generic entity lifecycle
    // replication so a DESTROY can be emitted without coupling ECS to network.
    std::vector<EntityId> consumeDestroyed();
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
    std::vector<EntityId> mDestroyed;
    std::uint32_t nextDynamicId_ = 1;
    std::unordered_map<std::uint64_t, std::uint32_t> mNextLegacyId;
};
