// 09 14 2026
/* purpose
* Kernel-owned generic predicted -> authoritative entity association. A
* prediction key (for example a request id) links a locally predicted provisional
* entity to the authoritative replicated entity that supersedes it. On
* association the provisional identity is retired so there is exactly one
* canonical entity and one visual. Type-agnostic: projectiles, predicted items,
* authoritative effects, and future predicted entities all use it.
* Does NOT own prediction/interpolation math, networking, or presentation.
*/
#pragma once

#include <cstdint>
#include <unordered_map>

#include "ecs/entity-types.h"

namespace Ecs {

enum PredictionStatus : std::uint32_t {
    PREDICTION_PENDING = 0,     // provisional only
    PREDICTION_ASSOCIATED = 1,  // authoritative is canonical
    PREDICTION_RETIRED = 2,     // provisional retired, no authority yet
};

struct PredictionAssociation {
    std::uint64_t key = 0;
    EntityId provisional = kInvalidEntityId;
    EntityId authoritative = kInvalidEntityId;
    std::uint32_t status = PREDICTION_PENDING;
    std::uint64_t tick = 0;
};

class PredictionRegistry {
public:
    static PredictionRegistry& instance();

    void clear();

    // Register a provisional entity for a key. If the authoritative entity is
    // already known, the provisional is retired immediately and the
    // authoritative entity is returned. Re-registering a key retires the old
    // provisional.
    EntityId registerProvisional(std::uint64_t key, EntityId provisional,
                                 std::uint64_t tick);

    // Record the authoritative entity for a key. A live provisional is retired.
    // Returns the canonical entity (authoritative, or the key's provisional).
    EntityId associate(std::uint64_t key, EntityId authoritative,
                       std::uint64_t tick);

    // Canonical entity: authoritative when known and alive, else the live
    // provisional, else invalid.
    EntityId canonical(std::uint64_t key) const;
    EntityId provisionalOf(std::uint64_t key) const;
    EntityId authoritativeOf(std::uint64_t key) const;
    PredictionStatus statusOf(std::uint64_t key) const;

    // Keep associations from dangling: clears the matching side, falling back to
    // the surviving side or dropping the association.
    void onEntityDestroyed(EntityId entity);

    // Retire provisional entities whose pending association is older than `tick`.
    void pruneOlderThan(std::uint64_t tick);

    std::size_t count() const { return associations_.size(); }

private:
    std::unordered_map<std::uint64_t, PredictionAssociation> associations_;
};

} // namespace Ecs
