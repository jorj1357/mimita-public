// 09 13 2026
/* purpose
* Implements the canonical active-constraint set.
* Does NOT solve constraints or own networking transport.
*/
#include "physics/constraints/constraint-store.h"

#include "ecs/entity-registry.h"

namespace Physics {

ConstraintStore& ConstraintStore::instance()
{
    static ConstraintStore store;
    return store;
}

EntityId ConstraintStore::create(EntityRealm realm, const ConstraintComponent& component)
{
    // An out-of-order create for an already-released serial is dropped.
    if (component.constraintSerial != 0 && tombstoned(component.constraintSerial))
        return kInvalidEntityId;
    EntityRegistry& registry = EntityRegistry::instance();
    const EntityId id =
        registry.create(realm, EntityDomain::Constraint, component.constraintSerial);
    registry.add<ConstraintComponent>(id, component);
    bySerial_[component.constraintSerial] = id;
    return id;
}

bool ConstraintStore::release(std::uint32_t serial, std::uint32_t releaseTick,
                              std::uint8_t reason)
{
    auto it = bySerial_.find(serial);
    if (it == bySerial_.end()) {
        // Not active (release before create, or already gone): record a
        // tombstone so a later out-of-order create is dropped.
        released_.insert(serial);
        releasedOrder_.push_back(serial);
        while (releasedOrder_.size() > kMaxTombstones) {
            released_.erase(releasedOrder_.front());
            releasedOrder_.pop_front();
        }
        return false;
    }
    if (const auto* c = EntityRegistry::instance().tryGet<ConstraintComponent>(it->second)) {
        ConstraintComponent updated = *c;
        updated.released = true;
        updated.releaseTick = releaseTick;
        (void)reason;
        EntityRegistry::instance().add<ConstraintComponent>(it->second, updated);
    }
    EntityRegistry::instance().destroy(it->second);
    bySerial_.erase(it);

    released_.insert(serial);
    releasedOrder_.push_back(serial);
    while (releasedOrder_.size() > kMaxTombstones) {
        released_.erase(releasedOrder_.front());
        releasedOrder_.pop_front();
    }
    return true;
}

EntityId ConstraintStore::find(std::uint32_t serial) const
{
    auto it = bySerial_.find(serial);
    return it == bySerial_.end() ? kInvalidEntityId : it->second;
}

bool ConstraintStore::active(std::uint32_t serial) const
{
    return bySerial_.find(serial) != bySerial_.end();
}

bool ConstraintStore::tombstoned(std::uint32_t serial) const
{
    return released_.find(serial) != released_.end();
}

const ConstraintComponent* ConstraintStore::component(std::uint32_t serial) const
{
    auto it = bySerial_.find(serial);
    if (it == bySerial_.end())
        return nullptr;
    return EntityRegistry::instance().tryGet<ConstraintComponent>(it->second);
}

std::vector<std::uint32_t> ConstraintStore::activeSerials() const
{
    std::vector<std::uint32_t> out;
    out.reserve(bySerial_.size());
    for (const auto& entry : bySerial_)
        out.push_back(entry.first);
    return out;
}

std::vector<std::uint32_t> ConstraintStore::activeSerialsForOwner(
    std::uint32_t ownerActorId) const
{
    std::vector<std::uint32_t> out;
    for (const auto& entry : bySerial_) {
        const auto* c = EntityRegistry::instance().tryGet<ConstraintComponent>(entry.second);
        if (c && c->ownerActor == ownerActorId)
            out.push_back(entry.first);
    }
    return out;
}

std::uint32_t ConstraintStore::allocateSerial(std::uint32_t ownerActorId)
{
    std::uint16_t& counter = ownerCounter_[ownerActorId];
    ++counter;
    if (counter == 0)
        ++counter;
    const std::uint32_t serial =
        (ownerActorId << 16) | static_cast<std::uint32_t>(counter);
    return serial ? serial : 1u;
}

void ConstraintStore::clear()
{
    EntityRegistry& registry = EntityRegistry::instance();
    for (const auto& entry : bySerial_)
        registry.destroy(entry.second);
    bySerial_.clear();
    released_.clear();
    releasedOrder_.clear();
    ownerCounter_.clear();
}

} // namespace Physics
