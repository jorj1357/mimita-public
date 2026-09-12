// 09 12 2026
/* purpose
* Implements the transitional entity adapters and their journal evidence.
* Does NOT own gameplay systems or the legacy structs it projects.
*/
#include "ecs/actor-entities.h"

#include "ecs/entity-registry.h"
#include "live-code/live-journal.h"

#include <string>

namespace {

std::string entityField(const char* name, EntityId id)
{
    return std::string("\"") + name + "\":" + std::to_string(static_cast<unsigned long long>(id));
}

} // namespace

namespace Ecs {

std::uint64_t raw(EntityId id)
{
    return static_cast<std::uint64_t>(id);
}

EntityId ensure(EntityRealm realm, EntityDomain domain, std::uint32_t legacyId)
{
    return EntityRegistry::instance().create(realm, domain, legacyId);
}

EntityId ensureLocalPlayerEntity()
{
    const EntityId id = ensure(EntityRealm::Local, EntityDomain::Player, 1);
    setControlSource(id, ControlSource::LocalHuman);
    setAuthority(id, NetworkAuthority::ClientPredicted);
    return id;
}

void setControlSource(EntityId id, ControlSource source)
{
    auto& registry = EntityRegistry::instance();
    auto* existing = registry.tryGet<ControlSourceComponent>(id);
    if (existing && existing->source == source)
        return;

    registry.add<ControlSourceComponent>(id, ControlSourceComponent{source});

    LiveEventJournal::Fields fields;
    fields.actorId = controlSourceName(source);
    fields.extra = entityField("entity_id", id) + ",\"control_source\":\"" +
                   controlSourceName(source) + "\"";
    LiveEventJournal::instance().record("control_source_set", fields);
}

void setAuthority(EntityId id, NetworkAuthority authority)
{
    EntityRegistry::instance().add<NetworkAuthorityComponent>(
        id, NetworkAuthorityComponent{authority});
}

void setTransform(EntityId id, const glm::vec3& position, const glm::vec3& look,
                  float yaw, float pitch)
{
    TransformComponent transform;
    transform.position = position;
    transform.look = look;
    transform.yaw = yaw;
    transform.pitch = pitch;
    EntityRegistry::instance().add<TransformComponent>(id, transform);
}

void setVelocity(EntityId id, const glm::vec3& linear, const glm::vec3& externalImpulse)
{
    VelocityComponent velocity;
    velocity.linear = linear;
    velocity.externalImpulse = externalImpulse;
    EntityRegistry::instance().add<VelocityComponent>(id, velocity);
}

void setHealth(EntityId id, int current, int max, bool dead)
{
    HealthComponent health;
    health.current = current;
    health.max = max;
    health.dead = dead;
    EntityRegistry::instance().add<HealthComponent>(id, health);
}

void setBody(EntityId id, float sizeScale, float radius, float height)
{
    BodyComponent body;
    body.sizeScale = sizeScale;
    body.radius = radius;
    body.height = height;
    EntityRegistry::instance().add<BodyComponent>(id, body);
}

void setMovementIntent(EntityId id, float moveX, float moveY, bool pressed,
                       bool jump, bool dash, bool downDash, bool freeze)
{
    MovementIntentComponent intent;
    intent.moveX = moveX;
    intent.moveY = moveY;
    intent.pressed = pressed;
    intent.jump = jump;
    intent.dash = dash;
    intent.downDash = downDash;
    intent.freeze = freeze;
    EntityRegistry::instance().add<MovementIntentComponent>(id, intent);
}

void setAimIntent(EntityId id, const glm::vec3& direction, float yaw, float pitch)
{
    AimIntentComponent intent;
    intent.direction = direction;
    intent.yaw = yaw;
    intent.pitch = pitch;
    EntityRegistry::instance().add<AimIntentComponent>(id, intent);
}

void setWeaponInventory(EntityId id, std::uint32_t equippedNetworkId, std::uint32_t slot)
{
    WeaponInventoryComponent inventory;
    inventory.equippedNetworkId = equippedNetworkId;
    inventory.slot = slot;
    EntityRegistry::instance().add<WeaponInventoryComponent>(id, inventory);
}

EntityId spawnRocket(EntityRealm realm, std::uint32_t legacyId, EntityId owner,
                     const glm::vec3& position, const glm::vec3& velocity,
                     std::uint32_t weaponDefNetworkId, std::uint32_t fireSerial,
                     float lifetime, NetworkAuthority authority)
{
    EntityId id = ensure(realm, EntityDomain::Projectile, legacyId);
    setTransform(id, position, glm::vec3(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
    setVelocity(id, velocity, glm::vec3(0.0f));
    setAuthority(id, authority);

    OwnerComponent ownerComponent;
    ownerComponent.owner = owner;
    EntityRegistry::instance().add<OwnerComponent>(id, ownerComponent);

    ProjectileComponent projectile;
    projectile.weaponDefNetworkId = weaponDefNetworkId;
    projectile.fireSerial = fireSerial;
    projectile.lifetime = lifetime;
    EntityRegistry::instance().add<ProjectileComponent>(id, projectile);

    ColliderComponent collider;
    EntityRegistry::instance().add<ColliderComponent>(id, collider);

    LiveEventJournal::Fields fields;
    fields.projectileId = std::to_string(legacyId);
    fields.extra = entityField("entity_id", id) + "," +
                   entityField("owner_entity_id", owner) +
                   ",\"fire_serial\":" + std::to_string(fireSerial) +
                   ",\"weapon_network_id\":" + std::to_string(weaponDefNetworkId);
    LiveEventJournal::instance().record("rocket_entity_spawned", fields);
    return id;
}

void setRocketMotion(EntityId id, const glm::vec3& position, const glm::vec3& velocity)
{
    if (!EntityRegistry::instance().alive(id))
        return;
    auto* transform = EntityRegistry::instance().tryGet<TransformComponent>(id);
    if (transform)
        transform->position = position;
    auto* vel = EntityRegistry::instance().tryGet<VelocityComponent>(id);
    if (vel)
        vel->linear = velocity;
}

void despawn(EntityId id)
{
    EntityRegistry::instance().destroy(id);
}

void journalDamage(EntityId victim, EntityId attacker, float amount, const char* source)
{
    LiveEventJournal::Fields fields;
    fields.result = source ? source : "";
    fields.extra = entityField("entity_id", victim) + "," +
                   entityField("owner_entity_id", attacker) +
                   ",\"amount\":" + std::to_string(amount);
    LiveEventJournal::instance().record("damage_applied", fields);
}

} // namespace Ecs
