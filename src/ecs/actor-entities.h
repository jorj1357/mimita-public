// 09 12 2026
/* purpose
* Transitional adapters that project legacy Player/NPC/projectile state into
* entity components. Call sites keep their existing structs; these helpers make
* identity, control source, intent, and owner entity-driven.
* Does NOT own gameplay systems or replace the legacy structs yet.
*/
#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "ecs/components.h"
#include "ecs/entity-types.h"

namespace Ecs {

// Create or find the stable entity for a legacy id.
EntityId ensure(EntityRealm realm, EntityDomain domain, std::uint32_t legacyId);

// The single locally-controlled human player entity (stable across reloads).
EntityId ensureLocalPlayerEntity();

// Control source is journaled only when it changes.
void setControlSource(EntityId id, ControlSource source);
void setAuthority(EntityId id, NetworkAuthority authority);

void setTransform(EntityId id, const glm::vec3& position, const glm::vec3& look,
                  float yaw, float pitch);
void setVelocity(EntityId id, const glm::vec3& linear, const glm::vec3& externalImpulse);
void setHealth(EntityId id, int current, int max, bool dead);
void setBody(EntityId id, float sizeScale, float radius, float height);
void setMovementIntent(EntityId id, float moveX, float moveY, bool pressed,
                       bool jump, bool dash, bool downDash, bool freeze);
void setAimIntent(EntityId id, const glm::vec3& direction, float yaw, float pitch);
void setWeaponInventory(EntityId id, std::uint32_t equippedNetworkId, std::uint32_t slot);

// Register a projectile entity and journal its owner resolution.
EntityId spawnRocket(EntityRealm realm, std::uint32_t legacyId, EntityId owner,
                     const glm::vec3& position, const glm::vec3& velocity,
                     std::uint32_t weaponDefNetworkId, std::uint32_t fireSerial,
                     float lifetime, NetworkAuthority authority);
void setRocketMotion(EntityId id, const glm::vec3& position, const glm::vec3& velocity);
void despawn(EntityId id);

// Journal a damage application against a victim entity with the attacker owner.
void journalDamage(EntityId victim, EntityId attacker, float amount, const char* source);

// Packed EntityId for evidence fields.
std::uint64_t raw(EntityId id);

} // namespace Ecs
