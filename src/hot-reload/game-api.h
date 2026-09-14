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
static constexpr std::uint32_t MIMITA_GAME_API_VERSION = 4;
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
    std::uint32_t reserved;
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

// One kernel query result (world triangle or entity bound) along the ray.
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
