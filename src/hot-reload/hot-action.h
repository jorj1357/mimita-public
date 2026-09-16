// 09 16 2026
/* purpose
* Shared hot actor action-state. One generic `ActorActionState` dynamic component
* carries the generic action facts (equip/reload/fire/melee timers, grounded,
* lifecycle generation, equipped tool identity) that the hot animation state
* machine consumes. The cold side is a bridge only: it mirrors existing weapon
* runtime state onto the actor EntityId; it never decides the animation.
* Any actor (player, NPC, replay actor) may carry it; there is no Player/Npc
* animation type. Hot-only header: not a GameAPI context field.
* Does NOT link into the EXE; the EXE includes it only to publish facts.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_ACTOR_ACTION_COMPONENT =
    gameHash("ActorActionState");

static constexpr std::uint32_t HOT_ACTION_STATE_VERSION = 1;

// Logical action ids. These are the state-machine outputs consumed by the
// procedural pose library; they are NOT a kernel enum and never cross as one.
static constexpr std::uint64_t HOT_ACTION_NONE = 0;
static constexpr std::uint64_t HOT_ACTION_IDLE = gameHash("action.idle");
static constexpr std::uint64_t HOT_ACTION_WALK = gameHash("action.walk");
static constexpr std::uint64_t HOT_ACTION_JUMP = gameHash("action.jump");
static constexpr std::uint64_t HOT_ACTION_FALL = gameHash("action.fall");
static constexpr std::uint64_t HOT_ACTION_LAND = gameHash("action.land");
static constexpr std::uint64_t HOT_ACTION_DASH = gameHash("action.dash");
static constexpr std::uint64_t HOT_ACTION_DOWN_DASH = gameHash("action.downdash");
static constexpr std::uint64_t HOT_ACTION_FREEZE = gameHash("action.freeze");
static constexpr std::uint64_t HOT_ACTION_EQUIP = gameHash("action.equip");
static constexpr std::uint64_t HOT_ACTION_EQUIPPED_IDLE = gameHash("action.equipped-idle");
static constexpr std::uint64_t HOT_ACTION_SHOOT = gameHash("action.shoot");
static constexpr std::uint64_t HOT_ACTION_JUST_SHOT = gameHash("action.just-shot");
static constexpr std::uint64_t HOT_ACTION_RELOAD = gameHash("action.reload");
static constexpr std::uint64_t HOT_ACTION_SLASH = gameHash("action.slash");
static constexpr std::uint64_t HOT_ACTION_LUNGE = gameHash("action.lunge");
static constexpr std::uint64_t HOT_ACTION_HURT = gameHash("action.hurt");
static constexpr std::uint64_t HOT_ACTION_DEATH = gameHash("action.death");
static constexpr std::uint64_t HOT_ACTION_RESPAWN = gameHash("action.respawn");
static constexpr std::uint64_t HOT_ACTION_UNEQUIP = gameHash("action.unequip");

// `flags` bits: generic action facts the hot state machine reads. A missing
// fact is simply absent; the machine degrades safely (remote actors only carry
// the replicated network weapon-state bits for now).
static constexpr std::uint32_t HOT_ACTION_FLAG_GROUNDED  = 1u << 0;
static constexpr std::uint32_t HOT_ACTION_FLAG_JUMPING   = 1u << 1;
static constexpr std::uint32_t HOT_ACTION_FLAG_DASHING   = 1u << 2;
static constexpr std::uint32_t HOT_ACTION_FLAG_DOWN_DASH = 1u << 3;
static constexpr std::uint32_t HOT_ACTION_FLAG_FREEZING  = 1u << 4;
static constexpr std::uint32_t HOT_ACTION_FLAG_DEAD      = 1u << 5;
static constexpr std::uint32_t HOT_ACTION_FLAG_SHOOTING  = 1u << 6;
static constexpr std::uint32_t HOT_ACTION_FLAG_RELOADING = 1u << 7;
static constexpr std::uint32_t HOT_ACTION_FLAG_EQUIPPING = 1u << 8;
static constexpr std::uint32_t HOT_ACTION_FLAG_MELEE     = 1u << 9;
static constexpr std::uint32_t HOT_ACTION_FLAG_HURT      = 1u << 10;
static constexpr std::uint32_t HOT_ACTION_FLAG_RESPAWN   = 1u << 11;
static constexpr std::uint32_t HOT_ACTION_FLAG_DOWNED    = 1u << 12;

// Plain data; no STL, pointers, or class layout. Explicit version + byte size.
struct HotActorActionStateV1 {
    std::uint32_t version;         // HOT_ACTION_STATE_VERSION
    std::uint32_t byteSize;        // sizeof(HotActorActionStateV1)
    std::uint64_t flags;           // HOT_ACTION_FLAG_* bitset
    std::uint64_t weaponKey;       // equipped tool identity (hash), 0 = none
    std::uint64_t sourceEventSeq;  // last authoritative action event sequence
    std::uint32_t lifecycleGeneration;  // spawn generation
    std::uint32_t meleeAction;     // weapon-specific melee pose state (0 = none)
    float equipTimer;              // seconds remaining in equip transition
    float reloadTimer;             // seconds remaining in reload
    float fireCooldown;            // seconds until next allowed shot
    float shootEffectTimer;        // muzzle/shoot effect time remaining
    float speed;                   // planar speed (locomotion scaling)
    std::uint32_t ammo;            // current magazine ammo (advisory)
    std::uint32_t reserve;         // reserve ammo (advisory)
    std::uint32_t isReloading;     // 1 = reload owns the upper body
    std::uint32_t reserved[3];
};
