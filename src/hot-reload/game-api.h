#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define MIMITA_GAME_CALL __cdecl
#if defined(MIMITA_GAME_DLL)
#define MIMITA_GAME_EXPORT extern "C" __declspec(dllexport)
#else
#define MIMITA_GAME_EXPORT extern "C"
#endif
#else
#define MIMITA_GAME_CALL
#define MIMITA_GAME_EXPORT extern "C"
#endif

// v3 introduces versioned data envelopes, named module tables, and a
// deterministic self-test hook. The replaceable DLL still ships as one coarse
// module collection so function-level or per-module DLLs can arrive later
// without changing the public contract.
static constexpr std::uint32_t MIMITA_GAME_API_VERSION = 3;
static constexpr std::uint32_t MIMITA_GAME_MAX_MODULES = 8;
static constexpr std::size_t MIMITA_GAME_SELFTEST_MESSAGE = 128;

// State type identifiers carried by GameEnvelope::stateType. Later phases add
// the concrete plain-data structs; the identifier space is reserved here.
enum GameStateType : std::uint32_t {
    GAME_STATE_NONE = 0,
    GAME_STATE_ACTOR = 1,
    GAME_STATE_MOVEMENT = 2,
    GAME_STATE_PROJECTILE = 3,
    GAME_STATE_EFFECT = 4,
    GAME_STATE_UI = 5,
    GAME_STATE_CAMERA = 6,
};

// Versioned data envelope. Hot code receives state and returns decisions or
// results through this stable plain-data contract. It must never carry STL
// containers, owning pointers, live engine objects, or unstable class layouts.
struct GameEnvelope {
    std::uint32_t stateType;
    std::uint32_t stateVersion;
    std::uint32_t byteSize;
    std::uint32_t flags;
    void* data;
    std::uint64_t tick;
    std::uint64_t codeGeneration;
};

// Result of a candidate's deterministic smoke validation.
struct GameSelfTestResult {
    std::uint32_t structSize;
    std::uint32_t passed;
    std::uint64_t checksum;
    char message[MIMITA_GAME_SELFTEST_MESSAGE];
};

using GameLogFn = void (MIMITA_GAME_CALL *)(const char* message);

struct GamePlatformAPI {
    std::uint32_t version;
    GameLogFn log;
};

struct GameMemory {
    std::uint32_t apiVersion;
    std::uint32_t reloadCount;
    void* permanentStorage;
    std::size_t permanentStorageSize;
    GamePlatformAPI platform;
};

using GameOnReloadFn = bool (MIMITA_GAME_CALL *)(GameMemory* memory);
using GameBeforeUnloadFn = void (MIMITA_GAME_CALL *)(GameMemory* memory);
using GameSelfTestFn = bool (MIMITA_GAME_CALL *)(GameSelfTestResult* out);

struct GameEffectPartState {
    float position[3];
    float velocity[3];
    float lifetime;
    float maxLifetime;
    float gravity;
    float drag;
    std::uint8_t alive;
    std::uint8_t sticky;
    std::uint8_t affectedByGravity;
    std::uint8_t reserved;
};

using GameUpdateEffectsFn = void (MIMITA_GAME_CALL *)(
    GameMemory* memory,
    GameEffectPartState* effects,
    std::uint32_t effectCount,
    float dt);

// A named, ABI-versioned function table inside the coarse replaceable DLL.
// A null function table means the module is declared but not implemented yet.
struct GameModuleDescriptor {
    const char* name;
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    const void* functions;
};

// Effects module function table. Expanded effect/visual entry points arrive in
// a later phase; updateEffects keeps the existing behavior.
struct GameEffectModuleV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    GameUpdateEffectsFn updateEffects;
};

// ── Actor module ────────────────────────────────────────────
// Plain-data actor state and command. Player and NPC both project into
// ActorStateV1; the hot module returns decisions without owning EXE objects.
static constexpr std::uint32_t ACTOR_STATE_VERSION = 1;
static constexpr std::uint32_t ACTOR_COMMAND_VERSION = 1;
static constexpr std::uint32_t ACTOR_EVENT_VERSION = 1;

