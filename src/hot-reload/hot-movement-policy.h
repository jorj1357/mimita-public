// 09 15 2026
/* purpose
* Generic movement-policy payloads shared between the cold movement mechanism
* (which fills inputs and applies the returned velocity) and hot movement
* algorithms (which own how velocity changes). A hot handler that sets `handled`
* owns the actual acceleration/friction/jump/dash math for that step. No
* ServerPlayer/Npc/entity-type fields; inputs are plain numbers so the same hot
* function drives server simulation and local prediction.
* Does NOT own collision/physics or the network transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

// Air-acceleration algorithm step. Cold fills the inputs from the shared
// movement state; a hot handler sets `handled` and writes outVelocity[2].
struct GameAirAccelerateV1 {
    float velocity[2];          // in: current horizontal velocity (x,y)
    float wishDir[2];           // in: normalized wish direction
    float wishSpeed;            // in: uncapped wish speed (maxSpeed)
    float wishspd;              // in: Source wish-speed projection cap
    float maxSpeed;             // in: movement max speed
    float airAcceleration;      // in
    float surfaceFriction;      // in
    float airSpeedGainMultiplier;  // in
    float dt;                   // in
    float currentSpeed;         // in: dot(velocity, wishDir)
    float blendedAddSpeed;      // in: addSpeed after blending
    std::uint32_t tick;
    std::uint32_t movementModel;  // in: 0 = source projection, 1 = v2.0.6 additive
    // out
    float outVelocity[2];
    std::uint32_t handled;
    std::uint32_t reserved;
};

static constexpr std::uint64_t GAME_EVENT_MOVEMENT_AIR_ACCELERATE =
    gameHash("movement.air-accelerate");

// The single air-acceleration implementation. The `movement.air-accelerate`
// event handler calls it, and local prediction (`movement.main`) calls it
// directly. Context-free: generic/POD numbers only (no ServerPlayer/Player/
// renderer/packet state). Defined once in modules/movement-air.cpp.
namespace MimitaHotMovement {
void airAccelerate(const GameAirAccelerateV1& in, float outVelocity[2]);
}

// Ground-move algorithm step (friction + acceleration along wishdir). Cold
// fills the inputs; a hot handler sets `handled` and writes outVelocity[2].
struct GameGroundMoveV1 {
    float velocity[2];       // in: current horizontal velocity
    float wishDir[2];        // in: normalized wish direction (0 if no input)
    float wishSpeed;         // in: ground max speed
    float groundAcceleration;// in
    float frictionAmount;    // in: combined friction scalar (source*surface*...)
    float stopspeed;         // in: friction floor
    float dt;                // in
    std::uint32_t hasInput;  // in
    std::uint32_t movementModel; // in: 0 = source, 1 = v2.0.6 XOR friction/accelerate
    float outVelocity[2];
    std::uint32_t handled;
    std::uint32_t reserved;
};

static constexpr std::uint64_t GAME_EVENT_MOVEMENT_GROUND_MOVE =
    gameHash("movement.ground-move");

namespace MimitaHotMovement {
// The single ground-move implementation (friction + acceleration).
void groundMove(const GameGroundMoveV1& in, float outVelocity[2]);
}

// Gravity algorithm step: how the vertical velocity changes over a tick and the
// terminal-speed clamp. Cold fills inputs; a hot handler sets `handled`.
struct GameGravityV1 {
    float velocityZ;         // in
    float gravityZ;          // in: signed (negative = downward)
    float maximumFallSpeed;  // in: terminal speed (magnitude)
    float dt;                // in
    float outVelocityZ;      // out
    std::uint32_t handled;
    std::uint32_t reserved;
};
static constexpr std::uint64_t GAME_EVENT_MOVEMENT_GRAVITY =
    gameHash("movement.gravity");

namespace MimitaHotMovement {
// The single gravity implementation.
void gravity(const GameGravityV1& in, float& outVelocityZ);
}

// Speed / wish-speed derivation policy: size-scale factor, effective max speed
// (with a fixed speed limit), and the air wish-speed projection cap. Cold fills
// inputs; a hot handler sets `handled` and writes outMaxSpeed/outWishspd.
struct GameSpeedPolicyV1 {
    float baseMaxSpeed;       // in: preset max speed (0 => use fallback)
    float baseFallbackSpeed;  // in: fallback when baseMaxSpeed <= 0 (e.g. groundSpeed)
    float sizeScale;          // in
    float sizeExponent;       // in
    float speedLimit;         // in: 0 = none
    float airMaxWishspeed;    // in: 0 = none (use maxSpeed)
    float rawWishSpeed;       // in: wish * maxSpeed (for the projection cap)
    std::uint32_t speedLimitFixed;  // in: apply speedLimit as a hard cap
    std::uint32_t reserved;
    float outMaxSpeed;        // out
    float outWishspd;         // out
    std::uint32_t handled;
    std::uint32_t reserved2;
};
static constexpr std::uint64_t GAME_EVENT_MOVEMENT_SPEED_POLICY =
    gameHash("movement.speed-policy");

namespace MimitaHotMovement {
// The single speed/wish-speed derivation implementation.
void speedPolicy(const GameSpeedPolicyV1& in, float& outMaxSpeed, float& outWishspd);
}

// Jump policy (eligibility, buffer, coyote, air jumps, impulse, state). Carries
// the persistent runtime jump state in/out. Cold fills inputs + state; a hot
// handler owns the rules and sets `handled`.
struct GameJumpPolicyV1 {
    // in
    float velocityZ;
    float jumpSpeed;              // scaled jump velocity
    float dt;
    float coyoteSeconds;
    float jumpBufferSeconds;
    std::uint32_t grounded;
    std::uint32_t jumpPressed;
    std::uint32_t jumpHeld;
    std::uint32_t jumpHeldPreviously;
    std::uint32_t autoBhopEnabled;
    std::uint32_t maximumAirJumps;
    // in/out runtime jump state
    float jumpIntentTimerSeconds;
    float coyoteTimerSeconds;
    std::int32_t airJumpsLeft;
    std::uint32_t airJumpArmed;
    std::uint32_t airJumpLocked;
    // out
    float outVelocityZ;
    std::uint32_t outGrounded;
    std::uint32_t outJumpHeldPreviously;
    std::uint32_t outDidGroundJump;
    std::uint32_t outDidAirJump;
    std::uint32_t handled;
    std::uint32_t reserved;
};
static constexpr std::uint64_t GAME_EVENT_MOVEMENT_JUMP =
    gameHash("movement.jump");

namespace MimitaHotMovement {
// The single jump implementation (mutates the runtime jump state in place).
void jumpPolicy(GameJumpPolicyV1& io);
}

// Dash / down-dash policy (activation, direction choice, impulse composition,
// availability/state). Cold fills inputs; a hot handler sets `handled`.
struct GameDashPolicyV1 {
    float velocity[3];         // in: x,y,z
    float moveAxes[2];         // in
    float cameraForward[2];    // in: fallback direction
    float groundDashImpulse;   // in
    float airDashImpulse;      // in
    float downDashVerticalSpeed;  // in
    std::uint32_t dashPressed;
    std::uint32_t downDashPressed;
    std::uint32_t grounded;
    std::uint32_t dashAvailable;
    std::uint32_t downDashAvailable;
    std::uint32_t dashEnabled;
    std::uint32_t downDashEnabled;
    std::uint32_t dashMovementTicks;  // in: airborne ticks with move held (v2.0.6 dash quality)
    // out
    float outVelocity[3];
    std::uint32_t outDashAvailable;
    std::uint32_t outDownDashAvailable;
    std::uint32_t outDidDash;
    std::uint32_t outDidDownDash;
    std::uint32_t outUsedMoveInput;
    std::uint32_t handled;
};
static constexpr std::uint64_t GAME_EVENT_MOVEMENT_DASH =
    gameHash("movement.dash");

namespace MimitaHotMovement {
// The single dash/down-dash implementation.
void dashPolicy(GameDashPolicyV1& io);
}

// Freeze policy (activation, duration, velocity suppression, exit). Carries the
// persistent freeze state in/out. Cold fills inputs; a hot handler sets `handled`.
struct GameFreezePolicyV1 {
    float velocity[3];
    float dt;
    float durationSeconds;    // 0 = unlimited
    std::uint32_t freezePressed;
    std::uint32_t freezeHeld;
    std::uint32_t freezeHeldPreviously;
    std::uint32_t freezeEnabled;
    std::uint32_t freezeActive;
    std::uint32_t freezeAvailable;
    float freezeTimerSeconds;
    // out
    float outVelocity[3];
    std::uint32_t outFreezeActive;
    std::uint32_t outFreezeAvailable;
    std::uint32_t outFreezeHeldPreviously;
    float outFreezeTimerSeconds;
    std::uint32_t outDidFreeze;
    std::uint32_t outFreezeStarted;
    std::uint32_t outFreezeEnded;
    std::uint32_t handled;
};
static constexpr std::uint64_t GAME_EVENT_MOVEMENT_FREEZE =
    gameHash("movement.freeze");

namespace MimitaHotMovement {
// The single freeze implementation.
void freezePolicy(GameFreezePolicyV1& io);
}

// Post-acceleration speed clamp / preservation policy (horizontal). Cold fills
// inputs; a hot handler sets `handled`.
struct GameSpeedClampV1 {
    float velocity[2];   // in: horizontal
    float speedLimit;    // in: 0 = none
    std::uint32_t enabled;
    // out
    float outVelocity[2];
    std::uint32_t handled;
    std::uint32_t reserved;
};
static constexpr std::uint64_t GAME_EVENT_MOVEMENT_SPEED_CLAMP =
    gameHash("movement.speed-clamp");

namespace MimitaHotMovement {
// The single post-step speed clamp/preservation implementation.
void speedClamp(GameSpeedClampV1& io);
}

// ── Single movement tuning authority ───────────────────────────────────────
// The hot module owns the editable Source tuning. Cold callers (server, NPC,
// prediction setup, validation) request the active tuning through this event;
// the hot handler fills it from the in-module Source preset. There is exactly
// one tuning authority: the C++ table in modules/movement-system.cpp.
// JSON movement presets are reference/archive data and are never consulted.
struct GameMovementTuningV1 {
    // in: requested preset id (0 = source). Unknown clamps to the active preset.
    std::uint32_t presetId;
    // C++ preset values (filled by the hot handler).
    float walkSpeed;               // ground = air max speed
    float groundSpeed;
    float airSpeed;
    float groundAcceleration;
    float groundDeceleration;
    float groundDirectionChangeResponse;
    float airAcceleration;
    float groundFriction;          // Source friction scalar
    float groundFrictionAmount;    // MiMITA ground friction amount
    float airFrictionAmount;
    float stopspeed;               // friction floor
    float airMaxWishspeed;         // 0 = no air projection cap
    float airSpeedGainMultiplier;  // Source air residual gain scalar
    float airControl;
    float surfaceFriction;         // global surface friction scalar
    float gravityMagnitude;        // positive; applied as -magnitude
    float jumpSpeed;
    float maxFallSpeed;
    float jumpBufferSeconds;
    float coyoteSeconds;
    float dashImpulse;
    float groundDashImpulse;
    float airDashImpulse;
    float dashCooldownSeconds;
    float downDashSpeed;
    float dashGraceSeconds;
    float dashFrictionMultiplier;
    float freezeDurationSeconds;
    float freezeCurveExponent;
    float externalImpulseDecay;
    float maximumExternalImpulseSpeed;
    float impulseCarrySeconds;
    float landingOverspeedBleed;
    float landingSpeedRetention;
    float velocityClipEpsilon;
    float speedLimit;
    float bunnyHopSpeedCap;
    float minimumStrafeAngleDegrees;
    float maximumAccelerationPerTick;
    float accelerationFalloffNearCap;
    float airSteeringResponse;
    float maximumSteeringDegreesPerSecond;
    float minimumCameraYawDeltaDegrees;
    float minimumWishRotationDegrees;
    float strafeAngularToleranceDegrees;
    float softCapStart;
    float airInputBlending;
    float airInputMouseThresholdDegrees;
    float sourceMaxSpeed;
    float sourceFriction;
    float freeFlySpeed;
    std::uint32_t maximumAirJumps;
    std::uint32_t autoBhopEnabled;
    std::uint32_t dashEnabled;
    std::uint32_t downDashEnabled;
    std::uint32_t freezeEnabled;
    std::uint32_t sourceWalkMode;
    std::uint32_t walkMode;        // 0=mimita, 1=accel, 2=source
    std::uint32_t airControlEnabled;
    std::uint32_t bunnyHopEnabled;
    std::uint32_t preserveStraightSpeed;
    std::uint32_t diagonalInputNormalization;
    std::uint32_t speedCapEnabled;
    std::uint32_t maximumBhopSpeedMode;
    std::uint32_t speedLimitEnabled;
    std::uint32_t speedLimitMode;
    std::uint32_t debugDrawEnabled;
    std::uint32_t requireActiveWishRotation;
    std::uint32_t stationaryCameraInputMode;
    std::uint32_t sourceAirAccelerateBugCompatible;
    std::uint32_t groundSnap;
    std::uint32_t airInputBlendingEnabled;
    std::uint32_t impulseFrictionMode;
    // out
    std::uint32_t handled;
    std::uint32_t reserved;
};
static constexpr std::uint64_t GAME_EVENT_MOVEMENT_TUNING =
    gameHash("movement.tuning");

// ── Spawn protection (shared hot state) ─────────────────────────────────────
// Written on an actor entity at spawn/respawn and read by the hot damage policy.
// The window is expressed in ticks at the fixed 60 Hz simulation rate so it is
// independent of wall-clock and frame rate. Applies to every actor (players and
// NPCs) because the damage policy sees the victim entity, not an actor type.
static constexpr std::uint64_t HOT_SPAWN_PROTECTION_COMPONENT =
    gameHash("SpawnProtection");
struct HotSpawnProtectionV1 {
    std::uint32_t untilTick;   // damage ignored while currentTick < untilTick
    std::uint32_t reserved;
};

// ── Collision policy (hot tunables) ─────────────────────────────────────────
// Dispatched by the cold capsule solver so collision constants (skin, grounded
// epsilon, capsule size) are editable live. The solve algorithm itself can be
// replaced entirely via the `physics.capsuleSolve` capability.
static constexpr std::uint64_t GAME_EVENT_COLLISION_POLICY =
    gameHash("movement.collision-policy");
struct CollisionPolicyV1 {
    // in: cold-computed defaults
    float radius;
    float halfHeight;
    float groundedVelocityEpsilon;
    float skin;
    // out
    float outRadius;
    float outHalfHeight;
    float outGroundedVelocityEpsilon;
    float outSkin;
    std::uint32_t bounceEnabled;
    float bounceStrength;
    float bounceFriction;
    float bounceMinSpeed;
    float bounceMaxSpeed;
    float bounceCooldown;
    std::uint32_t handled;
    std::uint32_t reserved;
};
