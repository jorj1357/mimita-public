// 09 13 2026
/* purpose
* Canonical active-constraint set: one dedicated Constraint entity per serial,
* server-authoritative serial allocation, release tombstones for reordering.
* Single owner of network-visible constraint identity for the inspector,
* solver, and replication.
* Does NOT solve constraints or own networking transport.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/entity-types.h"
#include "physics/constraints/constraint-components.h"

namespace Physics {

class ConstraintStore {
public:
    static ConstraintStore& instance();

    // Creates (or replaces) the dedicated Constraint entity for `serial` and
    // stores the component. Returns the stable entity id.
    EntityId create(EntityRealm realm, const ConstraintComponent& component);

    // Marks a serial released: destroys the entity, records a bounded tombstone
    // so an out-of-order create is dropped. Returns false if not active.
    bool release(std::uint32_t serial, std::uint32_t releaseTick, std::uint8_t reason);

    EntityId find(std::uint32_t serial) const;
    bool active(std::uint32_t serial) const;
    bool tombstoned(std::uint32_t serial) const;
    const ConstraintComponent* component(std::uint32_t serial) const;

    std::vector<std::uint32_t> activeSerials() const;
    std::vector<std::uint32_t> activeSerialsForOwner(std::uint32_t ownerActorId) const;

    // Owner-scoped serial: high 16 bits = owner, low 16 = per-owner counter.
    // Client prediction allocates locally; the server validates uniqueness.
    std::uint32_t allocateSerial(std::uint32_t ownerActorId);

    void clear();
    std::size_t activeCount() const { return bySerial_.size(); }

private:
    ConstraintStore() = default;

    std::unordered_map<std::uint32_t, EntityId> bySerial_;
    std::unordered_set<std::uint32_t> released_;
    std::deque<std::uint32_t> releasedOrder_;
    std::unordered_map<std::uint32_t, std::uint16_t> ownerCounter_;

    static constexpr std::size_t kMaxTombstones = 256;
};

} // namespace Physics
