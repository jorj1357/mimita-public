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
// deterministic self-test hook. v4 adds the generic component capability layer
// (read/write/find/query/log) and the ragdoll-bind, movement, projectile,
// death/respawn, and connection-state behavior seams, so those systems are
// edited as hot behavior instead of cold kernel code.
static constexpr std::uint32_t MIMITA_GAME_API_VERSION = 8;
static constexpr std::uint32_t MIMITA_GAME_MAX_MODULES = 8;
static constexpr std::size_t MIMITA_GAME_SELFTEST_MESSAGE = 128;

// Compile-time FNV-1a so the kernel and packages hash names identically without
// a runtime table. Declared early so any constant below may use it.
constexpr std::uint64_t gameHash(const char* s, std::uint64_t h = 1469598103934665603ull)
{
    return (*s == '\0') ? h : gameHash(s + 1, (h ^ (std::uint64_t)(unsigned char)*s) * 1099511628211ull);
}

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

// ── Generic component capability layer ─────────────────────
// Stable component type ids for readComponent/writeComponent/findEntities. The
// kernel maps each id to its ECS component; the POD projections below are the
// only layouts that cross the boundary. Adding a projection does not add a new
// call site: capabilities are generic over the id.
enum GameComponentType : std::uint32_t {
    GAME_COMPONENT_NONE = 0,
    GAME_COMPONENT_TRANSFORM = 1,
    GAME_COMPONENT_VELOCITY = 2,
    GAME_COMPONENT_HEALTH = 3,
    GAME_COMPONENT_MOVEMENT_INTENT = 4,
    GAME_COMPONENT_AIM_INTENT = 5,
    GAME_COMPONENT_FIRE_INTENT = 6,
    GAME_COMPONENT_PROJECTILE = 7,
    GAME_COMPONENT_COLLIDER = 8,
    GAME_COMPONENT_BODY = 9,
    GAME_COMPONENT_RAGDOLL_LIMB = 10,
    GAME_COMPONENT_RAGDOLL_JOINT = 11,
    GAME_COMPONENT_RAGDOLL_ROOT = 12,
    GAME_COMPONENT_RAGDOLL_GRAB = 13,
    GAME_COMPONENT_BEHAVIOR_BINDINGS = 14,
    GAME_COMPONENT_MOVEMENT_RUNTIME_STATE = 15,
    GAME_COMPONENT_CONTROL_SOURCE = 16,
};

struct GameTransformComponentV1 {
    float position[3];
    float look[3];
    float yaw;
    float pitch;
};

struct GameVelocityComponentV1 {
    float linear[3];
    float externalImpulse[3];
};

struct GameHealthComponentV1 {
    std::int32_t current;
    std::int32_t max;
    std::uint32_t dead;
};

struct GameMovementIntentComponentV1 {
    float moveX;
    float moveY;
    std::uint32_t pressed;
    std::uint32_t jump;
    std::uint32_t dash;
    std::uint32_t downDash;
    std::uint32_t freeze;
};

static constexpr std::uint32_t MOVEMENT_RUNTIME_STATE_VERSION = 1;
struct GameMovementRuntimeStateComponentV1 {
    std::uint32_t version;
    std::uint32_t grounded;
    std::uint32_t jumpHeldPreviously;
    std::uint32_t jumpAirJumpArmed;
    std::int32_t airJumpsLeft;
    std::uint32_t dashHeldPreviously;
    std::uint32_t downDashHeldPreviously;
    std::uint32_t dashAvailable;
    std::uint32_t downDashAvailable;
    float dashCooldownSeconds;
    float jumpIntentSeconds;
    float dashGraceSeconds;
    std::uint32_t freezePreviously;
    // reserved[0] = tick of the last hot actor-movement simulation,
    // reserved[1] = hot generation that produced it,
    // reserved[2] bit0 = hot actor-movement authority is active for this actor.
    // Cold server code yields its kernel movement when reserved[0] == server tick.
    std::uint32_t reserved[3];
};

// MovementRuntimeStateComponentV1.reserved indices (shared hot/cold contract).
static constexpr int GAME_MOVEMENT_STAMP_TICK = 0;
static constexpr int GAME_MOVEMENT_STAMP_GENERATION = 1;
static constexpr int GAME_MOVEMENT_STAMP_FLAGS = 2;
static constexpr std::uint32_t GAME_MOVEMENT_STAMP_FLAG_ACTIVE = 1u;

// ControlSource projection: who decides for this actor. Mirrors ecs::ControlSource.
enum GameControlSourceV1 : std::uint32_t {
    GAME_CONTROL_LOCAL_HUMAN = 0,
    GAME_CONTROL_SERVER_NPC = 1,
    GAME_CONTROL_REMOTE_NETWORK = 2,
    GAME_CONTROL_REPLAY = 3,
    GAME_CONTROL_SCRIPTED = 4,
};
struct GameControlSourceComponentV1 {
    std::uint32_t source;
    std::uint32_t reserved;
};

struct GameAimIntentComponentV1 {
    float direction[3];
    float yaw;
    float pitch;
};

struct GameFireIntentComponentV1 {
    std::uint32_t weaponNetworkId;
    std::uint32_t trigger;
};

struct GameProjectileComponentV1 {
    std::uint32_t weaponDefNetworkId;
    std::uint32_t fireSerial;
    float spawnTime;
    float lifetime;
};

struct GameColliderComponentV1 {
    float radius;
    float height;
};

struct GameBodyComponentV1 {
    float sizeScale;
    float radius;
    float height;
};

struct GameRagdollLimbComponentV1 {
    std::uint32_t limbIndex;
    std::uint32_t parentIndex;
    float position[3];
    float orientation[4];
    float linearVelocity[3];
    float angularVelocity[3];
    float mass;
    float radius;
    float halfHeight;
    float inverseMass;
};

struct GameRagdollJointComponentV1 {
    std::uint32_t limbIndex;
    std::uint32_t parentLimb;
    float parentLocalAnchor[3];
    float childLocalAnchor[3];
    float restLength;
    float maxStretch;
    float stiffness;
    float damping;
    float positionBeta;
};

struct GameRagdollRootComponentV1 {
    std::uint32_t ownerActorId;
    std::uint32_t limbCount;
    std::uint32_t solverIterations;
    float gravityScale;
    float stiffness;
    float damping;
    std::uint32_t alive;
    std::uint32_t corpse;
    std::uint64_t lastSolveTick;
};

struct GameRagdollGrabComponentV1 {
    std::uint32_t active;
    std::uint32_t wasActive;
    std::uint32_t hand;
    std::uint32_t limbEntity;
    float grabPoint[3];
    float grabNormal[3];
    float handPosition[3];
    float handLocalAnchor[3];
    std::uint32_t targetEntity;
    float targetLocalAnchor[3];
    std::uint32_t grabbedActorId;
    float strength;
    std::uint32_t constraintSerial;
};

static constexpr std::uint32_t GAME_MAX_BEHAVIOR_BINDINGS = 8;

struct GameBehaviorBindingV1 {
    std::uint32_t eventType;
    std::uint32_t reserved;
    std::uint64_t behaviorId;
    std::uint64_t codeHash;
    std::uint32_t generation;
    std::uint32_t reserved2;
};

struct GameBehaviorBindingsComponentV1 {
    std::uint32_t count;
    std::uint32_t reserved;
    GameBehaviorBindingV1 bindings[GAME_MAX_BEHAVIOR_BINDINGS];
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
    GAME_EVENT_FIRE_INTENT = 6,
    GAME_EVENT_RAGDOLL_SOLVE = 7,
    // Generic behavior seams (ABI v4). The kernel fills the base payload and
    // applies the result; hot behavior owns the policy.
    GAME_EVENT_RAGDOLL_BIND = 8,          // mesh node -> ragdoll body frame
    GAME_EVENT_MOVEMENT_RECONCILE = 9,    // client local-player snap policy
    GAME_EVENT_MOVEMENT_VALIDATION = 10,  // server movement-report decision
    GAME_EVENT_PROJECTILE_PRESENT = 11,   // projectile visibility/style
    GAME_EVENT_ACTOR_DEATH = 12,          // death presentation policy
    GAME_EVENT_ACTOR_RESPAWN = 13,        // respawn policy
    GAME_EVENT_CONNECTION_STATE = 14,     // connection status/retry policy
    // Generic UI interaction. The cold backend hit-tests a hot-emitted widget
    // (logical element id) and emits this; hot code owns what the action means.
    GAME_EVENT_UI_ACTION = 15,
};

// Damage source ids carried by DamagePolicyV1::source.
enum GameDamageSource : std::uint32_t {
    GAME_DAMAGE_SOURCE_EXPLOSION = 0,
    GAME_DAMAGE_SOURCE_HITSCAN = 1,
    GAME_DAMAGE_SOURCE_MELEE = 2,
    GAME_DAMAGE_SOURCE_CONTACT = 3,
};

struct GameEventV1 {
    // 64-bit event identity: the full gameHash("package.event") id, so runtime
    // event types are never truncated to 32 bits. `schemaHash` is the canonical
    // payload-schema identity (0 = unspecified).
    std::uint64_t typeId;
    std::uint64_t schemaHash;
    std::uint32_t payloadVersion;
    std::uint32_t payloadSize;
    std::uint32_t flags;
    std::uint32_t reserved;
    std::uint64_t sourceEntity;
    std::uint64_t targetEntity;
    std::uint64_t projectileEntity;
    std::uint64_t tick;
    void* payload;  // mutable, kernel-owned plain data
};

// Generic kernel capabilities exposed to hot behavior. `host` is opaque and
// only valid for the duration of the dispatch call; never cache it.
using GameReadComponentFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, std::uint32_t componentType,
    void* out, std::uint32_t outSize);
using GameWriteComponentFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, std::uint32_t componentType,
    const void* in, std::uint32_t inSize);
using GameFindEntitiesFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t domain, std::uint32_t componentType,
    std::uint64_t* out, std::uint32_t maxOut);
using GameQueryWorldRayFn = bool (MIMITA_GAME_CALL *)(
    void* host, const float origin[3], const float dir[3], float maxDistance,
    float* outPoint, float* outNormal, float* outDistance);
using GameLogCapFn = void (MIMITA_GAME_CALL *)(void* host, const char* message);

// Dynamic (package-declared) component access, keyed by schema id hash. Lets a
// package read/write its own component without a new typed enum entry.
using GameDynamicComponentReadFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, std::uint64_t typeId, void* out, std::uint32_t outSize);
using GameDynamicComponentWriteFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, std::uint64_t typeId, const void* in, std::uint32_t inSize);

// ── Generic entity / dynamic-component lifecycle (ABI v6) ─────────────
// Operation-generic capabilities over the entity registry and the dynamic
// component store. None of these are per-component-type: the type is a 64-bit
// schema id the package declared at runtime. This is the layer that lets a hot
// package create state that did not exist when the EXE started.
struct GameDynamicComponentInfoV1 {
    std::uint64_t typeId;
    std::uint64_t schemaHash;
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t align;
    std::uint32_t copyPolicy;    // GameCopyPolicy
    std::uint32_t networkPolicy;
    std::uint32_t reserved;
    char name[48];
};

using GameEntityCreateFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t realm, std::uint64_t* outEntity);
using GameEntityDestroyFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity);
using GameDynamicComponentRemoveFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, std::uint64_t typeId);
using GameDynamicComponentEnumerateFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t typeId, std::uint64_t* out, std::uint32_t maxOut);
using GameDynamicComponentsOnEntityFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, std::uint64_t* out, std::uint32_t maxOut);
using GameDynamicComponentInfoFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t typeId, GameDynamicComponentInfoV1* out);
using GameRelationshipAddFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t typeId, std::uint64_t from, std::uint64_t to,
    std::uint64_t value);
using GameRelationshipRemoveFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t typeId, std::uint64_t from, std::uint64_t to);
using GameRelationshipQueryFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t typeId, std::uint64_t from, std::uint64_t* outTo,
    std::uint64_t* outValue, std::uint32_t maxOut);

// ── Generic match mechanism (ABI v7) ─────────────────────────────────
// A gamemode is runtime-registered metadata: an id (hash), a display name, and
// the simulation domain whose systems own that mode's policy. The kernel does
// not know any mode name; it routes the active mode id to its domain and lets
// those systems decide. No mode enum or per-mode callback exists.
struct GameModeDescriptorV1 {
    std::uint64_t id;             // gameHash("ffa")
    std::uint64_t domainId;       // domain whose systems run while this mode is active
    std::uint64_t matchSchemaId;  // optional package match-state component (0 = none)
    std::uint64_t matchSchemaHash;
    const char* displayName;
};

