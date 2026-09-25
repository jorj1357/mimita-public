// 09 12 2026
/* purpose
* Implements the entity identity map and component-store teardown.
* Does NOT implement component access templates (header) or gameplay systems.
*/
#include "ecs/entity-registry.h"

#include "ecs/dynamic-components.h"
#include "ecs/relationship-store.h"
#include "live-code/live-journal.h"

namespace {

void journalEntity(const char* type, EntityId id, const EntityIdentity& identity)
{
    LiveEventJournal::Fields fields;
    fields.actorId = entityDomainName(identity.domain);
    fields.result = entityRealmName(identity.realm);
    fields.extra = "\"entity_id\":" + std::to_string(id) +
                   ",\"legacy_id\":" + std::to_string(identity.legacyId) +
                   ",\"domain\":\"" + entityDomainName(identity.domain) +
                   "\",\"realm\":\"" + entityRealmName(identity.realm) + "\"";
    LiveEventJournal::instance().record(type, fields);
}

} // namespace

EntityRegistry& EntityRegistry::instance()
{
    static EntityRegistry registry;
    return registry;
}

EntityId EntityRegistry::create(EntityRealm realm, EntityDomain domain,
                                std::uint32_t legacyId, std::uint16_t generation)
{
    const std::uint64_t key = entityLookupKey(realm, domain, legacyId);
    auto found = mLookup.find(key);
    if (found != mLookup.end() && alive(found->second))
        return found->second;

    const EntityId id = makeEntityId(realm, domain, legacyId, generation);
    EntityIdentity identity;
    identity.realm = realm;
    identity.domain = domain;
    identity.legacyId = legacyId;
    identity.generation = generation;
    mIdentities[id] = identity;
    mLookup[key] = id;
    journalEntity("entity_registered", id, identity);
    return id;
}

EntityId EntityRegistry::createGeneric(EntityRealm realm, std::uint32_t legacyId)
{
    if (legacyId == 0) {
        do {
            legacyId = nextDynamicId_++;
        } while (alive(find(realm, EntityDomain::None, legacyId)));
    }
    return create(realm, EntityDomain::None, legacyId);
}

std::uint32_t EntityRegistry::allocateLegacyId(EntityRealm realm,
                                               EntityDomain domain)
{
    const std::uint64_t key = entityLookupKey(realm, domain, 0);
    std::uint32_t& next = mNextLegacyId[key];
    if (next == 0)
        next = 1000;  // stable base for the first typed actor id
    for (;;) {
        const std::uint32_t candidate = next++;
        if (candidate == 0)
            continue;
        if (mLookup.find(entityLookupKey(realm, domain, candidate)) == mLookup.end())
            return candidate;
    }
}

EntityId EntityRegistry::adopt(EntityId id)
{
    if (id == kInvalidEntityId)
        return id;
    if (alive(id))
        return id;
    const EntityRealm realm = entityRealm(id);
    const EntityDomain domain = entityDomain(id);
    const std::uint32_t legacyId = entityLegacyId(id);
    const std::uint16_t generation = entityGeneration(id);
    const std::uint64_t key = entityLookupKey(realm, domain, legacyId);
    auto found = mLookup.find(key);
    if (found != mLookup.end() && alive(found->second) && found->second != id)
        destroy(found->second);  // generation reuse: retire the old identity

    EntityIdentity identity;
    identity.realm = realm;
    identity.domain = domain;
    identity.legacyId = legacyId;
    identity.generation = generation;
    mIdentities[id] = identity;
    mLookup[key] = id;
    journalEntity("entity_adopted", id, identity);
    return id;
}

void EntityRegistry::destroy(EntityId id)
{
    if (id == kInvalidEntityId)
        return;
    auto it = mIdentities.find(id);
    if (it == mIdentities.end())
        return;
    journalEntity("entity_destroyed", id, it->second);
    mLookup.erase(entityLookupKey(it->second.realm, it->second.domain, it->second.legacyId));
    mIdentities.erase(it);
    mDestroyed.push_back(id);
    for (auto& entry : mStores)
        entry.second->erase(id);
    // Entity lifetime owns every component storage: purge the dynamic component
    // blobs and relationship edges that reference this entity.
    MimitaRuntime::DynamicComponentStore::instance().eraseEntity(id);
    MimitaRuntime::RelationshipStore::instance().eraseEntity(id);
}

std::vector<EntityId> EntityRegistry::consumeDestroyed()
{
    std::vector<EntityId> out = std::move(mDestroyed);
    mDestroyed.clear();
    return out;
}

void EntityRegistry::destroyAll()
{
    mIdentities.clear();
    mLookup.clear();
    mDestroyed.clear();
    for (auto& entry : mStores)
        entry.second = nullptr;
    mStores.clear();
}

bool EntityRegistry::alive(EntityId id) const
{
    return id != kInvalidEntityId && mIdentities.find(id) != mIdentities.end();
}

EntityId EntityRegistry::find(EntityRealm realm, EntityDomain domain, std::uint32_t legacyId) const
{
    auto it = mLookup.find(entityLookupKey(realm, domain, legacyId));
    return it == mLookup.end() ? kInvalidEntityId : it->second;
}

const EntityIdentity* EntityRegistry::identity(EntityId id) const
{
    auto it = mIdentities.find(id);
    return it == mIdentities.end() ? nullptr : &it->second;
}

std::vector<EntityId> EntityRegistry::all() const
{
    std::vector<EntityId> ids;
    ids.reserve(mIdentities.size());
    for (const auto& entry : mIdentities)
        ids.push_back(entry.first);
    return ids;
}
