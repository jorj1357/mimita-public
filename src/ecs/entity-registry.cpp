// 09 12 2026
/* purpose
* Implements the entity identity map and component-store teardown.
* Does NOT implement component access templates (header) or gameplay systems.
*/
#include "ecs/entity-registry.h"

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
    for (auto& entry : mStores)
        entry.second->erase(id);
}

void EntityRegistry::destroyAll()
{
    mIdentities.clear();
    mLookup.clear();
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