// General occurrence fact: one actor killed another. Describes the event only.
// Team/role/state must be queried from dynamic components or relationships; a
// hot handler that sets `handled` owns the scoring decision for this kill.
struct GameActorKilledV1 {
    std::uint64_t killerEntity;
    std::uint64_t victimEntity;
    std::uint32_t killerId;
    std::uint32_t victimId;
    std::uint32_t killerIsNpc;
    std::uint32_t victimIsNpc;
    std::uint32_t weaponNetworkId;
    std::uint32_t tick;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// General match evaluation request/response. The kernel asks the active mode to
// decide the match outcome; a handler that sets `handled` owns that decision and
// the cold mode-specific win branch is skipped.
struct GameMatchEvaluateV1 {
    std::uint64_t matchEntity;
    std::uint32_t tick;
    std::uint32_t phase;
    std::uint32_t handled;
    std::uint32_t outEndMatch;
    std::uint32_t outWinnerKind;   // 0 none, 1 actor, 2 team
    std::uint32_t outWinnerId;
    std::uint32_t outVictoryType;  // 0 score, 1 time
    std::uint32_t reserved;
};

// TEMPORARY compatibility bridge to the existing DuelStatePacket/UI. Authoritative
// score lives in package dynamic components; this snapshot lets the legacy packet
// builder read it generically. It is NOT the canonical future gamemode-state
// representation and should be replaced by generic state replication.
static constexpr std::uint32_t GAME_MAX_MATCH_SCORES = 32;
struct GameMatchScoreEntryV1 {
    std::uint64_t ownerId;   // actor id or team id
    std::int32_t score;
    std::int32_t deaths;
    std::uint32_t kind;      // 0 actor, 1 team
    std::uint32_t reserved;
};
struct GameMatchScoreSnapshotV1 {
    std::uint32_t count;
    std::uint32_t reserved;
    GameMatchScoreEntryV1 entries[GAME_MAX_MATCH_SCORES];
};

using GameMatchCurrentFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t* outMatchEntity);
using GameMatchActorTeamReadFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t actorId, std::int32_t* outTeam);
using GameMatchFinishFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t winnerKind, std::uint32_t winnerId,
    std::uint32_t victoryType);
using GameMatchSetPhaseFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t phase);
using GameMatchRespawnFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t actorEntity);
using GameMatchSetTeamFn = bool (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t actorId, std::int32_t team);
using GameMatchScoreSnapshotFn = void (MIMITA_GAME_CALL *)(
    void* host, GameMatchScoreSnapshotV1* out);

// A hot movement system requests a full local-player movement override for this
// tick (free-fly/noclip). flags bit0 = active. The kernel applies the transform
// and skips the built-in physics step when active.
using GameRequestMovementOverrideFn = void (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t flags, const float position[3],
    const float velocity[3], float yaw);

// ── Movement state / capsule solve (hot movement groundwork) ────────
// Plain-data movement state the kernel can read from and write back into the
// real player, plus a kernel capsule-vs-world solve so hot movement systems do
// not need the cold collision code. Append-only.
static constexpr std::uint32_t MOVEMENT_STATE_VERSION = 1;
struct MovementStateV1 {
    float position[3];
    float velocity[3];
    float yaw;
    float radius;
    float halfHeight;
    float sizeScale;
    float gravityScale;   // multiplies 9.81 in the capsule solver (0/absent = 1)
    std::uint32_t grounded;
    std::uint32_t collided;
};
// One fixed capsule move against the world: integrates velocity + gravity,
// resolves world collision, and writes the resolved state back in place.
using GameMoveCapsuleFn = void (MIMITA_GAME_CALL *)(
    void* host, MovementStateV1* state, float dt);

// ── Generic capability resolution (ABI v8) ──────────────────────────
// ONE generic bridge so hot code reaches kernel primitives (physics.move,
// effect.spawn, skeleton.apply, ...) by id without a permanent context field per
// concept. New reusable primitives register by id; ordinary gameplay never adds
// an ABI field again.
using GameResolveCapabilityFn = void* (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t capabilityId);

// physics.move: a low-level, policy-free physics/collision primitive. The caller
// supplies the velocity + gravity scale; the kernel runs the shared collision
// pipeline (sweep/slide, step-up, floor recovery, contact grounded) and writes
// the resolved state back in place. This is a general capsule move, not a
// movement-policy function; vehicles/swimming/climbing call the same primitive.
static constexpr std::uint32_t GAME_PHYSICS_MOVE_FULL_PIPELINE = 1u;
// Resolve against the server's HeadlessWorld collision instead of the client
// render World. Hot movement sets this for authoritative server actors.
static constexpr std::uint32_t GAME_PHYSICS_MOVE_HEADLESS = 2u;
using GamePhysicsMoveFn = void (MIMITA_GAME_CALL *)(
    void* host, MovementStateV1* state, float dt, std::uint32_t flags);

// effect.spawn: ONE generic effect/particle spawn descriptor. The `kind` hash
// selects an emitter template (footstep, dash, freeze, spark, smoke, blood,
// debris, muzzle flash, ...); the numeric fields tune it. Future effects use the
// same mechanism and need no ABI change.
// A `kind` of gameHash("light.dynamic") drives the EXISTING cold dynamic-light
// manager through this same descriptor: position = light position, color =
// light color, scale = intensity, endScale = radius, lifetime = lifetime.
struct GameEffectSpawnV1 {
    std::uint64_t kind;         // gameHash("effect.footstep") etc.
    std::uint64_t ownerEntity;  // 0 = none
    std::uint32_t count;
    std::uint32_t flags;
    float position[3];
    float direction[3];
    float color[4];
    float scale;
    float endScale;
    float speed;
    float lifetime;
    float spread;
};
using GameEffectSpawnFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameEffectSpawnV1* desc);

// effect.part: exposes the EXISTING pooled `EffectPart` primitive to hot code.
// Hot policy builds the descriptor; the kernel owns the pool, lifetime, and
// renderer. This is the same primitive the cold JSON hit/impact/blood path uses,
// so a hot recipe reproduces that behavior exactly: textured camera-facing
// billboards (replayType "hitfx_particle" + texturePath), sticky/flat decals,
// beams, boxes, gravity, and tick-defined lifetimes. Fixed-size strings only.
static constexpr std::uint32_t GAME_EFFECT_STRING = 96;
struct GameEffectPartV1 {
    float position[3];
    float velocity[3];
    float color[3];
    float normal[3];
    float rotation[3];
    float endPosition[3];
    float halfSize[3];
    float scale;
    float endScale;
    float alpha;
    float gravity;
    float drag;
    float thickness;
    float endThickness;
    float maxLifetime;          // seconds (1/60 = one tick)
    std::uint32_t affectedByGravity;
    std::uint32_t sticky;
    std::uint32_t flatDecal;
    std::uint32_t beam;
    std::uint32_t box;
    std::uint32_t billboardText;
    char replayType[GAME_EFFECT_STRING];
    char texturePath[GAME_EFFECT_STRING];
    char label[GAME_EFFECT_STRING];
    // Append-only: draw a real primitive mesh (sphere/cube/beam/hexagon) with
    // per-axis scale instead of the legacy DebugVis shapes. 0 = legacy shape.
    std::uint64_t meshResourceId;
    std::uint64_t textureResourceId;   // 0 = model's own texture / solid color
    float scaleXYZ[3];                 // per-axis scale multiplier (0 => 1)
    std::uint32_t reserved;
};
using GameEffectPartFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameEffectPartV1* part);

// skeleton.apply: apply a pose to an actor's skeleton. The kernel owns the
// rest pose, hierarchy, and node mapping; the caller supplies per-part euler
// offsets keyed by part-name hash. Generic for animation graphs, ragdoll poses,
// replay poses, and any future pose source.
static constexpr std::uint32_t GAME_MAX_POSE_PARTS = 16;
struct GamePosePartV1 {
    std::uint64_t part;         // gameHash("torso") etc.
    float translation[3];
    float rotationEuler[3];
};
struct GameSkeletonPoseV1 {
    std::uint32_t count;
    std::uint32_t flags;
    std::uint64_t entity;
    GamePosePartV1 parts[GAME_MAX_POSE_PARTS];
};
using GameSkeletonApplyFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameSkeletonPoseV1* pose);

// skeleton.validate: verify that an actor's current skeleton exposes a set of
// required part/bone name hashes. Generic mechanism for model-generation swaps
// and candidate self-tests: a mesh/skeleton missing a required part is rejected
// before it replaces the active resource. The caller owns which parts are
// required; the kernel owns bone-name lookup. No new context field.
static constexpr std::uint32_t GAME_MAX_VALIDATE_PARTS = 16;
struct GameSkeletonValidateV1 {
    std::uint64_t entity;
    std::uint64_t requiredParts[GAME_MAX_VALIDATE_PARTS];
    std::uint32_t requiredCount;
    // out
    std::uint32_t presentMask;   // bit i set when requiredParts[i] resolved
    std::uint32_t missingCount;  // number of required parts not resolved
    std::uint32_t valid;         // 1 = all required parts present
};
using GameSkeletonValidateFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameSkeletonValidateV1* request);

// animation.update: TEMPORARY BRIDGE (allowed by the animation guidance). The
// hot animation system owns when/how this runs; the kernel currently runs the
// existing procedural animation behind it. The desired forward path is a hot
// system computing poses itself and calling skeleton.apply; this bridge exists
// so animation is correct immediately and can be replaced hot, piece by piece.
using GameAnimationUpdateFn = void (MIMITA_GAME_CALL *)(
    void* host, float dt, std::uint32_t flags);

struct GameplayContextV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    void* host;
    std::uint64_t tick;
    std::uint64_t generation;
    std::uint64_t codeHash;
    void* emitEvent;    // emit a nested GameEventV1 (bounded FIFO queue)
    GameReadComponentFn readComponent;
    GameWriteComponentFn writeComponent;
    GameFindEntitiesFn findEntities;
    GameQueryWorldRayFn queryWorldRay;
    GameLogCapFn log;
    // v5 additions (append-only): dynamic component access.
    GameDynamicComponentReadFn dynamicReadComponent;
    GameDynamicComponentWriteFn dynamicWriteComponent;
    // Kernel shared state + reload-persistent storage for cross-module
    // coordination (e.g. editor mode visible to gameplay policy).
    void* permanentStorage;
    std::uint64_t permanentStorageSize;
    // v5 additions (append-only): hot movement override.
    GameRequestMovementOverrideFn requestMovementOverride;
    // v6 additions (append-only): kernel capsule-vs-world solve.
    GameMoveCapsuleFn moveCapsule;
    // v6 additions (append-only): generic entity / dynamic-component lifecycle.
    GameEntityCreateFn entityCreate;
    GameEntityDestroyFn entityDestroy;
    GameDynamicComponentRemoveFn dynamicRemoveComponent;
    GameDynamicComponentEnumerateFn dynamicEnumerateComponent;
    GameDynamicComponentsOnEntityFn dynamicComponentsOnEntity;
    GameDynamicComponentInfoFn dynamicComponentInfo;
    GameRelationshipAddFn relationshipAdd;
    GameRelationshipRemoveFn relationshipRemove;
    GameRelationshipQueryFn relationshipQuery;
    // v7 additions (append-only): generic authoritative match capabilities.
    GameMatchCurrentFn matchCurrent;
    GameMatchActorTeamReadFn matchActorTeamRead;
    GameMatchFinishFn matchFinish;
    GameMatchSetPhaseFn matchSetPhase;
    GameMatchRespawnFn matchRespawn;
    GameMatchSetTeamFn matchSetTeam;
    // v8 additions (append-only): one generic capability resolver. Kernel
    // primitives register by id (physics.move, effect.spawn, skeleton.apply);
    // hot code resolves and calls them. This replaces per-concept ABI fields.
    GameResolveCapabilityFn resolveCapability;
};

// Small kernel-owned shared state area at the start of permanentStorage so hot
// modules can coordinate without a dedicated ABI slot per concept. Fields are
// intentionally generic; packages may extend within their own budget.
static constexpr std::uint32_t GAME_SHARED_MAGIC = 0x45445348u;  // 'EDSH'
struct GameSharedStateV1 {
    std::uint32_t magic;
    std::uint32_t modeFlags;       // bit0 = creation/inspection mode active
    std::uint64_t selectedEntity;
    std::uint64_t hoveredEntity;
    std::uint64_t localPlayerEntity;
    std::uint32_t reserved[4];
    // Kernel-owned match entity for the active match (0 = none). Package match
    // state attaches to this entity through the dynamic component capabilities.
    std::uint64_t matchEntity;
    // Append-only: hot-owned aim mode, a hash of gameHash("crosshair" /
    // "camforward" / "physical" / "farpoint" / "world_hit"). 0 = use the cold
    // gameplay config. Hot policy writes it; the cold aim code reads it.
    std::uint64_t aimModeHash;
};
static constexpr std::uint32_t GAME_MODE_FLAG_CREATION = 1u;
static constexpr std::uint32_t GAME_MODE_FLAG_HOT_MOVEMENT = 2u;

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
    // Kernel safety cap filled from serverAuthoritativeDamageLimit(); a hot
    // behavior may override it (0 = unlimited). Separate from gameplay tuning.
    std::uint32_t outDamageLimit;
};

