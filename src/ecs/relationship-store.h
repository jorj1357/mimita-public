// 09 14 2026
/* purpose
* Kernel-owned generic relationship storage: typed directed edges between
* entities, keyed by a 64-bit relationship id (gameHash("relationship.name")).
* One generic containment/ownership/reference mechanism so tools, inventory,
* vehicles, and gamemodes do not each add a parallel edge structure or enum.
* Does NOT own gameplay policy, rendering, or replication.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "ecs/entity-types.h"

namespace MimitaRuntime {

class RelationshipStore {
public:
    static RelationshipStore& instance();

    void clear();

    // Adds or updates the (typeId, from -> to) edge. Returns false only for an
    // invalid id.
    bool add(std::uint64_t typeId, EntityId from, EntityId to, std::uint64_t value);
    bool remove(std::uint64_t typeId, EntityId from, EntityId to);

    // Deterministic (sorted by target id) outgoing query.
    std::size_t query(std::uint64_t typeId, EntityId from, EntityId* outTo,
                      std::uint64_t* outValue, std::size_t maxOut) const;
    // Deterministic incoming query.
    std::size_t queryReverse(std::uint64_t typeId, EntityId to, EntityId* outFrom,
                             std::size_t maxOut) const;
    bool has(std::uint64_t typeId, EntityId from, EntityId to) const;

    // Drops every edge that references the entity on either side.
    void eraseEntity(EntityId entity);

    std::size_t edgeCount() const;

private:
    RelationshipStore() = default;

    struct Edge {
        EntityId from = kInvalidEntityId;
        EntityId to = kInvalidEntityId;
        std::uint64_t value = 0;
    };

    std::unordered_map<std::uint64_t, std::vector<Edge>> edges_;
};

} // namespace MimitaRuntime
