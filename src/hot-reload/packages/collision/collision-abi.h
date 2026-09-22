// 09 17 2026
/* purpose
* Generic collision package ABI. The kernel supplies world geometry and platform
* services only; any hot caller (movement, projectiles, props, ragdolls) sends a
* plain-data solve request and receives a plain-data result. No Player&, NPC&,
* World&, STL container, or owning pointer crosses this boundary.
*
* The package registers one capability, `collision.main`, and owns the world
* triangle cache, spatial broadphase, narrowphase, swept movement, depenetration,
* slide, bounce, grounding, contact merging, solver pass limits, and impact
* selection. Callers own movement policy and supply the collider list.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cstdint>

#include "hot-reload/game-api.h"

namespace HotCollisionPackage {

// ── Versioned registration ──────────────────────────────────────────────────
static constexpr std::uint32_t COLLISION_ABI_VERSION = 1;
static constexpr std::uint64_t GAME_CAP_COLLISION = gameHash("collision.main");
static constexpr std::uint64_t GAME_SIG_COLLISION =
    gameHash("sig.collision.main.v1");

// ── Shapes / masks / policies ───────────────────────────────────────────────
enum CollisionShape : std::uint32_t {
    COLLISION_SHAPE_SPHERE = 0,
    COLLISION_SHAPE_CAPSULE = 1,
    COLLISION_SHAPE_BOX = 2,
};

enum CollisionMask : std::uint32_t {
    COLLISION_MASK_WORLD = 1u << 0,
    COLLISION_MASK_ENTITY = 1u << 1,
};

enum CollisionPolicy : std::uint32_t {
    COLLISION_POLICY_BODY = 0,
    COLLISION_POLICY_CAPSULE = 1,
    COLLISION_POLICY_WEAPON = 2,
    COLLISION_POLICY_ROCKET = 3,
    COLLISION_POLICY_GRENADE = 4,
    COLLISION_POLICY_NPC = 5,
};

enum CollisionPart : std::uint32_t {
    COLLISION_PART_CAPSULE = 0,
    COLLISION_PART_HEAD = 1,
    COLLISION_PART_TORSO = 2,
    COLLISION_PART_LEFT_ARM = 3,
    COLLISION_PART_RIGHT_ARM = 4,
    COLLISION_PART_LEFT_LEG = 5,
    COLLISION_PART_RIGHT_LEG = 6,
    COLLISION_PART_WEAPON = 7,
};

// ── Centralised response policy (single owner; edit live) ───────────────────
// The values are loaded by collision.main from config/collision.json. These
// are the C++ fallback values when behaviorSource is "cpp" or JSON is invalid.
struct CollisionBehaviorV1 {
    std::uint32_t behaviorSourceJson;
    std::uint32_t groundBounce;
    std::uint32_t bounceEnabled;
    float bounceStrength;
    float bounceFriction;
    float bounceMinSpeed;
    float bounceMaxSpeed;
    float bounceCooldown;
};

CollisionBehaviorV1 collisionBehavior();

// ── Fixed bounds ────────────────────────────────────────────────────────────
// The v2.0.6 player-body path submitted three sphere samples per animated
// body part, in addition to the root capsule and weapon shape. Keep enough
// room for that complete hot request instead of silently dropping limbs.
static constexpr std::uint32_t COLLISION_MAX_COLLIDERS = 32;
static constexpr std::uint32_t COLLISION_MAX_CONTACTS = 32;
static constexpr std::uint32_t COLLISION_MAX_IMPACTS = 16;
static constexpr std::uint32_t COLLISION_MAX_LABEL = 32;

// ── Plain-data request / result ─────────────────────────────────────────────
struct CollisionColliderV1 {
    std::uint32_t partId;     // CollisionPart
    std::uint32_t shape;      // CollisionShape
    std::uint32_t policyId;   // CollisionPolicy
    std::uint32_t flags;
    float position[3];        // world-space center
    float radius;
    float halfHeight;         // capsule: tip-to-tip half extent
    float extents[3];         // box half extents (future)
    float velocity[3];
    char label[COLLISION_MAX_LABEL];
    // Append-only: world-space second capsule endpoint. When non-zero the
    // collider is an oriented capsule from `position` to `endPosition` with
    // `radius`; when zero it is the legacy Z-aligned capsule using `halfHeight`.
    float endPosition[3];
};

// Helper colliders may classify support/steps but must not become invisible
// walls once authoritative body colliders are present.
static constexpr std::uint32_t COLLISION_COLLIDER_HELPER = 1u << 0;
static constexpr std::uint32_t COLLISION_COLLIDER_BODY_AUTHORITATIVE = 1u << 1;
// When set, the collider is an oriented capsule from `position` to
// `endPosition` with `radius`; when unset, `endPosition` is ignored and the
// legacy Z-aligned capsule using `halfHeight` is used.
static constexpr std::uint32_t COLLISION_COLLIDER_ORIENTED_CAPSULE = 1u << 2;

struct CollisionContactV1 {
    std::uint64_t sourceEntity;
    std::uint32_t sourcePart;
    std::uint32_t targetKind;   // 0 = world, 1 = entity
    std::uint64_t targetEntity;
    std::uint32_t targetPart;
    std::int32_t triangle;
    float point[3];
    float normal[3];
    float penetration;
    float incomingSpeed;
};

struct CollisionImpactV1 {
    std::uint64_t entityId;
    std::uint32_t partId;
    float position[3];
    float normal[3];
    float size;
    std::uint32_t lifetimeTicks;
};

enum CollisionSolveFlags : std::uint32_t {
    COLLISION_SOLVE_SPAWN_IMPACTS = 1u << 0,
};

struct CollisionSolveV1 {
    // in
    std::uint64_t entityId;
    std::uint64_t tick;
    float dt;
    float yaw;            // degrees
    float sizeScale;
    std::uint32_t mask;   // CollisionMask bits
    std::uint32_t flags;  // CollisionSolveFlags
    std::uint32_t colliderCount;
    CollisionColliderV1 colliders[COLLISION_MAX_COLLIDERS];
    float position[3];
    float velocity[3];
    // Append-only identity/timing so collision records can name the actor and
    // the frame/client/server tick being solved. 0 = unknown.
    std::uint64_t frame;
    std::uint64_t serverTick;
    std::uint64_t clientTick;
    std::uint32_t actorKind;   // CollisionLogActorKind (1 player, 2 npc, ...)
    std::uint32_t reserved0;
    // out
    float outPosition[3];
    float outVelocity[3];
    std::uint32_t grounded;
    std::uint32_t worldContact;
    std::uint32_t bodyContact;
    std::uint32_t bounced;
    std::uint32_t groundSettled;
    std::uint32_t handled;      // 1 = package solved; 0 = decline
    std::uint32_t contactCount;
    CollisionContactV1 contacts[COLLISION_MAX_CONTACTS];
    std::uint32_t impactCount;
    CollisionImpactV1 impacts[COLLISION_MAX_IMPACTS];
};

// `host` is the GameplayContextV1* (the gameplay context), matching the original
// hot capsule-solve contract: the package resolves capabilities
// (`world.collision`, `log.event`, `effect.part`) and reads tick/frame from it.
// Hot callers pass their `ctx`, never the opaque `ctx->host`.
using GameCollisionSolveFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       CollisionSolveV1* solve);

// Direct entry point to the package solve (same function the capability calls).
// Exposed so in-DLL callers and the package self-test need no capability host.
void collisionSolve(void* host, CollisionSolveV1* solve);

// Package-provided candidate self-test. Defined in `collision-selftest.cpp` and
// called by the package self-test hook before activation.
bool collisionPackageSelfTest(char* message, std::uint32_t cap);

// Clears all per-entity runtime memory (bounce cooldowns, ground hysteresis) so
// a candidate self-test starts from a known state. Not called during gameplay.
void collisionResetRuntimeState();

} // namespace HotCollisionPackage

#endif // MIMITA_GAME_DLL