using GameBehaviorOnEventFn = void (MIMITA_GAME_CALL *)(
    const GameEventV1* event, GameplayContextV1* context);

// Request/response payload for GAME_EVENT_FIRE_INTENT (one held-fire tick).
// The kernel fills baseFire (1 normally); a behavior sets handled and may
// suppress or adjust the shot and its ammo cost.
struct FireIntentPolicyV1 {
    std::uint64_t entity;
    std::uint32_t weaponNetworkId;
    std::uint32_t tick;
    std::uint32_t baseFire;
    std::uint32_t outFire;
    std::uint32_t ammoCost;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// Request/response payload for GAME_EVENT_RAGDOLL_SOLVE. The kernel fills the
// base solver parameters; a behavior sets handled and may override stiffness,
// damping, gravity, and iteration count. This is the live solver-policy seam.
struct RagdollPolicyV1 {
    std::uint64_t ownerActor;
    std::uint32_t limbCount;
    std::uint32_t tick;
    float baseStiffness;
    float baseDamping;
    float baseGravityScale;
    std::uint32_t baseIterations;
    float outStiffness;
    float outDamping;
    float outGravityScale;
    std::uint32_t outIterations;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// ── Ragdoll bind policy ─────────────────────────────────────
// Kernel -> behavior, once per body part, while building a ragdoll body from
// the model's rest pose. The kernel supplies the mesh node's world transform
// and its collider bounds; the behavior returns the physics body frame. This
// is the seam that keeps mirrored/scaled avatar nodes (negative scale) from
// collapsing onto the wrong side, without a second build site.
enum RagdollBindPartId : std::uint32_t {
    RAGDOLL_BIND_TORSO = 0,
    RAGDOLL_BIND_HEAD = 1,
    RAGDOLL_BIND_LEFT_ARM = 2,
    RAGDOLL_BIND_RIGHT_ARM = 3,
    RAGDOLL_BIND_LEFT_LEG = 4,
    RAGDOLL_BIND_RIGHT_LEG = 5,
};

struct RagdollBindPartV1 {
    std::uint32_t partId;        // RagdollBindPartId
    std::uint32_t hasCollider;
    float nodePosition[3];
    // Node world linear part, column-major 3x3 as glm stores it. May contain a
    // reflection (negative determinant) for mirrored avatar parts.
    float nodeLinear[9];
    float colliderMin[3];
    float colliderMax[3];
    float cfgRadius;
    float cfgHalfHeight;
    float cfgOffset[3];
    float cfgCenterOfMass[3];
    std::uint32_t hasCfgAxis;
    float cfgAxis[3];
    // ── out (canonical body frame) ──
    float outComLocal[3];        // center of mass offset from the node position
    float outCapsuleCenter[3];   // capsule center relative to the body COM
    float outCapsuleAxis[3];
    float outRadius;
    float outHalfHeight;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// ── Client local-player movement reconcile policy ──────────
struct MovementReconcileV1 {
    std::uint64_t ownerActor;
    float clientPosition[3];
    float serverPosition[3];
    float error;             // distance(client, server)
    float majorThreshold;
    std::uint32_t epochReady;
    std::uint32_t ragdollActive;
    std::uint32_t teleportPending;
    std::uint32_t dead;
    // out
    std::uint32_t applyPosition;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// ── Server movement-report validation policy ───────────────
// The kernel performs the structural/lifecycle checks and fills the computed
// decision + suggested correction; the behavior may override it (for example
// to let a ragdoll's physics root move under client body authority).
struct MovementValidationV1 {
    std::uint64_t ownerActor;
    std::uint32_t serverTick;
    std::uint32_t ragdollActive;
    std::uint32_t computedDecision;   // 0=accept 1=correct 2=reject
    std::uint32_t computedReason;
    float reportPosition[3];
    float previousPosition[3];
    float suggestedPosition[3];
    float suggestedVelocity[3];
    // out
    std::uint32_t decision;
    std::uint32_t handled;
    // reserved bit0 = GAME_MOVEMENT_VALIDATION_FORCE_ACTIVE: the hot policy
    // proved this is a current-life report, so the cold server must clear the
    // spawn wedge (activate the player) instead of holding it at spawn. Keeps
    // the payload size stable for an already-running executable.
    std::uint32_t reserved;
};

static constexpr std::uint32_t GAME_MOVEMENT_VALIDATION_FORCE_ACTIVE = 1u;

// ── Generic actor policy seams (bridge) ─────────────────────────────────────
// These let the hot package own decisions that used to live only in cold code.
// Every cold caller dispatches with facts; a handler that sets `handled` owns
// the decision; otherwise the cold default behavior runs unchanged.

// actor.spawn-policy: choose/suppress an actor spawn (startup NPCs and players).
static constexpr std::uint64_t GAME_EVENT_ACTOR_SPAWN_POLICY =
    gameHash("actor.spawn-policy");
enum GameActorSpawnKindV1 : std::uint32_t {
    GAME_ACTOR_SPAWN_PLAYER = 0,
    GAME_ACTOR_SPAWN_NPC = 1,
};
struct ActorSpawnPolicyV1 {
    // in
    std::uint32_t kind;              // GameActorSpawnKindV1
    std::uint32_t index;             // startup/actor index
    std::uint32_t spawnPointCount;
    std::uint32_t occupiedByActor;   // nearest non-self actor within a radius
    std::uint32_t isRespawn;         // 0 initial/startup, 1 respawn
    float chosenPosition[3];         // cold-picked candidate
    float chosenYaw;
    // in: candidate spawn points (up to 8) so the policy can relocate
    std::uint32_t candidateCount;
    std::uint32_t reservedIn;
    float candidatePosition[8][3];
    float candidateYaw[8];
    // out
    std::uint32_t suppress;          // 1 = do not spawn this actor
    std::uint32_t handled;
    float position[3];               // override spawn position (used if handled)
    float yaw;
};

// input.send-policy: whether the client sends an InputPacket this frame and why.
static constexpr std::uint64_t GAME_EVENT_INPUT_SEND_POLICY =
    gameHash("input.send-policy");
enum GameInputSendGateV1 : std::uint32_t {
    GAME_INPUT_GATE_CONNECTED = 1u << 0,
    GAME_INPUT_GATE_LOCAL_PLAYER = 1u << 1,
    GAME_INPUT_GATE_INPUT_PRESENT = 1u << 2,
    GAME_INPUT_GATE_DUE = 1u << 3,
    GAME_INPUT_GATE_GENERATION_ALLOWED = 1u << 4,
};
struct InputSendPolicyV1 {
    // in
    std::uint32_t localPlayerId;
    std::uint32_t due;
    std::uint32_t dead;
    std::uint32_t bootstrapState;    // generation bootstrap state
    std::uint64_t serverCodeGeneration;
    std::uint64_t localGeneration;
    float position[3];
    // out
    std::uint32_t send;              // 1 = send this frame
    std::uint32_t handled;
    std::uint32_t gateFlags;         // GameInputSendGateV1 bits (filled in by cold)
};

// input.receive-policy: accept/reject a received InputPacket before processing.
static constexpr std::uint64_t GAME_EVENT_INPUT_RECEIVE_POLICY =
    gameHash("input.receive-policy");
struct InputReceivePolicyV1 {
    // in
    std::uint32_t playerId;
    std::uint32_t packetBytes;
    std::uint32_t playerExists;
    std::uint32_t playerActive;
    std::uint32_t playerDead;
    std::uint64_t inputPacketsSeen;
    // in: accepted-report facts, so the policy can adopt client-authoritative
    // ordinary movement (spec phase 1) or leave the server to simulate.
    std::uint64_t playerEntity;
    std::uint32_t serverTick;
    std::uint32_t reportGrounded;
    float reportPosition[3];
    float reportVelocity[3];
    // out
    std::uint32_t accept;            // 0 = drop this InputPacket
    std::uint32_t handled;
    std::uint32_t adoptState;        // 1 = server adopts this report as authority
};

// actor.lifecycle-policy: decide respawn and spawn protection after death.
static constexpr std::uint64_t GAME_EVENT_ACTOR_LIFECYCLE_POLICY =
    gameHash("actor.lifecycle-policy");
struct ActorLifecyclePolicyV1 {
    // in
    std::uint32_t playerId;
    std::uint32_t dead;
    std::uint32_t respawnsEnabled;
    std::uint32_t pendingRespawn;
    std::uint64_t actorEntity;       // server actor entity for this life
    float respawnSeconds;
    float chosenPosition[3];
    float chosenYaw;
    // out
    std::uint32_t respawn;           // 1 = respawn now
    std::uint32_t handled;
    float position[3];
    float yaw;
    std::uint32_t spawnProtectionTicks; // 0 = none; 60 = 1 second at 60 Hz
};

// net.generation-policy: the hot decision when the client and server hot
// generations differ. Keeps a mismatch from silently wedging world
// participation, and is editable live.
static constexpr std::uint64_t GAME_EVENT_GENERATION_POLICY =
    gameHash("net.generation-policy");
struct GenerationPolicyV1 {
    // in
    std::uint64_t localGeneration;
    std::uint64_t serverGeneration;
    std::uint32_t bootstrapState;
    std::uint32_t mismatch;
    // out
    std::uint32_t allowWorld;    // 1 = participate even if generations differ
    std::uint32_t handled;
};

// net.send-policy: hot decision for whether an outbound server update (snapshot)
// is sent to a player this tick. The wire format stays kernel-owned.
static constexpr std::uint64_t GAME_EVENT_NET_SEND_POLICY =
    gameHash("net.send-policy");
struct NetSendPolicyV1 {
    // in
    std::uint32_t playerId;
    std::uint32_t tick;
    std::uint32_t entityCount;
    std::uint32_t reason;        // 0 snapshot
    float intervalMs;
    // out
    std::uint32_t send;          // 1 = send
    std::uint32_t handled;
};

// net.reliable-policy: hot decision for reliable-event delivery (whether an
// event is queued reliably, retried, and whether a failure may drop the
// connection). Parameters (retryMs/ttl/attempts) remain NetworkingConfig.
static constexpr std::uint64_t GAME_EVENT_NET_RELIABLE_POLICY =
    gameHash("net.reliable-policy");
struct NetReliablePolicyV1 {
    // in
    std::uint32_t playerId;
    std::uint32_t eventId;
    std::uint32_t packetType;
    std::uint32_t attempts;
    std::uint32_t ttlExpired;
    std::uint32_t attemptsExhausted;
    // out
    std::uint32_t reliable;      // 0 = send best-effort instead of reliable
    std::uint32_t retry;         // 0 = do not retry this event
    std::uint32_t keepConnection;// 0 = a failure may mark the connection unhealthy
    std::uint32_t handled;
};

// ── Projectile presentation policy ─────────────────────────
// Kernel -> behavior for every active projectile of every owner (player, NPC,
// network). The behavior decides whether it is visible and its trail style;
// the kernel owns the draw. Fixes "invisible projectile that still damages"
// by never gating presentation on the currently equipped weapon.
struct ProjectilePresentV1 {
    std::uint64_t projectileEntity;
    std::uint64_t ownerEntity;
    std::uint32_t source;        // 0=player, 1=npc, 2=network
    std::uint32_t weaponNetworkId;
    float position[3];
    float velocity[3];
    float age;
    float lifetime;
    std::uint32_t exploded;
    // out
    std::uint32_t visible;
    float outTrailEmissionRate;
    float outTrailSize;
    float outTrailEndSize;
    float outTrailAlpha;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// ── Generic projectile impact/expire policy (runtime event) ─────
// Kernel -> behavior when a simulated projectile hits the world, an actor, or
// exhausts its lifetime. The behavior decides the consequence; the kernel owns
// simulation, collision, and networking. This is the seam that lets a new
// projectile behavior exist without a kernel branch per projectile type.
struct ProjectileImpactPolicyV1 {
    std::uint64_t projectileEntity;
    std::uint64_t ownerEntity;
    std::uint64_t victimEntity;
    std::uint64_t projectileTypeId;  // runtime type key (network id or package hash)
    std::uint32_t ownerId;
    std::uint32_t victimId;
    std::uint32_t weaponNetworkId;
    std::uint32_t hitKind;      // 0 none, 1 world, 2 player, 3 npc, 4 lifetime
    float position[3];
    float normal[3];
    float age;
    float lifetime;
    // out
    std::uint32_t outExplode;   // 1 = run the kernel explosion/damage path
    std::uint32_t handled;
    std::uint32_t reserved;
};

// ── Generic tool/action use policy (runtime event) ──────────────
// Kernel -> behavior for one held primary or alternate use of a held tool.
// The behavior reads the tool's components/relationships and may own the use
// (outFire = 0 suppresses the built-in fire; = 1 allows it).
struct ToolUsePolicyV1 {
    std::uint64_t userEntity;
    std::uint64_t toolEntity;
    std::uint64_t toolId;       // runtime tool key (network id or package hash)
    std::uint32_t ownerId;
    std::uint32_t toolNetworkId;
    std::uint32_t kind;         // 0 primary, 1 alt
    std::uint32_t tick;
    std::uint32_t baseFire;
    std::uint32_t outFire;
    std::uint32_t ammoCost;
    std::uint32_t handled;
    float origin[3];
    float direction[3];
    std::uint32_t reserved;
    // Generic prediction correlation key (0 = not a predicted action). Carried
    // unchanged from the originating client request so a tool behavior can link
    // the authoritative entity it creates back to the predicted one. Opaque and
    // type-agnostic; no weapon category is implied.
    std::uint64_t predictionKey;
};

// ── Generic surface effect / decal (hot policy -> cold mechanism) ──
// Hot policy describes a mark on a world surface (blood, bullet hole, scorch,
// paint, graffiti, arbitrary runtime mark). The kernel owns projection/geometry/
// storage/draw; it never learns what the mark means. No feature-specific enum.
struct GameSurfaceEffectV1 {
    float position[3];
    float normal[3];
    float axis[3];          // in-plane orientation hint (0 = default)
    float color[4];
    float radius;           // decal radius
    float height;           // used by strip-like marks
    float rotation;
    float lifetime;         // seconds (0 = backend default)
    float fadeTime;         // seconds
    std::uint64_t sourceEntity;
    std::uint32_t flags;    // bit0 = persistent (survives, fades)
    std::uint32_t reserved;
    // Append-only: textured decal (bullet holes, cracks, blood splats). Empty =
    // untextured generic mark.
    char texture[GAME_EFFECT_STRING];
    float textureScale;
    // 0 = generic round mark, 1 = blood splat, 2 = bullet hole, 3 = crack strip.
    std::uint32_t decalKind;
};
using GameSurfaceEffectFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameSurfaceEffectV1* request);

// ── Generic effect request (cold -> hot) ────────────────────────
// The client emits one generic fact when a visual effect should be composed.
// A hot handler reads it, creates generic effect entities/commands, and sets
// handled = 1 so the cold fallback composition yields. `effectTypeId` is a
// runtime key (gameHash("effect.explosion") etc.); no effect enum exists.
struct EffectRequestV1 {
    std::uint64_t sourceEntity;
    std::uint64_t effectTypeId;
    std::uint64_t weaponNetworkId;
    float position[3];
    float normal[3];
    float scale;
    float color[4];
    float distance;          // source->observer distance (for falloff policy)
    float falloffDistance;   // 0 = none
    char text[64];           // optional logical name (e.g. a sound)
    std::uint32_t flags;
    std::uint32_t handled;
    // Append-only (effect.request.v2): generic disagreement fact. The kernel
    // forwards the server-disagreement event as plain data; a hot policy may own
    // the whole presentation. No appearance value is read from JSON when handled.
    float correction[3];     // predicted -> corrected delta
    std::uint32_t reason;    // DisagreementReason value (plain number)
    std::uint32_t sourcePlayerId;
    std::uint32_t targetPlayerId;
    std::uint32_t localIndicator;  // 1 = local-only correction indicator
    // Append-only (effect.request.v3): hit-feedback fact so a hot hit recipe can
    // reproduce the full cold HitEffects composition (blood, holes, cracks,
    // impact spheres, damage numbers) with hot-owned appearance.
    std::int32_t damage;
    float directness;        // 0..1, shot alignment with the surface
    float hitDistance;       // -1 = unknown
    std::uint32_t hitEntity; // 1 = entity hit, 0 = world hit
    char victimName[32];
    std::uint32_t spawnDamageNumber;  // 1 = the cold owner would show a number
    std::uint32_t reserved2;
};

// ── Actor death / respawn policy ───────────────────────────
// `authoritative` means a server-controlled life owns the actor, so local
// respawn must not run (the death loop root cause).
struct ActorDeathV1 {
    std::uint64_t actorEntity;
    std::uint32_t isNpc;
    std::uint32_t authoritative;
    std::uint32_t alreadyPresented;
    std::uint32_t deathEventId;
    std::uint32_t deathTick;
    float position[3];
    // out
    std::uint32_t presentCorpse;
    std::uint32_t handled;
    std::uint32_t reserved;
};

struct ActorRespawnV1 {
    std::uint64_t actorEntity;
    std::uint32_t isNpc;
    std::uint32_t authoritative;
    std::uint32_t duelActive;
    float respawnTimer;
    float dt;
    // out
    std::uint32_t allowLocalRespawn;
    std::uint32_t handled;
    std::uint32_t reserved;
};

// ── Connection status / retry policy ───────────────────────
struct ConnectionStateV1 {
    std::uint32_t phase;         // 0=gather 1=coordinator 2=answer 3=connecting 4=done
    std::uint32_t attempt;
    std::uint32_t maxAttempts;
    std::uint64_t elapsedMs;
    std::int32_t lastError;
    // out
    float outBackoffScale;
    char outMessage[96];
    std::uint32_t handled;
    std::uint32_t reserved;
};

using GameEmitEventFn = void (MIMITA_GAME_CALL *)(
    GameplayContextV1* context, const GameEventV1* event);

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

// ── Editor module ───────────────────────────────────────────
// A dedicated hot module that owns creation/inspection selection policy, the
// inspector data formatting, and the overlay layout. The kernel owns the ECS,
// world, spatial query, and UI primitives; the module receives capability
// function pointers and plain data only (no live engine objects).
static constexpr std::uint32_t EDITOR_STATE_VERSION = 1;
static constexpr std::uint32_t EDITOR_MAX_CANDIDATES = 16;
static constexpr std::uint32_t EDITOR_COMPONENT_COUNT = 20;

enum EditorHitKind : std::uint32_t {
    EDITOR_HIT_NONE = 0,
    EDITOR_HIT_WORLD = 1,
    EDITOR_HIT_ENTITY = 2,
};

// One kernel query result (world triangle/object or entity bound) along the ray.
// Layout is frozen: extra per-candidate data arrives through separate
// capabilities, never by growing this struct (keeps hot swaps into an older
// EXE from mis-striding the array).
struct EditorCandidateV1 {
    std::uint32_t kind;        // EditorHitKind
    std::uint32_t domain;      // EntityDomain when kind == ENTITY
    std::uint64_t entity;      // 0 for world
    std::uint32_t worldTriangle;
    float distance;
    float point[3];
};

struct EditorQueryV1 {
    std::uint32_t count;
    std::uint32_t reserved;
    EditorCandidateV1 hits[EDITOR_MAX_CANDIDATES];
};

// POD component/constraint snapshot for one entity (kernel fills; hot formats).
// Layout is frozen (see EditorCandidateV1); extended actor/weapon data arrives
// through EditorInspectExFn.
struct EditorInspectionV1 {
    std::uint32_t valid;
    std::uint32_t realm;
    std::uint32_t domain;
    std::uint32_t legacyId;
    std::uint32_t generation;
    std::uint32_t componentCount;
    char components[EDITOR_COMPONENT_COUNT][32];
    float position[3];
    float yaw;
    std::uint32_t hasConstraint;
    std::uint32_t constraintSerial;
    std::uint32_t constraintActive;
    std::uint32_t constraintType;
    std::uint64_t constraintBodyA;
    std::uint64_t constraintBodyB;
    float constraintStrength;
    std::uint32_t linkedConstraintSerial;
};

// Extended entity inspection (kernel fills into a module-owned buffer). New
// fields are appended only; the module only calls this when the host kernel
// advertises the v2 capability block.
struct EditorInspectionExV1 {
    std::uint32_t valid;
    std::uint32_t hasHealth;
    std::int32_t health;
    std::int32_t maxHealth;
    std::uint32_t dead;
    std::uint32_t hasControl;
    std::uint32_t controlSource;
    std::uint32_t authority;
    std::uint64_t ownerEntity;
    std::uint32_t hasWeapon;
    std::uint32_t weaponNetworkId;
    std::int32_t weaponSlot;
    std::int32_t weaponAmmo;
    char weaponName[48];
    std::uint32_t isRagdollLimb;
    std::uint32_t limbIndex;
    std::uint32_t limbParent;
    std::uint32_t reserved;
};

// World object at a hit point (kernel matches legacy block / GLB mesh batch by
// AABB). Module-owned output buffer.
struct EditorWorldObjectV1 {
    std::uint32_t valid;
    std::uint32_t kind;      // 0 none, 1 block, 2 glb batch, 3 triangle
    std::uint32_t index;
    float center[3];
    float size[3];
    char material[48];
};

// Kernel -> module per-tick input.
struct EditorStateV1 {
    std::uint32_t enabled;
    std::uint32_t tick;
    float origin[3];
    float dir[3];
    float maxDistance;
    std::uint64_t currentSelection;
};

// Module -> kernel result (applied to the kernel-owned editor cache).
struct EditorResultV1 {
    std::uint32_t handled;
    std::uint32_t hitKind;      // EditorHitKind
    std::uint64_t selectedEntity;
    float distance;
    std::uint32_t hasInspection;
    std::uint32_t reserved;
    EditorInspectionV1 inspection;
};

// Map-wide overview (kernel fills; hot formats).
struct EditorMapInfoV1 {
    std::uint32_t valid;
    std::uint32_t blockCount;
    std::uint32_t batchCount;
    std::uint32_t spawnCount;
    std::uint32_t triangleCount;
    float boundsMin[3];
    float boundsMax[3];
    char mapPath[128];
};

// Edge-triggered input for the hot editor (kernel fills every tick).
struct EditorInputV1 {
    std::uint32_t enabled;
    std::uint32_t dropPressed;      // backspace edge
    std::uint32_t interactPressed;  // F edge
    std::uint32_t confirmPressed;   // enter/left-click edge
    std::uint32_t cancelPressed;    // escape/right-click edge
    std::uint32_t copyPressed;
    std::uint32_t pastePressed;
    std::uint32_t deletePressed;
    std::uint32_t rotatePressed;
    std::uint32_t scalePressed;
    // v5 additions: selection + overlap navigation.
    std::uint32_t selectPressed;      // CTRL + left mouse edge
    std::uint32_t cyclePrev;          // wheel up edge
    std::uint32_t cycleNext;          // wheel down edge
    std::uint32_t moveUp;             // vertical movement intent (create mode)
    std::uint32_t moveDown;
};

// Generic fork editing: one op in, one result out. The kernel owns fork storage
// and never rewrites the base map; the hot module owns edit policy/keys.
enum EditorForkOp : std::uint32_t {
    EDITOR_FORK_DUPLICATE = 0,
    EDITOR_FORK_DELETE = 1,
    EDITOR_FORK_SET_TRANSFORM = 2,
    EDITOR_FORK_SET_LABEL = 3,
    EDITOR_FORK_SET_MATERIAL = 4,
    EDITOR_FORK_UNDO = 5,
    EDITOR_FORK_REDO = 6,
};

struct EditorForkArgsV1 {
    std::uint32_t op;             // EditorForkOp
    std::uint32_t sourceKind;     // EditorHitKind (WORLD/ENTITY)
    std::uint32_t sourceWorldKind;
    std::uint32_t sourceWorldIndex;
    std::uint64_t sourceEntity;
    float position[3];
    float rotation[3];
    float scale[3];
    float size[3];                // source object bounds (for fork visualization)
    char label[48];
    char material[48];
    // out
    std::uint32_t ok;
    std::uint32_t reserved;
    std::uint64_t outObjectId;
    std::uint32_t opCount;
    char forkHash[72];
};

using EditorQueryRayFn = void (MIMITA_GAME_CALL *)(
    void* host, const float origin[3], const float dir[3],
    float maxDistance, std::uint32_t maxHits, EditorQueryV1* out);
using EditorInspectFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, EditorInspectionV1* out);
using EditorDrawTextFn = void (MIMITA_GAME_CALL *)(
    void* host, const char* text, float x, float y, float scale, const float rgba[4]);
using EditorDrawRectFn = void (MIMITA_GAME_CALL *)(
    void* host, float x, float y, float w, float h, const float rgba[4]);
using EditorScreenSizeFn = void (MIMITA_GAME_CALL *)(
    void* host, float* outWidth, float* outHeight);
// World-space label projected from a world position (reuses the debug-label
// primitive; the hot module owns the text and timing).
using EditorDrawWorldLabelFn = void (MIMITA_GAME_CALL *)(
    void* host, const float worldPos[3], const char* text, const float rgba[4]);
// Generic outline for any selected identity (entity or world object). The
// kernel owns the draw primitive; the hot module owns color/thickness/cycle.
using EditorDrawOutlineFn = void (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t hitKind, std::uint64_t entity,
    std::uint32_t worldKind, std::uint32_t worldIndex,
    const float rgba[4], float thickness, std::uint32_t throughWalls);
using EditorMapInfoFn = void (MIMITA_GAME_CALL *)(void* host, EditorMapInfoV1* out);
// Generic 3D wire box (fork visualization / selection gizmos). Reuses the
// kernel debug-draw primitive; the hot module owns color/usage.
using EditorDrawWireBoxFn = void (MIMITA_GAME_CALL *)(
    void* host, const float center[3], const float size[3], const float rgba[4]);
// Fork enumeration so hot code can visualize recorded edits.
using EditorForkOpCountFn = std::uint32_t (MIMITA_GAME_CALL *)(void* host);
using EditorForkOpFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint32_t index, EditorForkArgsV1* out);
using EditorForkFn = void (MIMITA_GAME_CALL *)(void* host, EditorForkArgsV1* args);
using EditorInspectExFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, std::uint64_t entity, EditorInspectionExV1* out);
using EditorWorldObjectInfoFn = void (MIMITA_GAME_CALL *)(
    void* host, const float point[3], EditorWorldObjectV1* out);

// Capabilities + kernel-owned state handle. `host` and `permanentStorage` are
// valid only for the duration of the call; never cache them.
struct EditorContextV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    void* host;
    std::uint64_t tick;
    std::uint64_t generation;
    EditorQueryRayFn queryRay;
    EditorInspectFn inspect;
    EditorDrawTextFn drawText;
    EditorDrawRectFn drawRect;
    EditorScreenSizeFn screenSize;
    void* permanentStorage;        // survives hot reloads
    std::uint64_t permanentStorageSize;
    // v2 additions (append-only).
    EditorDrawWorldLabelFn drawWorldLabel;
    EditorDrawOutlineFn drawOutline;
    EditorMapInfoFn mapInfo;
    EditorForkFn fork;
    const EditorInputV1* input;    // valid for the current call
    EditorInspectExFn inspectEx;
    EditorWorldObjectInfoFn worldObjectInfo;
    // v5 additions (append-only). Growing this struct is intentional: a new
    // module loading into an older EXE sees structSize < sizeof and degrades to
    // the legacy path instead of reading past the end.
    std::uint32_t editorAbiVersion;  // 2 = Phase-1 selection/cycle input
    std::uint32_t editorReserved;
    // v6 additions (append-only): fork visualization primitives.
    EditorDrawWireBoxFn drawWireBox;
    EditorForkOpCountFn forkOpCount;
    EditorForkOpFn forkOp;
};

