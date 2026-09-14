// 09 13 2026
/* purpose
* One reusable entity/component/constraint/resource/telemetry reading API,
* independent of the terminal and GUI. The terminal, a future panel, and AI
* tooling all consume this same data.
* Does NOT mutate state or own an editor.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ecs/components.h"
#include "ecs/entity-types.h"
#include "physics/constraints/constraint-components.h"

namespace Editor {

struct EntityInspection {
    EntityId id = kInvalidEntityId;
    bool alive = false;
    EntityRealm realm = EntityRealm::Server;
    EntityDomain domain = EntityDomain::None;
    std::uint32_t legacyId = 0;
    std::uint16_t generation = 0;
    std::string label;

    EntityId parent = kInvalidEntityId;
    bool hasTransform = false;
    TransformComponent transform;

    std::vector<std::string> components;
    std::vector<BehaviorBinding> behaviors;

    bool hasWorldObject = false;
    WorldObjectComponent worldObject;

    bool hasConstraint = false;
    Physics::ConstraintComponent constraint;
    // For a grab hand limb: the linked networked constraint serial.
    std::uint32_t linkedConstraintSerial = 0;

    std::string toText() const;
    std::string toJson() const;
};

EntityInspection inspectEntity(EntityId id);

} // namespace Editor
