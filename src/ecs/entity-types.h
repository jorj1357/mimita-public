// 09 12 2026
/* purpose
* Define the stable EntityId and its packed realm/domain/legacy layout.
* Reuses existing repository identity spaces (playerId, Npc::id,
* ServerNpc::entityId, ServerProjectile::id, fireSerial) instead of inventing a
* parallel identifier space.
* Does NOT store components, own systems, or know about gameplay behavior.
*/
#pragma once

#include <cstdint>

using EntityId = std::uint64_t;

static constexpr EntityId kInvalidEntityId = 0;

// Where the entity lives. The host process runs server and client together, so
// realm prevents server entities from colliding with predicted/replicated ones.
enum class EntityRealm : std::uint8_t {
    Server = 0,
    ClientPredicted = 1,
    ClientReplicated = 2,
    Local = 3,
};

// What capability family the entity belongs to. This is metadata, not a root
// type: an actor is a player/npc entity that carries the required components.
enum class EntityDomain : std::uint8_t {
    None = 0,
    Player = 1,
    Npc = 2,
    Projectile = 3,
    WorldObject = 4,
};

// Bit layout (little-endian value):
//   bits  0..31  legacy id
//   bits 32..39  domain
//   bits 40..43  realm
//   bits 44..59  generation (0 for the vertical slice; reserved for respawn)
inline EntityId makeEntityId(EntityRealm realm, EntityDomain domain,
                             std::uint32_t legacyId, std::uint16_t generation = 0)
{
    return (static_cast<EntityId>(generation & 0xffffu) << 44)
         | (static_cast<EntityId>(static_cast<std::uint8_t>(realm) & 0x0fu) << 40)
         | (static_cast<EntityId>(static_cast<std::uint8_t>(domain) & 0xffu) << 32)
         | static_cast<EntityId>(legacyId);
}

inline std::uint32_t entityLegacyId(EntityId id) { return static_cast<std::uint32_t>(id & 0xffffffffu); }
inline EntityDomain entityDomain(EntityId id) { return static_cast<EntityDomain>((id >> 32) & 0xffu); }
inline EntityRealm entityRealm(EntityId id) { return static_cast<EntityRealm>((id >> 40) & 0x0fu); }
inline std::uint16_t entityGeneration(EntityId id) { return static_cast<std::uint16_t>((id >> 44) & 0xffffu); }

// Stable hash key for lookups, independent of generation.
inline std::uint64_t entityLookupKey(EntityRealm realm, EntityDomain domain, std::uint32_t legacyId)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint8_t>(realm)) << 40)
         | (static_cast<std::uint64_t>(static_cast<std::uint8_t>(domain)) << 32)
         | static_cast<std::uint64_t>(legacyId);
}

inline const char* entityRealmName(EntityRealm realm)
{
    switch (realm) {
    case EntityRealm::Server: return "server";
    case EntityRealm::ClientPredicted: return "client_predicted";
    case EntityRealm::ClientReplicated: return "client_replicated";
    case EntityRealm::Local: return "local";
    }
    return "unknown";
}

inline const char* entityDomainName(EntityDomain domain)
{
    switch (domain) {
    case EntityDomain::None: return "none";
    case EntityDomain::Player: return "player";
    case EntityDomain::Npc: return "npc";
    case EntityDomain::Projectile: return "projectile";
    case EntityDomain::WorldObject: return "world_object";
    }
    return "unknown";
}