using EditorOnTickFn = void (MIMITA_GAME_CALL *)(
    const EditorStateV1* state, EditorContextV1* context, EditorResultV1* out);
using EditorOnDrawFn = void (MIMITA_GAME_CALL *)(
    const EditorStateV1* state, const EditorResultV1* result, EditorContextV1* context);

struct GameEditorModuleV1 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    EditorOnTickFn onTick;
    EditorOnDrawFn onDraw;
};

// ── Generic runtime package ABI (v5) ────────────────────────────────
// One descriptor per package generation. The kernel registers the descriptor's
// entries generically; it does not know the names/ids inside the arrays. Adding
// a system/event/schema/capability/command later must NOT require a new field in
// this header — only appending to an existing array.
// v2: capability requirements carry a signature so activation can validate
// provider compatibility generically (no capability enum).
static constexpr std::uint32_t MIMITA_PACKAGE_ABI_VERSION = 2;

// Reserved domains the kernel times. Packages may register systems in these or
// in their own hashed domains (which the kernel runs when it times that domain).
static constexpr std::uint64_t GAME_DOMAIN_GAMEPLAY = gameHash("gameplay.60");
static constexpr std::uint64_t GAME_DOMAIN_RENDER = gameHash("render.frame");
// Runs once per frame so hot systems can compose HUD/UI. The kernel only draws
// the generic UI commands they emit; it never knows a gamemode's HUD.
static constexpr std::uint64_t GAME_DOMAIN_UI = gameHash("ui.frame");
// Runs once per fixed tick after movement (resolved generically by the kernel).
static constexpr std::uint64_t GAME_DOMAIN_POST_MOVEMENT = gameHash("postmovement.60");
// Client-only fixed 60 Hz domain. Run only from the client simulation tick, so
// non-authoritative presentation (effect timelines, animations) is never
// simulated on a dedicated server and is tick-rate-stable for everyone.
static constexpr std::uint64_t GAME_DOMAIN_CLIENT_TICK = gameHash("client.tick");

