// 09 13 2026
/* purpose
* Implements the reusable entity inspection API.
* Does NOT mutate state.
*/
#include "editor/entity-inspector.h"

#include <cstdio>
#include <sstream>

#include "ecs/entity-registry.h"
#include "hot-reload/hot-reload-system.h"
#include "physics/constraints/constraint-store.h"
#include "ragdoll/ragdoll-components.h"
#include "telemetry/telemetry.h"

namespace Editor {

namespace {

void addIf(bool present, const char* name, std::vector<std::string>& out)
{
    if (present)
        out.push_back(name);
}

} // namespace

EntityInspection inspectEntity(EntityId id)
{
    EntityInspection out;
    out.id = id;
    EntityRegistry& registry = EntityRegistry::instance();
    const EntityIdentity* identity = registry.identity(id);
    out.alive = identity != nullptr;
    if (!identity)
        return out;

    out.realm = identity->realm;
    out.domain = identity->domain;
    out.legacyId = identity->legacyId;
    out.generation = identity->generation;
    out.label = std::string(entityDomainName(identity->domain)) + ":" +
                std::to_string(identity->legacyId);

    if (const auto* t = registry.tryGet<TransformComponent>(id)) {
        out.hasTransform = true;
        out.transform = *t;
    }
    if (const auto* owner = registry.tryGet<OwnerComponent>(id))
        out.parent = owner->owner;

    std::vector<std::string>& c = out.components;
    addIf(registry.has<TransformComponent>(id), "Transform", c);
    addIf(registry.has<VelocityComponent>(id), "Velocity", c);
    addIf(registry.has<BodyComponent>(id), "Body", c);
    addIf(registry.has<HealthComponent>(id), "Health", c);
    addIf(registry.has<ControlSourceComponent>(id), "ControlSource", c);
    addIf(registry.has<NetworkAuthorityComponent>(id), "NetworkAuthority", c);
    addIf(registry.has<MovementIntentComponent>(id), "MovementIntent", c);
    addIf(registry.has<AimIntentComponent>(id), "AimIntent", c);
    addIf(registry.has<FireIntentComponent>(id), "FireIntent", c);
    addIf(registry.has<WeaponInventoryComponent>(id), "WeaponInventory", c);
    addIf(registry.has<ProjectileComponent>(id), "Projectile", c);
    addIf(registry.has<OwnerComponent>(id), "Owner", c);
    addIf(registry.has<ColliderComponent>(id), "Collider", c);
    addIf(registry.has<WorldObjectComponent>(id), "WorldObject", c);
    addIf(registry.has<BehaviorBindingsComponent>(id), "BehaviorBindings", c);
    addIf(registry.has<Ragdoll::LimbComponent>(id), "RagdollLimb", c);
    addIf(registry.has<Ragdoll::JointComponent>(id), "RagdollJoint", c);
    addIf(registry.has<Ragdoll::GrabComponent>(id), "RagdollGrab", c);
    addIf(registry.has<Ragdoll::RagdollRootComponent>(id), "RagdollRoot", c);
    addIf(registry.has<Physics::ConstraintComponent>(id), "Constraint", c);

    if (const auto* bindings = registry.tryGet<BehaviorBindingsComponent>(id)) {
        for (int i = 0; i < bindings->count; ++i)
            out.behaviors.push_back(bindings->bindings[i]);
    }
    if (const auto* wo = registry.tryGet<WorldObjectComponent>(id)) {
        out.hasWorldObject = true;
        out.worldObject = *wo;
    }
    if (const auto* cc = registry.tryGet<Physics::ConstraintComponent>(id)) {
        out.hasConstraint = true;
        out.constraint = *cc;
    }
    if (const auto* grab = registry.tryGet<Ragdoll::GrabComponent>(id))
        out.linkedConstraintSerial = grab->constraintSerial;

    return out;
}

std::string EntityInspection::toText() const
{
    std::ostringstream o;
    o << "[ENTITY]\n";
    o << "  id=" << id << " alive=" << (alive ? 1 : 0) << "\n";
    o << "  realm=" << entityRealmName(realm) << " domain=" << entityDomainName(domain)
      << " legacyId=" << legacyId << " generation=" << generation << "\n";
    o << "  label=" << label;
    if (parent != kInvalidEntityId)
        o << " owner=" << parent;
    o << "\n";

    if (hasTransform) {
        o << "  transform.pos=(" << transform.position.x << ", " << transform.position.y
          << ", " << transform.position.z << ") yaw=" << transform.yaw
          << " pitch=" << transform.pitch << "\n";
    }

    o << "[COMPONENTS]\n";
    if (components.empty())
        o << "  (none)\n";
    else
        for (const std::string& name : components)
            o << "  " << name << "\n";

    o << "[BEHAVIORS]\n";
    if (behaviors.empty())
        o << "  (none)\n";
    else
        for (const BehaviorBinding& b : behaviors)
            o << "  event=" << b.eventType << " behaviorId=" << b.behaviorId
              << " codeHash=" << b.codeHash << " generation=" << b.generation << "\n";

    o << "[RESOURCES]\n";
    if (hasWorldObject)
        o << "  sourceIndex=" << worldObject.sourceIndex
          << " nodeIndex=" << worldObject.nodeIndex
          << " path=" << worldObject.sourcePath
          << " meshHash=" << worldObject.meshHash
          << " materialHash=" << worldObject.materialHash << "\n";
    else
        o << "  (none)\n";

    o << "[NETWORK]\n";
    if (const auto* authority = EntityRegistry::instance().tryGet<NetworkAuthorityComponent>(id))
        o << "  authority=" << (int)authority->authority << "\n";
    o << "  realm=" << entityRealmName(realm) << "\n";

    o << "[CONSTRAINT]\n";
    if (hasConstraint) {
        const Physics::Constraint& c = constraint.constraint;
        o << "  serial=" << constraint.constraintSerial
          << " owner=" << constraint.ownerActor
          << " type=" << (int)c.type
          << " active=" << (c.active ? 1 : 0)
          << " released=" << (constraint.released ? 1 : 0) << "\n";
        o << "  bodyA=" << c.bodyA << " limbA=" << c.limbA
          << " anchorA=(" << c.anchorA.x << ", " << c.anchorA.y << ", " << c.anchorA.z << ")\n";
        o << "  bodyB=" << c.bodyB << " limbB=" << c.limbB
          << " anchorB=(" << c.anchorB.x << ", " << c.anchorB.y << ", " << c.anchorB.z << ")\n";
        o << "  worldPoint=(" << c.worldPoint.x << ", " << c.worldPoint.y
          << ", " << c.worldPoint.z << ")"
          << " strength=" << c.strength << " damping=" << c.damping
          << " min=" << c.minDistance << " max=" << c.maxDistance << "\n";
        o << "  createdTick=" << constraint.createdTick
          << " releaseTick=" << constraint.releaseTick << "\n";
    } else if (linkedConstraintSerial != 0) {
        o << "  linkedSerial=" << linkedConstraintSerial << "\n";
    } else {
        o << "  (none)\n";
    }

    o << "[TELEMETRY]\n";
    o << "  " << Telemetry::Registry::instance().entityJson(id) << "\n";
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    o << "  generation=" << status.activeGeneration << " codeHash=" << status.activeHash << "\n";
    return o.str();
}

std::string EntityInspection::toJson() const
{
    std::ostringstream o;
    o << "{\"id\":" << id << ",\"alive\":" << (alive ? 1 : 0)
      << ",\"realm\":\"" << entityRealmName(realm) << "\""
      << ",\"domain\":\"" << entityDomainName(domain) << "\""
      << ",\"legacyId\":" << legacyId
      << ",\"generation\":" << generation;
    if (hasTransform)
        o << ",\"pos\":[" << transform.position.x << "," << transform.position.y << ","
          << transform.position.z << "]";
    if (hasConstraint)
        o << ",\"constraintSerial\":" << constraint.constraintSerial
          << ",\"constraintActive\":" << (constraint.constraint.active ? 1 : 0);
    o << "}";
    return o.str();
}

} // namespace Editor
