// 09 15 2026
/* purpose
* Shared hot animation state. One generic `AnimationState` dynamic component
* carries the clip the hot animation policy selected and its playback. Any
* actor/monster/entity may carry it; no Player/Npc/Monster animation type.
* The hot system owns clip selection; the cold renderer owns skeleton/skinning.
* Hot-only header: not a GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_ANIMATION_STATE_COMPONENT =
    gameHash("AnimationState");

// v1 layout (kept for the v1 -> v2 migration function). Never enlarge this
// struct; new fields go in the versioned v2 contract below.
struct HotAnimationStateV1 {
    std::uint64_t clipId;       // logical animation clip id (gameHash("anim.*"))
    float playbackTime;         // seconds into the clip
    float playbackRate;         // 1 = normal; <=0 treated as 1
    std::uint32_t loop;         // 1 = loop, 0 = one-shot
    std::uint32_t flags;        // reserved
};

// Versioned replicated animation contract. Plain data only; explicit version
// and byte size. Layout changes always create a new version plus a migration.
static constexpr std::uint32_t HOT_ANIMATION_STATE_VERSION = 2;

struct HotAnimationStateV2 {
    std::uint32_t version;           // HOT_ANIMATION_STATE_VERSION
    std::uint32_t byteSize;          // sizeof(HotAnimationStateV2)
    std::uint64_t actionId;          // HOT_ACTION_* selected by the state machine
    std::uint64_t weaponKey;         // equipped tool identity (hash), 0 = none
    std::uint64_t sourceEventSeq;    // authoritative action event sequence
    std::uint32_t loop;              // 1 = loop, 0 = one-shot
    std::uint32_t actionPhase;       // 0 = begin, 1 = hold, 2 = end (advisory)
    std::uint32_t lifecycleGeneration;  // spawn generation (restart detection)
    std::uint32_t flags;             // HOT_ANIM_FLAG_* blend/lifecycle bits
    float playbackTime;              // seconds into the clip
    float playbackRate;              // 1 = normal; <=0 treated as 1
    float blendWeight;               // 0..1 transition blend into this clip
    float blendDuration;             // seconds of the active blend
};

// Local-only skeleton validation result for an animated actor. Written by
// `hot.animation-validate` from the generic skeleton.validate capability; a
// model/resource swap consumer uses it to reject a mesh missing required body
// parts before it replaces the active generation.
static constexpr std::uint64_t HOT_ANIMATION_VALID_COMPONENT =
    gameHash("AnimationValid");
static constexpr std::uint32_t HOT_ANIMATION_VALID_VERSION = 1;

struct HotAnimationValidV1 {
    std::uint32_t version;
    std::uint32_t validated;    // 1 = validation ran for this actor
    std::uint32_t valid;        // 1 = every required body part is present
    std::uint32_t missingMask;  // bit i = required part i missing
};

static constexpr std::uint32_t HOT_ANIM_FLAG_BLENDING = 1u << 0;
static constexpr std::uint32_t HOT_ANIM_FLAG_ONE_SHOT_END = 1u << 1;

// Local-only per-entity state-machine memory (not replicated; reconstructed on
// each client from the same replicated facts, so it stays deterministic). It
// carries only the previous-frame facts needed for edge detection (jump, land,
// respawn) and one-shot persistence.
static constexpr std::uint64_t HOT_ANIMATION_MEMORY_COMPONENT =
    gameHash("AnimationMemory");
static constexpr std::uint32_t HOT_ANIMATION_MEMORY_VERSION = 2;

struct HotAnimationMemoryV1 {
    std::uint32_t version;
    std::uint32_t prevFlags;             // previous ActorActionState.flags
    std::uint32_t prevGrounded;
    std::uint32_t prevDead;
    std::uint32_t prevLifecycleGeneration;
    std::uint32_t prevMeleeAction;
    std::uint64_t prevActionId;
    std::uint64_t sourceEventSeq;        // monotonic per-entity action sequence
    std::int32_t prevHealth;             // previous health (hurt edge detection)
    float timeSinceGrounded;             // seconds since grounded became true
    float locomotionTime;                // free-running clock for locomotion cycles
};

// v2 adds the previously equipped tool key so a tool-removal edge can still
// resolve the departing tool's unequip phase.
struct HotAnimationMemoryV2 {
    std::uint32_t version;
    std::uint32_t prevFlags;
    std::uint32_t prevGrounded;
    std::uint32_t prevDead;
    std::uint32_t prevLifecycleGeneration;
    std::uint32_t prevMeleeAction;
    std::uint64_t prevActionId;
    std::uint64_t sourceEventSeq;
    std::int32_t prevHealth;
    float timeSinceGrounded;
    float locomotionTime;
    std::uint64_t prevWeaponKey;
    std::uint64_t departingWeaponKey;  // tool that is playing its unequip phase
};

// Logical action/clip ids (shared with the action-state machine).
static constexpr std::uint64_t HOT_ANIM_IDLE = gameHash("anim.idle");
static constexpr std::uint64_t HOT_ANIM_MOVE = gameHash("anim.move");
static constexpr std::uint64_t HOT_ANIM_ATTACK = gameHash("anim.attack");
static constexpr std::uint64_t HOT_ANIM_DEAD = gameHash("anim.dead");
static constexpr std::uint64_t HOT_ANIM_JUMP = gameHash("anim.jump");