// Kernel primitive capability ids resolved through GameplayContextV1::
// resolveCapability. Generic and reusable; adding a primitive never adds a
// context field.
static constexpr std::uint64_t GAME_CAP_PHYSICS_MOVE = gameHash("physics.move");
// actor.move.npc: hot NPC movement ownership. A hot movement package provides
// this; the cold NPC kernel calls it once per NPC after the AI has written the
// movement intent. It consumes the generic Transform/Velocity/MovementIntent/
// RuntimeState components and writes the moved result back, returning 1 when it
// owns the actor (the cold kernel yields) or 0 to fall back. NPC movement and
// routing become hot-reloadable with no per-actor ABI field.
using GameNpcMoveFn = std::uint32_t (MIMITA_GAME_CALL *)(void* host,
                                                         std::uint64_t entity,
                                                         std::uint32_t tick,
                                                         float dt);
static constexpr std::uint64_t GAME_CAP_NPC_MOVE = gameHash("actor.move.npc");
static constexpr std::uint64_t GAME_CAP_EFFECT_SPAWN = gameHash("effect.spawn");
// Existing pooled EffectPart primitive (textured billboard / decal / beam /
// box / tick sphere). One descriptor, no feature enum.
static constexpr std::uint64_t GAME_CAP_EFFECT_PART = gameHash("effect.part");
static constexpr std::uint64_t GAME_CAP_SKELETON_APPLY = gameHash("skeleton.apply");
static constexpr std::uint64_t GAME_CAP_SKELETON_VALIDATE = gameHash("skeleton.validate");

// tool.definition: ONE generic tool/weapon query. A hot package provides the
// definition (gameplay data + model + sounds + animation phases) for a stable
// tool key; the cold weapon system resolves this capability and applies the
// provided fields, so the hot C++ definition is authoritative and config JSON is
// fallback only. Data is plain POD (fixed arrays, no pointers/STL). Adding a new
// tool never adds an ABI field; it adds a provider entry.
static constexpr std::uint64_t GAME_CAP_TOOL_DEFINITION = gameHash("tool.definition");

enum GameToolFieldFlags : std::uint32_t {
    GAME_TOOL_FIELD_DAMAGE     = 1u << 0,
    GAME_TOOL_FIELD_BEHAVIOR   = 1u << 1,  // behaviorType/fireMode/networkMode/hitscan
    GAME_TOOL_FIELD_TIMING     = 1u << 2,  // fireDelay/reloadTime/equip+unequip pose time
    GAME_TOOL_FIELD_AMMO       = 1u << 3,  // magazine/reserve/pellets/spread/recoil
    GAME_TOOL_FIELD_PROJECTILE = 1u << 4,  // projectileSpeed/radius/lifetime
    GAME_TOOL_FIELD_MODEL      = 1u << 5,  // modelPath/scale/attachment
    GAME_TOOL_FIELD_SOUNDS     = 1u << 6,
    GAME_TOOL_FIELD_SLOT       = 1u << 7,
};

struct GameToolParamV1 {
    char key[24];   // customParams key (e.g. "reserveAmmo", "minDamageFraction")
    float value;
};

struct GameToolDefinitionV1 {
    // in
    std::uint64_t toolKey;       // gameHash(weaponId); 0 + enumerateIndex = list
    std::uint32_t structSize;    // sizeof(GameToolDefinitionV1)
    std::uint32_t enumerateIndex;// when toolKey == 0: Nth hot tool
    // out
    std::uint32_t found;         // 1 = a hot definition exists
    std::uint32_t presentMask;   // GameToolFieldFlags authoritative groups
    std::uint32_t behaviorType;  // WeaponBehaviorType numeric
    std::uint32_t fireMode;      // WeaponFireMode numeric
    std::uint32_t networkMode;   // WeaponNetworkMode numeric
    std::uint32_t hitscan;       // 1 = hitscan execution
    std::uint32_t slot;
    float damage;
    float headshotMultiplier;
    float fireDelay;
    float reloadTime;
    float spread;
    float recoil;
    float equipPoseTime;
    float unequipPoseTime;
    float projectileSpeed;
    float projectileRadius;
    float projectileLifetime;
    std::int32_t magazineSize;
    std::int32_t reserveAmmo;
    std::int32_t pelletCount;
    std::uint32_t paramCount;    // number of valid entries in params[]
    char id[32];                 // stable weapon id (registration key)
    char displayName[48];
    char modelPath[192];
    std::uint64_t socket;
    float attachmentPosition[3];
    float attachmentRotation[3]; // euler degrees
    float scale;
    char soundShoot[64];
    char soundReload[64];
    char soundEquip[64];
    char soundUnequip[64];
    char soundHit[64];
    char soundDryFire[64];
    GameToolParamV1 params[8];   // extra gameplay customParams
    std::uint32_t reserved2[8];
};
using GameToolDefinitionQueryFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameToolDefinitionV1* request);
static constexpr std::uint64_t GAME_CAP_ANIMATION_UPDATE = gameHash("animation.update");

// world.collision: paginated dump of the map's collision triangles. This is the
// one cold geometry primitive that lets hot code build its own spatial index and
// own broadphase/narrowphase, collision resolution, and the ragdoll solver so
// those algorithms and live world edits are hot. The kernel keeps the map
// triangle data; hot code owns everything above it.
static constexpr std::uint64_t GAME_CAP_WORLD_COLLISION = gameHash("world.collision");
static constexpr std::uint32_t GAME_MAX_COLLISION_TRIS = 4096;
struct GameCollisionTriangleV1 {
    float a[3];
    float b[3];
    float c[3];
    float normal[3];
};
struct GameWorldCollisionPageV1 {
    std::uint32_t offset;         // in: first triangle index
    std::uint32_t maxTriangles;   // in: capacity of `out`
    std::uint32_t total;          // out: total triangles in the map
    std::uint32_t count;          // out: triangles written
    GameCollisionTriangleV1* out; // in: caller buffer (maxTriangles)
};
using GameWorldCollisionFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GameWorldCollisionPageV1* page);

// physics.capsuleSolve: optional hot provider that replaces the kernel capsule
// solve. When a hot package registers a provider, the kernel calls it with the
// capsule state instead of running Physics::moveCapsuleStep. The provider owns
// broadphase/narrowphase over `world.collision` and writes the resolved state.
// When no provider is registered, the kernel solve runs unchanged.
static constexpr std::uint64_t GAME_CAP_PHYSICS_CAPSULE_SOLVE =
    gameHash("physics.capsuleSolve");
struct GameCapsuleSolveV1 {
    // in
    float position[3];
    float velocity[3];
    float yaw;
    float radius;
    float halfHeight;   // tip-to-tip half extent
    float sizeScale;
    float gravityScale; // negative = caller already integrated gravity
    float dt;
    // in: kernel geometry access. Call collisionFn(collisionHost, &page) only
    // when actually solving; it is null when no world is bound.
    GameWorldCollisionFn collisionFn;
    void* collisionHost;
    // out
    float outPosition[3];
    float outVelocity[3];
    std::uint32_t grounded;
    std::uint32_t collided;
    std::uint32_t handled;   // 0 = provider declines; kernel solve runs
    std::uint32_t reserved;
};
using GameCapsuleSolveFn = void (MIMITA_GAME_CALL *)(void* host,
                                                     GameCapsuleSolveV1* state);

// physics.impulse: generic apply-impulse primitive for hot collision / ragdoll
// solvers. Adds linear (and optional angular) velocity to an entity's body.
static constexpr std::uint64_t GAME_CAP_PHYSICS_IMPULSE = gameHash("physics.impulse");
struct GamePhysicsImpulseV1 {
    std::uint64_t entity;
    float linear[3];
    float angular[3];
    std::uint32_t applied;
    std::uint32_t reserved;
};
using GamePhysicsImpulseFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GamePhysicsImpulseV1* request);

