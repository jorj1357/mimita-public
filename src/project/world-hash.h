// 09 12 2026
/* purpose
* Deterministic world-state hashing over entity identity and known components.
* Used for snapshots, rollback comparison, and peer agreement.
* Does NOT mutate state or own the registry.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ecs/entity-types.h"

class EntityRegistry;

namespace Project {

class WorldHash {
public:
    // Hash identity plus the component set below for the given entities, in a
    // stable order. Empty `ids` means all live entities.
    static std::string hashEntities(const EntityRegistry& registry,
                                    const std::vector<EntityId>& ids = {});
};

} // namespace Project
