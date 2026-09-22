// 09 22 2026
/* purpose
* Frozen `afad20a` movement reference oracle.
*
* This is a self-contained, deterministic reconstruction of the movement step
* that shipped at git commit `afad20a` ("npc stuff its cool", 2026-09-11),
* which the project treats as the pre-hot-reload behavioral reference. It is a
* pure formula/ordering oracle; it is never a gameplay owner.
*
* It does NOT include Player&, World&, rendering, audio, effects, packets,
* input polling, or authority. It is plain data in, plain data out.
*
* Collision is intentionally out of scope for this oracle: callers supply the
* grounded/contact facts that the old collision pipeline produced. Movement
* math and ordering are the subject here; per-limb collision parity is a
* separate slice.
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace MimitaAfad20a {

struct Config {
    // Effective afad20a "source" preset values.
    float gravity = -40.0f;
    float maximumFallSpeed = 175.0f;
    float sourceMaxSpeed = 20.0f;
    float groundAcceleration = 20.0f;
    float sourceFriction = 3.25f;
    float surfaceFriction = 1.0f;
    float stopspeed = 1.0f;
    float airAcceleration = 12.0f;
    float airMaxWishspeed = 2.0f;
    float airSpeedGainMultiplier = 2.0f;
    bool sourceAirAccelerateBugCompatible = true;
    bool airControlEnabled = true;
    bool groundSnap = true;
    float velocityClipEpsilon = 1.01f;

    float jumpVerticalSpeed = 15.1f;
    float jumpBufferSeconds = 0.2f;
    float coyoteSeconds = 0.0f;
    int maximumAirJumps = 1;
    bool autoBhopEnabled = true;

    float dashImpulse = 10.0f;
    float downDashVerticalSpeed = -50.0f;

    float freezeDurationSeconds = 5.0f;
    float freezeCurveExponent = 4.0f;

    bool speedLimitEnabled = true;
    float speedLimit = 50.0f;

    float maximumDeltaSeconds = 0.033f;
    float almostZeroSpeed = 0.00001f;

    float dt = 1.0f / 60.0f;
};

struct State {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 externalImpulse{0.0f};

    // Ground facts are supplied by the caller (the old collision pipeline).
    bool grounded = false;
    bool stableOnGround = false;
    float groundLostTimer = 0.0f;

    int airJumpsLeft = 1;
    bool jumpHeldPrev = false;
    bool airJumpLocked = false;
    bool airJumpArmed = false;
    float jumpIntentTimer = 0.0f;
    float coyoteTimer = 0.0f;

    bool dashAvailable = true;
    bool dashHeldPrev = false;

    bool downDashAvailable = true;

    bool freezeAvailable = true;
    bool freezeHeldPrev = false;
    bool freezeActive = false;
    float freezeTimer = 0.0f;
};

struct Input {
    glm::vec2 moveAxes{0.0f};
    bool jumpHeld = false;
    bool jumpPressed = false;
    bool dashPressed = false;
    bool downDashPressed = false;
    bool freezeHeld = false;
    bool freezePressed = false;
    glm::vec2 cameraForward{1.0f, 0.0f};
};

// Advances the frozen afad20a movement step exactly once (one fixed tick).
void step(State& state, const Input& input, const Config& config);

// Deterministic self-test for the oracle itself (not a gameplay test).
bool runMovementAfad20aReferenceSelfTest(std::string& report);

} // namespace MimitaAfad20a