// ragdoll.solve: ONE generic seam that lets a hot module own the ragdoll solver
// algorithm. The cold host fills the limb/joint/grab snapshot each substep and
// calls the capability; if the hot side sets `handled`, the host applies the
// returned limb states and skips the cold solver (which stays as the fallback
// until the hot solver is proven). Plain POD, fixed bounds, no pointers.
static constexpr std::uint64_t GAME_CAP_RAGDOLL_SOLVE = gameHash("ragdoll.solve");
static constexpr std::uint32_t GAME_MAX_RAGDOLL_LIMBS = 24;

struct GameRagdollLimbStateV1 {
    float position[3];
    float orientation[4];      // quaternion, [0]=w
    float linearVelocity[3];
    float angularVelocity[3];
};
struct GameRagdollLimbStaticV1 {
    std::uint32_t parentIndex;      // 0xffffffff = root
    std::uint32_t hasRotationLimits;
    float inverseMass;
    float radius;
    float halfHeight;
    float parentLocalAnchor[3];
    float childLocalAnchor[3];
    float restLength;
    float maxStretch;
    float bindRotation[4];          // quaternion, [0]=w
    float rotMinDeg[3];
    float rotMaxDeg[3];
};
struct GameRagdollGrabV1 {
    std::uint32_t active;
    std::uint32_t limbIndex;
    std::int32_t targetLimb;        // -1 = world anchor
    float grabPoint[3];
    float grabNormal[3];
    float handLocalAnchor[3];
    float targetLocalAnchor[3];
    float strength;
};
struct GameRagdollSolveV1 {
    // in
    std::uint32_t structSize;
    std::uint32_t limbCount;
    float dt;
    float gravityScale;
    float stiffness;
    float damping;
    std::uint32_t iterations;
    GameRagdollLimbStateV1 limbs[GAME_MAX_RAGDOLL_LIMBS];
    GameRagdollLimbStaticV1 statics[GAME_MAX_RAGDOLL_LIMBS];
    GameRagdollGrabV1 grabLeft;
    GameRagdollGrabV1 grabRight;
    // out
    std::uint32_t handled;   // 1 = hot owned the solve; limbs[] are results
    std::uint32_t applied;
};
using GameRagdollSolveFn = void (MIMITA_GAME_CALL *)(void* host,
                                                     GameRagdollSolveV1* solve);

// input.read: hot code reads the current local input state (movement wish,
// held/edge actions, ragdoll grab/extend). The kernel owns polling and the
// key binding; the hot module owns what it does with the input.
static constexpr std::uint64_t GAME_CAP_INPUT_READ = gameHash("input.read");
struct GameInputStateV1 {
    float wishMoveX;
    float wishMoveY;
    float camForward[3];
    float movementHeldDuration;
    std::uint32_t jumpHeld;
    std::uint32_t jumpPressed;
    std::uint32_t dashPressed;
    std::uint32_t movementPressed;
    std::uint32_t movementJustPressed;
    std::uint32_t groundReturnPressed;
    std::uint32_t downDashPressed;
    std::uint32_t freezeHeld;
    std::uint32_t freezePressed;
    std::uint32_t ragdollTogglePressed;
    std::uint32_t grabLeftHeld;
    std::uint32_t grabRightHeld;
    std::uint32_t extendLeftMouse;
    std::uint32_t extendRightMouse;
    std::uint32_t reserved;
};
using GameInputReadFn = bool (MIMITA_GAME_CALL *)(void* host, GameInputStateV1* out);

// camera.read: hot code reads the current camera transform (position/axes/yaw/
// pitch/fov). The kernel owns the camera; hot presentation/physics policy reads
// it (e.g. ragdoll head aim, look motors).
static constexpr std::uint64_t GAME_CAP_CAMERA_READ = gameHash("camera.read");
struct GameCameraStateV1 {
    float position[3];
    float front[3];
    float up[3];
    float right[3];
    float yaw;
    float pitch;
    float fov;
    std::uint32_t valid;
    std::uint32_t reserved[3];
};
using GameCameraReadFn = bool (MIMITA_GAME_CALL *)(void* host, GameCameraStateV1* out);

// actor.skeleton.write: let a hot module (e.g. a hot ragdoll orchestrator) write
// the authoritative actor root + skeleton node local transforms back to the
// typed actor. The caller supplies final matrices (it owns the math); the kernel
// owns the Player storage, node mapping, and world-transform update. Plain POD.
static constexpr std::uint64_t GAME_CAP_ACTOR_SKELETON_WRITE =
    gameHash("actor.skeleton.write");
static constexpr std::uint32_t GAME_MAX_SKELETON_NODES = 64;
struct GameActorSkeletonNodeV1 {
    std::int32_t nodeIndex;
    float local[16];
};
struct GameActorSkeletonWriteV1 {
    std::uint64_t actorEntity;      // local player entity (from shared state)
    float rootPosition[3];
    float rootRotation[4];          // quaternion [0]=w
    float rootVelocity[3];
    std::uint32_t rootRotationActive;  // 1 = model root uses rootRotation
    std::uint32_t ancestorCount;
    std::int32_t ancestorNodes[8];  // neutralized (identity) non-part ancestors
    std::uint32_t nodeCount;
    GameActorSkeletonNodeV1 nodes[GAME_MAX_SKELETON_NODES];
    std::uint32_t applied;
    // Append-only: when ownerActorId != 0 the write targets that remote actor
    // (from the multiplayer context); otherwise the local actor.
    std::uint32_t ownerActorId;
    std::uint32_t isNpc;
    std::uint32_t reserved2;
};
using GameActorSkeletonWriteFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameActorSkeletonWriteV1* write);

// ragdoll.snapshot: read/write the per-owner ragdoll limb snapshot (the same POD
// the network codec uses) so a hot presenter can consume remote frames and a hot
// sender can emit them. op 0 = read (kernel fills), op 1 = write (kernel
// applies). Plain POD, fixed bounds.
static constexpr std::uint64_t GAME_CAP_RAGDOLL_SNAPSHOT = gameHash("ragdoll.snapshot");
static constexpr std::uint32_t GAME_MAX_RAGDOLL_SNAPSHOT_LIMBS = 24;
struct GameRagdollSnapshotLimbV1 {
    std::uint32_t limbIndex;
    float position[3];
    float rotation[4];   // quaternion [0]=w
};
struct GameRagdollSnapshotGrabV1 {
    std::uint8_t active;
    std::uint8_t hand;
    std::uint16_t reserved;
    std::uint32_t targetLimb;   // 0xffffffff = world
    float anchor[3];
    float handLocal[3];
    float strength;
};
struct GameRagdollSnapshotV1 {
    std::uint32_t op;            // 0 = read, 1 = write
    std::uint32_t ownerActorId;
    std::uint32_t limbCount;
    std::uint32_t reserved;
    std::uint64_t tick;
    GameRagdollSnapshotLimbV1 limbs[GAME_MAX_RAGDOLL_SNAPSHOT_LIMBS];
    GameRagdollSnapshotGrabV1 grabs[2];
};
using GameRagdollSnapshotFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameRagdollSnapshotV1* snapshot);

// ragdoll.bind: build the ragdoll body template for an actor (limb -> skeleton
// node mapping, mesh-local transforms, joint anchors, rotation limits, grab
// indices). A hot presentation/orchestrator uses it to map solved limb
// transforms onto the typed skeleton without seeing the cold body builder.
static constexpr std::uint64_t GAME_CAP_RAGDOLL_BIND = gameHash("ragdoll.bind");
static constexpr std::uint32_t GAME_MAX_RAGDOLL_PARTS = 24;
struct GameRagdollPartV1 {
    std::uint64_t nameHash;
    std::int32_t nodeIndex;
    std::int32_t skeletonParentPart;
    std::int32_t parentIndex;
    std::uint32_t hasRotationLimits;
    float meshLocal[16];
    float parentLocalAnchor[3];
    float childLocalAnchor[3];
    float restLength;
    float maxStretch;
    float bindRotation[4];
    float rotMinDeg[3];
    float rotMaxDeg[3];
};
struct GameRagdollTemplateV1 {
    std::uint64_t actorEntity;
    std::uint32_t partCount;
    std::uint32_t ancestorNodeCount;
    std::int32_t torsoIndex;
    std::int32_t headIndex;
    std::int32_t leftArmIndex;
    std::int32_t rightArmIndex;
    std::int32_t leftLegIndex;
    std::int32_t rightLegIndex;
    std::int32_t ancestorNodes[8];
    float rootOffsetLocal[3];
    std::uint32_t valid;
    GameRagdollPartV1 parts[GAME_MAX_RAGDOLL_PARTS];
};
using GameRagdollBindFn = bool (MIMITA_GAME_CALL *)(void* host,
                                                    GameRagdollTemplateV1* out);

// ragdoll.presentation: hot remote-ragdoll presentation. The cold presenter
// calls this per remote owner; a hot presenter interpolates the buffered
// snapshots and writes the typed actor via actor.skeleton.write, returning
// handled = 1 so the cold presenter yields. Plain POD.
static constexpr std::uint64_t GAME_CAP_RAGDOLL_PRESENT =
    gameHash("ragdoll.presentation");
struct GameRagdollPresentV1 {
    std::uint32_t ownerActorId;
    std::uint32_t isNpc;
    std::uint64_t actorEntity;
    double delaySeconds;
    double nowSeconds;
    std::uint32_t handled;
    std::uint32_t reserved;
};
using GameRagdollPresentFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GameRagdollPresentV1* present);
// ragdoll.aim: hot alive-ragdoll motor policy. The cold orchestrator fills the
// head/torso indices, the camera look basis, the per-limb aim offsets and the
// current limb angular velocities; a hot module computes the damped aim response
// and returns updated angular velocities (handled = 1) so the cold applyControls
// yields. Plain POD, fixed bounds, no pointers.
static constexpr std::uint64_t GAME_CAP_RAGDOLL_AIM = gameHash("ragdoll.aim");
struct GameRagdollAimLimbV1 {
    float orientation[4];    // quaternion, [0]=w
    float angularVelocity[3];
    float aimOffset[4];      // quaternion, [0]=w
};
struct GameRagdollAimV1 {
    // in
    std::uint32_t structSize;
    std::uint32_t headIndex;      // 0xffffffff = none
    std::uint32_t torsoIndex;     // 0xffffffff = none
    float cameraFront[3];
    float cameraUp[3];
    float headStrength;
    float headMaxSpeed;
    float torsoStrength;
    float torsoMaxSpeed;
    float lookDamping;
    float dt;
    std::uint32_t limbCount;
    GameRagdollAimLimbV1 limbs[GAME_MAX_RAGDOLL_LIMBS];
    // out
    std::uint32_t handled;   // 1 = hot owned the aim; limbs[] hold angularVel
    std::uint32_t reserved;
};
using GameRagdollAimFn = void (MIMITA_GAME_CALL *)(void* host,
                                                   GameRagdollAimV1* aim);
// Generic authoritative server-context primitives. These let hot code mutate
// authoritative world state through stable generic handles; the kernel keeps
// ownership of the players/projectiles containers and networking.
static constexpr std::uint64_t GAME_CAP_PROJECTILE_SPAWN = gameHash("projectile.spawn");
static constexpr std::uint64_t GAME_CAP_DAMAGE_APPLY = gameHash("damage.apply");
// Generic presentation command surface. A hot render.frame system resolves this
// capability and submits generic debug/presentation geometry (lines, wire boxes,
// wire spheres, world labels); the kernel owns the low-level draw. Adding a new
// shape kind is a new `shape` value, never a new context field or call site.
static constexpr std::uint64_t GAME_CAP_RENDER_DEBUG = gameHash("render.debug");
// Generic mesh presentation primitive. Hot systems submit logical resource ids;
// the kernel resolves handles and draws. No new context field.
static constexpr std::uint64_t GAME_CAP_RENDER_MESH = gameHash("render.mesh");
// Generic HUD/UI command surface. Hot ui.frame systems emit widgets; the kernel
// owns the low-level font/rect/texture draw. No gamemode-specific UI type.
static constexpr std::uint64_t GAME_CAP_RENDER_UI = gameHash("render.ui");
// Generic audio command. Hot policy chooses the sound/volume/pitch/falloff; the
// kernel owns the device/mixer/playback. No per-weapon/per-feature audio ABI.
static constexpr std::uint64_t GAME_CAP_AUDIO_PLAY = gameHash("audio.play");
// Generic surface-effect/decal primitive. Hot policy describes a mark; the kernel
// projects/renders it without knowing what it means.
static constexpr std::uint64_t GAME_CAP_SURFACE_EFFECT = gameHash("surface.effect");
// Generic camera-effect primitive. Hot policy decides amplitude/falloff; the
// kernel applies a temporary camera perturbation. No explosion/damage branch.
static constexpr std::uint64_t GAME_CAP_CAMERA_EFFECT = gameHash("camera.effect");