enum ActorEventType : std::uint32_t {
    ACTOR_EVENT_NONE = 0,
    ACTOR_EVENT_DAMAGED = 1,
    ACTOR_EVENT_KILLED = 2,
    ACTOR_EVENT_TARGET_SEEN = 3,
    ACTOR_EVENT_TARGET_LOST = 4,
};

struct ActorStateV1 {
    std::uint64_t id;
    std::uint32_t kind;   // 0 = player, 1 = npc
    std::uint32_t flags;  // bit0 alive, bit1 onGround, bit2 hasTarget
    float position[3];
    float velocity[3];
    float aim[3];
    float health;
    float maxHealth;
    float emotionPanic;
    float emotionFear;
    float emotionConfidence;
    float emotionStress;
    std::uint32_t team;
    std::uint32_t role;
    std::uint32_t targetId;
    float distanceToTarget;
    std::uint64_t tick;
};

struct ActorCommandV1 {
    float speedScale;
    std::uint32_t buttons;  // reserved action bits
    std::uint32_t role;
    float emotionPanic;
    float emotionFear;
    float emotionConfidence;
    float emotionStress;
    std::uint64_t tick;
};

struct ActorEventV1 {
    std::uint32_t type;
    std::uint32_t reserved;
    std::uint64_t otherId;
    float amount;
    std::uint64_t tick;
};

using GameActorChooseCommandFn = bool (MIMITA_GAME_CALL *)(
    const GameEnvelope* state, GameEnvelope* outCommand, GameMemory* memory);
using GameActorUpdateEmotionFn = void (MIMITA_GAME_CALL *)(
    GameEnvelope* state, const GameEnvelope* event, float dt, GameMemory* memory);
using GameActorChooseRoleFn = std::uint32_t (MIMITA_GAME_CALL *)(
    const GameEnvelope* state, GameMemory* memory);

struct GameActorModuleV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    GameActorChooseCommandFn chooseActorCommand;
    GameActorUpdateEmotionFn updateActorEmotion;
    GameActorChooseRoleFn chooseActorRole;
};

// ── Presentation module ─────────────────────────────────────
// Visual/UI behavior without transferring GPU resources or live engine objects.
static constexpr std::uint32_t DAMAGE_NUMBER_STYLE_VERSION = 1;
static constexpr std::uint32_t ROCKET_TRAIL_STYLE_VERSION = 1;

struct DamageNumberStyleV1 {
    float scale;
    float endScale;
    float alpha;
    float color[3];
    float lifetime;
    float moveSpeed;
    std::uint32_t visible;
    char text[32];
};

struct RocketTrailStyleV1 {
    float emissionRate;
    float size;
    float endSize;
    float lifetime;
    float alpha;
    float color[3];
    float speed;
    float spreadDegrees;
    std::uint32_t enabled;
};

using GameFormatDamageFn = bool (MIMITA_GAME_CALL *)(
    const DamageNumberStyleV1* base, int damage, std::uint32_t flags,
    DamageNumberStyleV1* out, GameMemory* memory);
using GameRocketTrailFn = bool (MIMITA_GAME_CALL *)(
    const RocketTrailStyleV1* base, RocketTrailStyleV1* out, GameMemory* memory);

struct GamePresentationModuleV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    GameFormatDamageFn formatDamageNumber;
    GameRocketTrailFn rocketTrail;
};

// ── Gameplay policy module ──────────────────────────────────
// Rocket flight and explosion policy as plain data. The EXE owns projectile
// state and authority; this module only returns parameters.
static constexpr std::uint32_t ROCKET_FLIGHT_VERSION = 1;
static constexpr std::uint32_t EXPLOSION_PARAMS_VERSION = 1;

struct RocketFlightStateV1 {
    float position[3];
    float velocity[3];
    float age;
    float lifetime;
    std::uint32_t weaponNetworkId;
    std::uint32_t flags;
};

struct RocketFlightParamsV1 {
    float speedScale;
    float gravityScale;
    float dragScale;
    float upBias;
    float lifetime;
    std::uint32_t bounces;
};

struct ExplosionStateV1 {
    float distance;
    float splashRadius;
    std::uint32_t directHit;
    std::uint32_t weaponNetworkId;
};

