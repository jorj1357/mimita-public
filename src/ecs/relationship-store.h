// 09 14 2026
/* purpose
* Kernel-owned generic relationship storage: typed directed edges between
* entities, keyed by a 64-bit relationship id (gameHash("relationship.name")).
* One generic containment/ownership/reference mechanism so tools, inventory,
* vehicles, and gamemodes do not each add a parallel edge structure or enum.
* Owns per-type replication policy and generic change markers so replication
* consumes edges generically (no item-/equip-specific networking).
* Does NOT own gameplay policy or rendering.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "ecs/entity-types.h"

namespace MimitaRuntime {

// One directed edge plus the generic change marker replication consumes.
struct RelationshipEdge {
    EntityId from = kInvalidEntityId;
    EntityId to = kInvalidEntityId;
    std::uint64_t value = 0;
    std::uint32_t changeVersion = 0;
};

class RelationshipStore {
public:
    static RelationshipStore& instance();

    void clear();

    // Adds or updates the (typeId, from -> to) edge. Returns false only for an
    // invalid id. Every change bumps the edge's generic changeVersion and, on
    // first use of a type, marks that type replicable.
    bool add(std::uint64_t typeId, EntityId from, EntityId to, std::uint64_t value);
    // Applies a replicated edge and adopts the sender's changeVersion so a
    // replayed/stale record can be rejected by the receiver.
    bool addVersioned(std::uint64_t typeId, EntityId from, EntityId to,
                      std::uint64_t value, std::uint32_t changeVersion);
    bool remove(std::uint64_t typeId, EntityId from, EntityId to);

    // Deterministic (sorted by target id) outgoing query.
    std::size_t query(std::uint64_t typeId, EntityId from, EntityId* outTo,
                      std::uint64_t* outValue, std::size_t maxOut) const;
    // Deterministic incoming query.
    std::size_t queryReverse(std::uint64_t typeId, EntityId to, EntityId* outFrom,
                             std::size_t maxOut) const;
    bool has(std::uint64_t typeId, EntityId from, EntityId to) const;

    // Deterministic (sorted by from then to) full enumerate for one type.
    std::size_t edgesOfType(std::uint64_t typeId, RelationshipEdge* out,
                            std::size_t maxOut) const;
    std::uint32_t changeVersionOf(std::uint64_t typeId, EntityId from,
                                  EntityId to) const;

    // Drops every edge that references the entity on either side.
    void eraseEntity(EntityId entity);

    std::size_t edgeCount() const;

    // Generic replication policy per relationship type (GAME_NET_*). A type
    // becomes replicable the first time an edge of that type is added, so a
    // relationship type unknown at startup becomes network-visible with no cold
    // registration. An explicit policy may override it.
    void setNetworkPolicy(std::uint64_t typeId, std::uint32_t policy);
    std::uint32_t networkPolicy(std::uint64_t typeId) const;
    std::vector<std::uint64_t> typeIds() const;

private:
    RelationshipStore() = default;

    struct Edge {
        EntityId from = kInvalidEntityId;
        EntityId to = kInvalidEntityId;
        std::uint64_t value = 0;
        std::uint32_t changeVersion = 0;
    };

    std::unordered_map<std::uint64_t, std::vector<Edge>> edges_;
    std::unordered_map<std::uint64_t, std::uint32_t> policies_;
    std::uint32_t revision_ = 0;
};

} // namespace MimitaRuntime