// Generic named-attachment-point query. "Give me the current world transform of
// attachment point X on entity Y." The kernel composes the entity transform with
// the entity's current generic skeleton pose (SkeletonInstances) and, when the
// entity's drawn mesh tags that part, its mesh bind transform. When the named
// point is absent the entity transform + caller local offset is used, so the
// same primitive works for skeletal actors, plain props, and tool entities.
// Generic across held tools, hats, carried props, muzzle points, bone particles.
static constexpr std::uint64_t GAME_CAP_SOCKET_QUERY = gameHash("socket.query");
struct GameSocketQueryV1 {
    // in
    std::uint64_t entity;          // parent entity
    std::uint64_t socket;          // gameHash("rightArm") / gameHash("muzzle")
    float localPosition[3];        // local offset applied after the socket
    float localRotation[4];        // quaternion xyzw (identity = {0,0,0,1})
    float localScale[3];           // <=0 treated as 1
    // out
    float position[3];
    float rotation[4];
    float scale[3];
    std::uint32_t found;           // 1 = named socket/bone matched
    std::uint32_t usedFallback;    // 1 = composed from entity transform only
    std::uint32_t valid;           // 1 = a transform was produced
    std::uint32_t reserved;
};
using GameSocketQueryFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameSocketQueryV1* query);

// Generic RAW attachment query: the attachment point in the ENTITY-LOCAL frame
// (skeleton bone pose + mesh bind), with NO entity transform and NO yaw. Lets hot
// policy compose the final transform itself (units, grip, mount), so rotation and
// grip bugs are fixable live in C++. Same inputs as socket.query minus the local
// offset, which hot applies.
static constexpr std::uint64_t GAME_CAP_SOCKET_RAW = gameHash("socket.raw");
struct GameSocketRawV1 {
    // in
    std::uint64_t entity;
    std::uint64_t socket;
    // out (entity-local)
    float position[3];
    float rotation[4];             // quaternion xyzw
    std::uint32_t found;
    std::uint32_t valid;
    std::uint32_t reserved[2];
};
using GameSocketRawFn = bool (MIMITA_GAME_CALL *)(void* host, GameSocketRawV1* q);

// Generic effect-pool access. Hot policy reads/writes/ages the EXISTING pooled
// effect storage (surface decals, blood particles) at a fixed tick, and can
// claim aging so the kernel stops aging (one owner). Storage/draw stay in the
// kernel; behavior is hot. No new feature slot.
static constexpr std::uint64_t GAME_CAP_EFFECT_POOL = gameHash("effect.pool");
enum GameEffectPoolKind : std::uint32_t {
    GAME_EFFECT_POOL_PARTS = 0,
    GAME_EFFECT_POOL_DECALS = 1,
    GAME_EFFECT_POOL_BLOOD = 2,
};
enum GameEffectPoolOp : std::uint32_t {
    GAME_EFFECT_POOL_COUNT = 0,
    GAME_EFFECT_POOL_GET = 1,
    GAME_EFFECT_POOL_SET = 2,
    GAME_EFFECT_POOL_KILL = 3,
    GAME_EFFECT_POOL_CLAIM = 4,   // count != 0 => hot owns aging
};
struct GameEffectPoolV1 {
    std::uint32_t op;
    std::uint32_t kind;
    std::uint32_t index;
    std::uint32_t count;      // out (COUNT)
    std::uint32_t alive;      // out (GET)
    std::uint32_t reserved;
    float position[3];
    float velocity[3];
    float normal[3];
    float axis[3];
    float color[3];
    float alpha;
    float scale;              // decal radius / particle size
    float height;             // decal strip height
    float age;
    float lifetime;
    float fadeTime;
    float gravity;
    float drag;
    float rotation;
    float stretch;
    float textureScale;
    std::uint32_t decalKind;  // SurfaceDecalKind
    std::uint32_t flags;      // bit0 = generic (untextured) mark
    char texturePath[GAME_EFFECT_STRING];
};
using GameEffectPoolFn = bool (MIMITA_GAME_CALL *)(void*, GameEffectPoolV1*);

// Generic local AABB for a logical presentation mesh. Hot policy computes grip
// recentre / mount from the model bounds (the same data the cold viewmodel used).
static constexpr std::uint64_t GAME_CAP_MESH_BOUNDS = gameHash("mesh.bounds");
struct GameMeshBoundsV1 {
    // in
    std::uint64_t meshResourceId;
    // out (model local space)
    float boundsMin[3];
    float boundsMax[3];
    std::uint32_t valid;
    std::uint32_t reserved;
};
using GameMeshBoundsFn = bool (MIMITA_GAME_CALL *)(void* host,
                                                   GameMeshBoundsV1* q);

// Generic setting access seam. Hot UI reads/writes real engine settings by
// logical id; the kernel maps the id to the actual config field and applies
// validity constraints (clamp/reject). Hot code never sees a SettingsManager*.
enum GameSettingType : std::uint32_t {
    GAME_SETTING_FLOAT = 1,
    GAME_SETTING_INT = 2,
    GAME_SETTING_BOOL = 3,
    // Discrete option identified by an index into the kernel-provided option
    // list (optionCount + optionLabel). Not a formatted string value.
    GAME_SETTING_OPTION = 4,
};
static constexpr std::uint64_t GAME_CAP_SETTING_GET = gameHash("setting.get");
static constexpr std::uint64_t GAME_CAP_SETTING_SET = gameHash("setting.set");
struct GameSettingV1 {
    std::uint64_t settingId;   // gameHash("video.fov") etc.
    std::uint32_t type;        // GameSettingType (out for GET, in for SET)
    float floatValue;
    std::int32_t intValue;     // option index for GAME_SETTING_OPTION
    std::uint32_t ok;          // 1 = known setting / applied
    std::uint32_t optionCount; // out (GET, OPTION): number of choices
    char optionLabel[24];      // out (GET, OPTION): current option label
    std::uint32_t reserved;
};
using GameSettingGetFn = bool (MIMITA_GAME_CALL *)(void* host, GameSettingV1* s);
using GameSettingSetFn = bool (MIMITA_GAME_CALL *)(void* host, GameSettingV1* s);

// Generic world->screen projection mechanism. Hot overlay/UI policy supplies a
// world position; the kernel projects it through the live camera and returns a
// screen position + in-front flag. The kernel never knows what the overlay is
// (nameplate, health bar, objective label, damage indicator, editor gizmo).
static constexpr std::uint64_t GAME_CAP_WORLD_PROJECT = gameHash("world.project");
struct GameWorldProjectV1 {
    float worldPosition[3];
    float viewportWidth;    // 0 = kernel viewport
    float viewportHeight;
    // out
    float screenX;
    float screenY;
    float depth;            // clip-space w (eye distance)
    std::uint32_t visible;  // 1 = in front of the camera
    std::uint32_t reserved;
};
using GameWorldProjectFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameWorldProjectV1* project);

// Generic logical-presentation-resource registration. Lets hot code register an
// arbitrary logical mesh/texture id backed by an asset path in the existing
// generation-aware provider; the kernel owns parsing, validation, generation
// swap, and last-good preservation. No per-weapon/per-tool resource slot.
enum GameResourceKind : std::uint32_t {
    GAME_RESOURCE_MESH = 1,      // GLB mesh
    GAME_RESOURCE_TEXTURE = 2,   // image texture
};
static constexpr std::uint64_t GAME_CAP_RESOURCE_REGISTER =
    gameHash("resource.register");
struct GameResourceRegisterV1 {
    std::uint64_t logicalId;   // package-chosen logical resource id
    std::uint32_t kind;        // GameResourceKind
    std::uint32_t applyNow;    // 1 = load immediately, 0 = lazy
    char path[192];            // asset path (mesh GLB / texture image)
    // out
    std::uint32_t ok;
    std::uint32_t generation;
    std::uint32_t reserved[2];
};
using GameResourceRegisterFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameResourceRegisterV1* request);

struct GameCameraEffectV1 {
    float pitch;              // rotational impulse (radians)
    float yaw;
    float falloffDistance;    // 0 = no distance attenuation
    float distance;           // source->camera distance for falloff
    std::uint64_t sourceEntity;
    std::uint64_t runtimeKey; // e.g. gameHash("effect.camera.shake")
    std::uint32_t flags;
    std::uint32_t reserved;
};
using GameCameraEffectFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameCameraEffectV1* effect);

// One audio command. `sound` is a logical sound name (resolved by the cold audio
// backend); the remaining fields are policy the hot system owns. `spatial` uses
// position + maxDistance falloff; otherwise it is a 2D/UI sound.
// Generic audio lifecycle ops. One-shot playback plus a logical persistent slot
// keyed by (ownerEntity, slotId): the hot side owns desired state, the cold side
// maps it to a physical voice (idempotent SET, safe STOP, entity-death cleanup).
enum GameAudioOp : std::uint32_t {
    GAME_AUDIO_PLAY_ONESHOT = 0,   // fire-and-forget
    GAME_AUDIO_SET_SLOT = 1,       // desired loop state for (owner,slot)
    GAME_AUDIO_STOP_SLOT = 2,      // stop (owner,slot)
};
struct GameAudioCommandV1 {
    char sound[64];
    float position[3];
    float volume;
    float pitch;
    float maxDistance;
    std::uint32_t spatial;
    std::uint32_t action;   // reserved: 0 play (loop/stop later)
    // Generic slot lifecycle (append-only). slotId == 0 => PLAY_ONESHOT.
    std::uint64_t ownerEntity;  // 0 = global/none
    std::uint64_t slotId;       // opaque logical slot id (hash), no enum
    std::uint32_t op;           // GameAudioOp
    std::uint32_t loop;         // SET_SLOT: 1 = loop
};
using GameAudioPlayFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameAudioCommandV1* command);
// Generic round-based match mechanism: a hot mode records the winner of one
// round. The kernel owns round tallying, the RESULTS transition, and the
// match-over decision; no mode-specific finish callback or round field.
struct GameMatchRoundResultV1 {
    std::uint32_t winnerTeam;
    std::uint32_t reasonHash;   // advisory gameHash("objective.exploded") etc.
    std::uint32_t tick;
    std::uint32_t handled;
    std::uint32_t outMatchOver;
    std::uint32_t reserved;
};
using GameMatchRoundResultFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameMatchRoundResultV1* request);
static constexpr std::uint64_t GAME_CAP_MATCH_ROUND_RESULT =
    gameHash("match.round-result");
// Generic map spatial anchors: the kernel projects map metadata (currently
// objective sites) as plain points + radius + kind. Hot modes create their own
// entities from these; the kernel never owns an objective site or a site slot.
static constexpr std::uint32_t GAME_MAX_MAP_ANCHORS = 32;
struct GameMapAnchorV1 {
    float position[3];
    float radius;
    std::uint64_t kind;   // gameHash("objective.site") / gameHash("spawn.team")
    std::uint32_t tag;    // generic per-kind index (e.g. team index); 0 = none
    float yaw;            // optional facing for spawn anchors
};
using GameMapAnchorsFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, GameMapAnchorV1* out, std::uint32_t maxOut);
static constexpr std::uint64_t GAME_CAP_MAP_ANCHORS = gameHash("map.anchors");
// Generic authoritative actor spawn/reset mechanism: the mode supplies where and
// the kernel performs the mutation (generic Transform/Velocity/health/dead and
// the typed projections the client snapshot still reads). No mode/player/NPC
// specific spawn function.
struct GameActorSpawnV1 {
    std::uint64_t actorEntity;
    float position[3];
    float velocity[3];
    float yaw;
    std::int32_t health;     // 0 = keep current
    std::uint32_t flags;     // bit0 reset velocity, bit1 clear dead, bit2 reset health
    std::uint32_t applied;
    std::uint32_t reserved;
};
using GameActorSpawnFn = bool (MIMITA_GAME_CALL *)(void* host, GameActorSpawnV1* request);
static constexpr std::uint64_t GAME_CAP_ACTOR_SPAWN = gameHash("actor.spawn");

