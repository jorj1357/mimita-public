// 09 21 2026
/* purpose
* Frozen v2.0.6 movement reference oracle.
*
* This is a self-contained, deterministic reconstruction of the movement step
* that shipped in git tag `v2.0.6` (src/physics/physics-mini.cpp plus the
* per-ability physics-*.cpp files and the effective config/movement.json
* values). It exists ONLY to generate and compare reference traces for
* movement parity work; it is never a gameplay owner.
*
* It does NOT include Player&, World&, rendering, audio, effects, packets,
* input polling, or authority. It is plain data in, plain data out.
*
* Collision scope: capsule vs a triangle list using the v2.0.6 contact
* response rules (walkable slope dot, velocity projection, touch resets).
* The full v2.0.6 swept GLB pipeline is a separate recovery step; scenarios
* that need it must be added deliberately.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MimitaV206 {

struct Config {
    // Effective v2.0.6 config/movement.json values.
    float gravity = -58.0f;
    float moveSpeed = 20.0f;
    float jumpStrength = 19.0f;

    float playerWidth = 1.0f;
    float playerHeight = 3.6f;
    float playerDepth = 0.4f;
    float playerRadius = 0.7f;

    float collisionSkin = 0.02f;
    float maxWalkableSlopeDot = 0.80f;
    float maxFallSpeed = 400.0f;
    float maxPlayerMoveSpeed = 2000.0f;
    float maxExternalImpulseSpeed = 120.0f;
    float externalImpulseDecay = 0.6f;
    float externalImpulseSteerRate = 40.0f;
    float externalImpulseBrakeRate = 20.0f;
    float almostZero = 0.00001f;

    float coyoteJumpTime = 0.001f;
    float groundFrictionAmount = 4.0f;
    float groundAccelerate = 8.0f;
    float airAccelAmount = 222.0f;
    float airSpeedCap = 1.0f;

    float dashImpulse = 100.0f;
    float airDashImpulse = 50.0f;
    float downDashSpeed = -100.0f;
    float freezeMaxTime = 5.0f;

    float jumpBufferTime = 0.12f;
    int airJumpsMax = 1;

    float stableGroundWindow = 0.08f;
};

struct PlayerState {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec3 externalImpulse{0.0f};
    float yaw = 0.0f;
    float sizeScale = 1.0f;

    bool onGround = false;
    bool stableOnGround = true;
    bool wasOnGround = false;
    float groundLostTimer = 0.0f;
    float airborneTimer = 0.0f;
    bool hasWorldContact = false;

    int airJumpsLeft = 1;
    bool jumpHeldPrev = false;
    bool airJumpLocked = false;
    bool airJumpArmed = false;
    float jumpIntentTimer = 0.0f;
    float coyoteTimer = 0.0f;

    bool dashHeldPrev = false;
    bool dashAvailable = true;
    int dashMovementTicks = 0;
    int lastDashQuality = 0;

    bool groundReturnAvailable = true;
    bool downDashAvailable = true;

    bool freezeAvailable = true;
    bool freezeHeldPrev = false;
    bool freezeActive = false;
    float freezeTimer = 0.0f;

    bool didGroundJump = false;
    bool didAirJump = false;
    bool didDash = false;
    bool didLand = false;

    // v2.0.6 collision bookkeeping (emergency-stuck counter).
    int collisionStuckFrames = 0;
};

struct Input {
    glm::vec2 wishMoveXY{0.0f};
    bool jumpHeld = false;
    bool jumpPressed = false;
    bool dashPressed = false;
    bool movementPressed = false;
    bool groundReturnPressed = false;
    bool downDashPressed = false;
    bool freezeHeld = false;
    glm::vec3 camForward{1.0f, 0.0f, 0.0f};
    float dt = 1.0f / 60.0f;
};

struct Triangle {
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    glm::vec3 c{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
};

struct World {
    std::vector<Triangle> triangles;
};

struct Trace {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec3 externalImpulse{0.0f};
    bool onGround = false;
    bool stableOnGround = false;
    bool hasWorldContact = false;
    bool didGroundJump = false;
    bool didAirJump = false;
    bool didDash = false;
    bool didLand = false;
};

// Advances the frozen v2.0.6 movement step exactly once (one fixed tick).
void step(PlayerState& p, const Input& in, const World& world, const Config& cfg);

Trace traceOf(const PlayerState& p);

// Deterministic self-test for the oracle itself (not a gameplay test).
bool runMovementV206ReferenceSelfTest(std::string& report);

} // namespace MimitaV206