struct ExplosionParamsV1 {
    float splashRadius;
    float splashExponent;
    float baseDamage;
    float knockbackStrength;
    float selfDamageMultiplier;
};

// ── Gameplay behavior module ────────────────────────────────
// Generic event/behavior boundary. The kernel owns state and authority and
// emits events with mutable POD payloads; hot behaviors read/write the payload
// and the kernel applies the result. New behavior is added by extending hot
// code, not by adding EXE call sites.
static constexpr std::uint32_t GAMEPLAY_EVENT_VERSION = 1;

enum GameEventType : std::uint32_t {
    GAME_EVENT_NONE = 0,
    GAME_EVENT_DAMAGE_POLICY = 1,
    GAME_EVENT_EXPLOSION = 2,
    GAME_EVENT_COLLISION = 3,
    GAME_EVENT_SPAWN = 4,
    GAME_EVENT_DESTROY = 5,
};

// Damage source ids carried by DamagePolicyV1::source.
enum GameDamageSource : std::uint32_t {
    GAME_DAMAGE_SOURCE_EXPLOSION = 0,
    GAME_DAMAGE_SOURCE_HITSCAN = 1,
    GAME_DAMAGE_SOURCE_MELEE = 2,
    GAME_DAMAGE_SOURCE_CONTACT = 3,
};

struct GameEventV1 {
    std::uint32_t typeId;
    std::uint32_t payloadVersion;
    std::uint32_t payloadSize;
    std::uint32_t flags;
    std::uint64_t sourceEntity;
    std::uint64_t targetEntity;
    std::uint64_t projectileEntity;
    std::uint64_t tick;
    void* payload;  // mutable, kernel-owned plain data
};

struct GameplayContextV1 {
    void* host;
    std::uint64_t tick;
    std::uint64_t generation;
    std::uint64_t codeHash;
    void* emitEvent;    // reserved capability (future nested events)
    void* findEntities; // reserved capability (future queries)
};

// Request/response payload for GAME_EVENT_DAMAGE_POLICY. The kernel fills the
// base values; a hot behavior sets `handled = 1` and may override `outDamage`
// and the knockback. If no behavior handles it, the kernel uses baseDamage.
struct DamagePolicyV1 {
    std::uint64_t attackerEntity;
    std::uint64_t victimEntity;
    std::uint64_t projectileEntity;
    std::uint32_t source;  // GameDamageSource
    std::uint32_t victimIsNpc;
    std::uint32_t weaponNetworkId;
    float distance;
    std::int32_t baseDamage;
    std::int32_t outDamage;
    float knockbackX;
    float knockbackY;
    float knockbackZ;
    std::uint32_t handled;
    std::uint32_t reserved;
};

using GameBehaviorOnEventFn = void (MIMITA_GAME_CALL *)(
    const GameEventV1* event, GameplayContextV1* context);

using GameAdjustRocketFlightFn = bool (MIMITA_GAME_CALL *)(
    const RocketFlightStateV1* state, const RocketFlightParamsV1* base,
    RocketFlightParamsV1* out, GameMemory* memory);

// The gameplay module now exposes motion policy plus the generic behavior
// event handler. `explosionParameters` was replaced by the behavior path.
struct GameGameplayModuleV1 {
    std::uint32_t abiVersion;  // 2
    std::uint32_t structSize;
    GameAdjustRocketFlightFn adjustRocketFlight;
    GameBehaviorOnEventFn onEvent;
};

struct GameAPI {
    std::uint32_t version;
    std::uint32_t structSize;
    // Filled by the EXE after load; the DLL leaves these zero.
    std::uint32_t generation;
    std::uint32_t reserved;
    std::uint64_t codeHash;
    GameOnReloadFn onReload;
    GameBeforeUnloadFn beforeUnload;
    GameUpdateEffectsFn updateEffects;
    GameSelfTestFn selfTest;
    std::uint32_t moduleCount;
    std::uint32_t reserved2;
    GameModuleDescriptor modules[MIMITA_GAME_MAX_MODULES];
};

using GetGameAPIFn = bool (MIMITA_GAME_CALL *)(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);

MIMITA_GAME_EXPORT bool MIMITA_GAME_CALL GetGameAPI(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);