// Generic authoritative projectile spawn. The kernel owns id allocation,
// simulation, collision, and replication; the spec carries only generic data
// (no weapon/projectile enum). `typeId` is a runtime key the package chooses.
struct GameProjectileSpawnSpecV1 {
    std::uint64_t ownerEntity;
    std::uint64_t typeId;
    std::uint32_t ownerPlayerId;
    std::uint32_t ownerNpcId;
    std::uint32_t weaponNetworkId;
    float position[3];
    float velocity[3];
    float radius;
    float lifetime;
    float gravity;
    float drag;
    float restitution;
    std::uint32_t maxBounceCount;
    std::uint32_t explodeOnPlayerImpact;
    std::uint32_t explodeOnWorldImpact;
    std::uint32_t explodeOnLifetime;
    // Optional area effect (generic; zero = no splash).
    float splashRadius;
    float splashDamage;
    float splashExponent;
    float fullDamageRadius;
    float edgeDamage;
    float knockbackStrength;
    float selfDamageMultiplier;
    std::uint32_t splashEnabled;
};
using GameProjectileSpawnFn = bool (MIMITA_GAME_CALL *)(
    void* host, const GameProjectileSpawnSpecV1* spec, std::uint64_t* outEntity);

// Generic authoritative damage application by entity id. The caller supplies a
// generic source kind; there are no weapon-specific damage callbacks.
struct GameDamageApplyV1 {
    std::uint64_t victimEntity;
    std::uint64_t sourceEntity;
    std::int32_t amount;
    std::uint32_t sourceKind;   // GameDamageSource
    float knockback[3];
    // out
    std::uint32_t applied;
    std::uint32_t killed;
    std::int32_t healthAfter;
    std::uint32_t reserved;
};
using GameDamageApplyFn = bool (MIMITA_GAME_CALL *)(
    void* host, GameDamageApplyV1* request);

// Generic presentation command for hot render systems. One shape vocabulary so
// wireframe/debug/outline policy can live in hot code while the kernel keeps the
// GPU mechanism: hot decides what/where/color; the kernel draws.
enum GameRenderDebugShape : std::uint32_t {
    GAME_RENDER_DEBUG_LINE = 1,
    GAME_RENDER_DEBUG_WIRE_BOX = 2,
    GAME_RENDER_DEBUG_WIRE_SPHERE = 3,
    GAME_RENDER_DEBUG_WORLD_LABEL = 4,
    // Screen-space HUD text. a[0]/a[1] = screen x/y, radius = scale, text = text.
    GAME_RENDER_DEBUG_HUD_TEXT = 5,
};
struct GameRenderDebugCommandV1 {
    std::uint32_t shape;      // GameRenderDebugShape
    std::uint32_t flags;      // reserved
    float a[3];               // line start / box center / sphere center / label pos
    float b[3];               // line end
    float half[3];            // box half extents
    float radius;             // sphere radius
    float color[4];           // rgba
    std::uint64_t ownerEntity;
    char text[64];            // world label
};
using GameRenderDebugFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameRenderDebugCommandV1* command);

// Generic mesh presentation command for hot render systems. The command carries
// logical resource ids; the kernel resolves the current generation handle and
// draws with the shared shader. No Player/NPC/Projectile/weapon branch.
// flags bit0: the command transform is camera-relative (VIEW space) instead of
// world space. The cold renderer keeps the view/projection mechanism; hot policy
// keeps position/rotation/visibility. No "this is the X viewmodel" branch.
static constexpr std::uint32_t GAME_RENDER_MESH_SPACE_VIEW = 1u;

struct GameRenderMeshCommandV1 {
    std::uint64_t entity;
    std::uint64_t meshResourceId;
    std::uint64_t textureResourceId;    // 0 = untextured solid color
    std::uint64_t shaderResourceId;     // 0 = shared basic shader
    float position[3];
    float rotation[4];                  // quaternion xyzw
    float scale[3];
    float color[4];
    std::uint32_t flags;                // GAME_RENDER_MESH_SPACE_VIEW etc.
    std::uint32_t reserved;
    // Optional UI clip rect (x,y,w,h; w/h <= 0 = none). Generic 3D-in-UI: a mesh
    // draw bound to a UI rectangle (viewport + scissor). Reusable for avatar/
    // inventory previews, editor viewports, spectator thumbnails.
    float uiClip[4];
};
using GameRenderMeshFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameRenderMeshCommandV1* command);

// Generic UI/HUD widget command. Hot ui.frame systems emit a list of these; the
// kernel draws them with the immediate-mode UI backend. Layout (stack/row/
// anchor) is data the hot system computes; the kernel only draws primitives.
enum GameUiKind : std::uint32_t {
    GAME_UI_TEXT = 1,
    GAME_UI_PANEL = 2,
    GAME_UI_BAR = 3,
    GAME_UI_IMAGE = 4,
    // Interactive widget: the cold backend hit-tests the rect and emits a
    // GAME_EVENT_UI_ACTION carrying elementId when interacted with. Hot code owns
    // the element's meaning; the backend only knows the id.
    GAME_UI_BUTTON = 5,
    // Numeric range control (minValue..maxValue, step). Interaction emits
    // VALUE_CHANGED with the new value. The backend knows no setting meaning.
    GAME_UI_SLIDER = 6,
    // Boolean control. Interaction emits VALUE_CHANGED (0/1) or CLICK.
    GAME_UI_TOGGLE = 7,
    // Bounded text field. `text` is the current value emitted by hot policy;
    // `maxValue` is the max length; flags bit0 = masked (password). The backend
    // tracks only the focused element id and reports keystrokes as generic
    // ui.action TEXT_INPUT/TEXT_SUBMIT events; it never owns the text value.
    GAME_UI_TEXT_INPUT = 9,
    // Discrete-choice control: displays `text` (the current option label);
    // `value` is the current option index, `maxValue` = optionCount-1. Clicking
    // emits VALUE_CHANGED with the next index. Hot code owns the option ids and
    // semantics; the backend only reports the index.
    GAME_UI_SELECT = 8,
};
struct GameUiCommandV1 {
    std::uint32_t kind;     // GameUiKind
    std::uint32_t flags;    // reserved
    float x, y, w, h;       // screen-space rect (w/h for panels/bars/images)
    float color[4];
    float value;            // bar fill fraction 0..1
    float scale;            // text scale
    std::uint64_t resourceId;  // logical image resource (GAME_UI_IMAGE)
    char text[64];          // text, or image path fallback
    std::uint64_t elementId;   // logical element id (GAME_UI_BUTTON/SLIDER/TOGGLE)
    float minValue;         // GAME_UI_SLIDER range
    float maxValue;
    float step;
};
using GameRenderUiFn = void (MIMITA_GAME_CALL *)(
    void* host, const GameUiCommandV1* command);

// Generic UI interaction payload. The cold backend reports WHICH logical element
// was interacted with and HOW; hot code maps that to behavior. No per-widget
// callback ABI, no cached function pointers (generation-safe by construction).
enum GameUiActionType : std::uint32_t {
    GAME_UI_ACTION_CLICK = 1,
    GAME_UI_ACTION_HOVER = 2,
    GAME_UI_ACTION_VALUE_CHANGED = 3,
    GAME_UI_ACTION_FOCUS = 4,
    // Text keystroke for the focused field: `value` = codepoint (0 = backspace).
    GAME_UI_ACTION_TEXT_INPUT = 5,
    GAME_UI_ACTION_TEXT_SUBMIT = 6,
};
struct GameUiActionV1 {
    std::uint64_t elementId;   // gameHash("menu.play") etc.
    std::uint32_t actionType;  // GameUiActionType
    float value;               // e.g. slider value
    float pointerX;
    float pointerY;
    std::uint32_t handled;     // set by a hot handler that owns the action
    std::uint32_t reserved;
};
using GameUiActionFn = void (MIMITA_GAME_CALL *)(void* host, GameUiActionV1* action);

// Component copy policy lives in schema metadata so the editor never hardcodes
// "if component == X".
// Generic dynamic-component network policy (schema metadata). Replication
// eligibility is data on the schema; the network layer never switches on a
// component name.
static constexpr std::uint32_t GAME_NET_NONE = 0;         // never replicated
static constexpr std::uint32_t GAME_NET_ALL = 1;          // to every client
static constexpr std::uint32_t GAME_NET_OWNER = 2;        // only the owning player
static constexpr std::uint32_t GAME_NET_SERVER_ONLY = 3;  // authoritative only

enum GameCopyPolicy : std::uint32_t {
    GAME_COPY_AUTHORING = 0,
    GAME_COPY_IDENTITY_ONLY = 1,
    GAME_COPY_RUNTIME_ONLY = 2,
    GAME_COPY_DERIVED = 3,
    GAME_COPY_NETWORK_TRANSIENT = 4,
    GAME_COPY_DO_NOT_COPY = 5,
};

using GameSystemInvokeFn = void (MIMITA_GAME_CALL *)(void* host, std::uint64_t tick, float dt);
using GameCommandInvokeFn = void (MIMITA_GAME_CALL *)(void* host, const char* args);
using GameEventDispatchFn = void (MIMITA_GAME_CALL *)(void* host, const GameEventV1* event);

struct GameSystemDescriptorV1 {
    std::uint64_t id;         // gameHash("package.system")
    std::uint64_t domainId;   // gameHash("domain")
    std::uint32_t priority;   // lower runs first
    std::uint32_t reserved;
    GameSystemInvokeFn invoke;
    const char* name;         // debug only
};

struct GameEventTypeDescriptorV1 {
    std::uint64_t id;         // gameHash("package.event")
    std::uint64_t schemaHash;
    std::uint64_t domainId;   // 0 = immediate dispatch
    GameEventDispatchFn dispatch;
    const char* name;
};

struct GameComponentSchemaDescriptorV1 {
    std::uint64_t id;         // gameHash("Component")
    std::uint64_t schemaHash;
    std::uint32_t size;
    std::uint32_t align;
    std::uint32_t copyPolicy;    // GameCopyPolicy
    std::uint32_t networkPolicy;
    const char* name;
    // Append-only (ABI v6): schema version drives migration on activation.
    // 0 is treated as 1 for packages built before this field existed.
    std::uint32_t version;
    std::uint32_t reserved;
};

struct GameCapabilityDescriptorV1 {
    std::uint64_t id;         // gameHash("entity.spawn")
    std::uint64_t signatureId;
    std::uint64_t schemaHash;
    void* callable;           // provider callable; signature is by convention
    const char* name;
};

// A requirement is a capability id plus the signature the requester expects.
// The kernel validates compatibility generically at activation; a mismatch
// rejects the candidate and the last-good generation stays active.
struct GameCapabilityRequirementV1 {
    std::uint64_t id;
    std::uint64_t signatureId;  // 0 = any
    std::uint64_t schemaHash;   // 0 = any
};

struct GameCommandDescriptorV1 {
    const char* name;
    const char* usage;
    std::uint64_t systemId;   // 0 if invoke provided
    GameCommandInvokeFn invoke;
};

struct GameResourceDescriptorV1 {
    std::uint64_t id;
    std::uint64_t contentHash;
    std::uint32_t kind;
    std::uint32_t reserved;
    const char* logicalName;
};

struct GameMigrationDescriptorV1 {
    std::uint64_t typeId;
    std::uint32_t fromVersion;
    std::uint32_t toVersion;
    void* migrate;            // MigrationFn
};

struct GamePackageDescriptorV1 {
    std::uint32_t structSize;
    std::uint32_t abiVersion;
    std::uint64_t packageId;
    std::uint64_t logicalHash;
    const char* name;

    const GameSystemDescriptorV1* systems;
    std::uint32_t systemCount;
    const GameEventTypeDescriptorV1* eventTypes;
    std::uint32_t eventTypeCount;
    const GameComponentSchemaDescriptorV1* componentSchemas;
    std::uint32_t componentSchemaCount;
    const GameCapabilityDescriptorV1* capabilityProviders;
    std::uint32_t capabilityProviderCount;
    const GameCapabilityRequirementV1* capabilityRequirements;
    std::uint32_t capabilityRequirementCount;
    const GameCommandDescriptorV1* commands;
    std::uint32_t commandCount;
    const GameResourceDescriptorV1* resources;
    std::uint32_t resourceCount;
    const GameMigrationDescriptorV1* migrations;
    std::uint32_t migrationCount;
    // v7 additions (append-only): runtime-registered gamemodes (metadata only).
    const GameModeDescriptorV1* modes;
    std::uint32_t modeCount;
};

using MimitaGetPackageDescriptorFn = const GamePackageDescriptorV1* (MIMITA_GAME_CALL *)();

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
    // v5: generic package descriptor (may be null for legacy modules).
    const GamePackageDescriptorV1* packageDescriptor;
};

using GetGameAPIFn = bool (MIMITA_GAME_CALL *)(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);

MIMITA_GAME_EXPORT bool MIMITA_GAME_CALL GetGameAPI(
    std::uint32_t requestedVersion,
    GameAPI* outAPI);
