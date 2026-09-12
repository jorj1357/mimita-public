// 09 12 2026
/* purpose
* Define the first entity components for the migrated vertical slice.
* Components are plain data for one concern; they follow existing repository
* vocabulary (MovementCommand fields, Player/Health shapes) instead of
* inventing a second ontology.
* Does NOT own systems, systems logic, rendering, or networking transport.
*/
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "ecs/entity-types.h"

// Who currently decides for this entity. The shared movement/weapon systems
// consume MoveIntent/AimIntent/FireIntent; the source that fills them is
// separate. This replaces "is this a player or an NPC" capability checks.
enum class ControlSource : std::uint8_t {
    LocalHuman = 0,
    ServerNpc = 1,
    RemoteNetwork = 2,
    Replay = 3,
    Scripted = 4,
};

inline const char* controlSourceName(ControlSource source)
{
    switch (source) {
    case ControlSource::LocalHuman: return "human";
    case ControlSource::ServerNpc: return "ai";
    case ControlSource::RemoteNetwork: return "remote_network";
    case ControlSource::Replay: return "replay";
    case ControlSource::Scripted: return "scripted";
    }
    return "unknown";
}

// Which side is authoritative for this entity's state.
enum class NetworkAuthority : std::uint8_t {
    Server = 0,
    ClientPredicted = 1,
    ClientReplicated = 2,
    LocalOnly = 3,
};

// Identity mirrors the legacy id so evidence can correlate both.
struct EntityIdentity {
    EntityRealm realm = EntityRealm::Server;
    EntityDomain domain = EntityDomain::None;
    std::uint32_t legacyId = 0;
    std::uint16_t generation = 0;
};

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 look{1.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct VelocityComponent {
    glm::vec3 linear{0.0f};
    glm::vec3 externalImpulse{0.0f};
};

struct BodyComponent {
    float sizeScale = 1.0f;
    float radius = 0.4f;
    float height = 1.8f;
};

struct HealthComponent {
    int current = 100;
    int max = 100;
    bool dead = false;
};

struct ControlSourceComponent {
    ControlSource source = ControlSource::RemoteNetwork;
};

struct NetworkAuthorityComponent {
    NetworkAuthority authority = NetworkAuthority::Server;
};

// Fillable by HumanInputSystem, NpcBrainSystem, NetworkInputSystem, or
// ReplayInputSystem. Mirrors the fields the shared movement kernel already uses.
struct MovementIntentComponent {
    float moveX = 0.0f;
    float moveY = 0.0f;
    bool pressed = false;
    bool jump = false;
    bool dash = false;
    bool downDash = false;
    bool freeze = false;
};

struct AimIntentComponent {
    glm::vec3 direction{1.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct FireIntentComponent {
    std::uint32_t weaponNetworkId = 0;
    bool trigger = false;
};

struct WeaponInventoryComponent {
    std::uint32_t equippedNetworkId = 0;
    std::uint32_t slot = 0;
};

struct ProjectileComponent {
    std::uint32_t weaponDefNetworkId = 0;
    std::uint32_t fireSerial = 0;
    float spawnTime = 0.0f;
    float lifetime = 0.0f;
};

struct OwnerComponent {
    EntityId owner = kInvalidEntityId;
};

struct DamageComponent {
    float amount = 0.0f;
};

struct ColliderComponent {
    float radius = 0.25f;
    float height = 0.25f;
};

// A behavior is referenced by identity/hash, never duplicated per entity.
// Multiple entities may point at the same behavior implementation.
struct BehaviorBinding {
    std::uint32_t eventType = 0;   // GameEventType the binding reacts to
    std::uint64_t behaviorId = 0;
    std::uint64_t codeHash = 0;
    std::uint32_t generation = 0;
};

struct BehaviorBindingsComponent {
    static constexpr int MAX_BINDINGS = 8;
    BehaviorBinding bindings[MAX_BINDINGS]{};
    int count = 0;

    bool add(const BehaviorBinding& binding)
    {
        if (count >= MAX_BINDINGS)
            return false;
        bindings[count++] = binding;
        return true;
    }
};
