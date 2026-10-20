// 2026-10-20
/* purpose
* One actor-neutral root-capsule collision path for player-player,
* player-NPC/monster, and NPC/monster pairs.
* Owns the per-tick spatial candidate grid, swept capsule narrowphase,
* flat-top support/carry, and impact-angle push response.
* Runs on the fixed gameplay tick only. Reuses scratch storage so candidate
* and pair processing does not allocate per pair.
* Does NOT own world collision, damage, animation, rendering, or networking.
*/
#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "physics/physics-types.h"
#include "config/actor-collision-config.h"

enum class ActorPairKind : std::uint8_t
{
    Player = 0,
    Npc = 1
};

inline constexpr std::uint64_t actorPairKey(ActorPairKind kind, std::uint32_t id)
{
    return (static_cast<std::uint64_t>(kind) << 32) | id;
}

struct ActorPairCollisionActor
{
    std::uint32_t id = 0;
    ActorPairKind kind = ActorPairKind::Player;
    glm::vec3 pos{0.0f};
    glm::vec3 prevPos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec3 externalImpulse{0.0f};
    float yaw = 0.0f;
    float sizeScale = 1.0f;
    bool dead = false;
    // Dead/corpse actors and client-only remote replicas never move or receive
    // an impulse from this solver. They remain solid support/collision shapes.
    bool staticCollider = false;
    bool onGround = false;
    bool stableOnGround = true;
    const ActorCollisionProfile* profile = nullptr;
    std::uint64_t previousSupportKey = 0;
    glm::vec3 previousSupportVelocity{0.0f};
};

struct ActorPairCollisionOutput
{
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec3 externalImpulse{0.0f};
    bool onGround = false;
    bool stableOnGround = true;
    std::uint64_t supportKey = 0;
    glm::vec3 supportVelocity{0.0f};
    bool corrected = false;
    bool pushed = false;
};

struct ActorPairCollisionStats
{
    std::uint32_t actorCount = 0;
    std::uint32_t cellEntries = 0;
    std::uint32_t candidatePairs = 0;
    std::uint32_t narrowPairs = 0;
    std::uint32_t contacts = 0;
    std::uint32_t supports = 0;
    std::uint32_t pushes = 0;
    bool truncated = false;
    float broadphaseMs = 0.0f;
    float narrowphaseMs = 0.0f;
    float totalMs = 0.0f;
};

// `actors` and `outputs` must have the same length. Outputs are overwritten.
void resolveActorPairCollisions(
    const ActorPairCollisionActor* actors,
    std::size_t count,
    float dt,
    const ActorCollisionData& data,
    ActorPairCollisionOutput* outputs,
    ActorPairCollisionStats* outStats = nullptr);
