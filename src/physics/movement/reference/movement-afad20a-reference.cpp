// 09 22 2026
/* purpose
* Implements the frozen afad20a movement oracle and its deterministic self-test.
* Does NOT run the game or own movement policy.
*/
#include "physics/movement/reference/movement-afad20a-reference.h"

#include <algorithm>
#include <cmath>

namespace MimitaAfad20a {
namespace {

float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void updateFreeze(State& s, const Input& in, const Config& c, float dt)
{
    const bool pressed = in.freezePressed || (in.freezeHeld && !s.freezeHeldPrev);
    const bool released = !in.freezeHeld && s.freezeHeldPrev;
    bool started = false;

    if (pressed && s.freezeAvailable) {
        s.velocity = glm::vec3(0.0f);
        s.externalImpulse = glm::vec3(0.0f);
        s.freezeActive = true;
        s.freezeAvailable = false;
        s.freezeTimer = 0.0f;
        started = true;
    }
    if (released && s.freezeActive)
        s.freezeActive = false;

    s.freezeHeldPrev = in.freezeHeld;

    if (s.freezeActive && in.freezeHeld && !started) {
        s.freezeTimer += dt;
        if (c.freezeDurationSeconds > 0.0f && s.freezeTimer > c.freezeDurationSeconds)
            s.freezeTimer = c.freezeDurationSeconds;
        float pt = 1.0f;
        if (c.freezeDurationSeconds > 0.0f) {
            const float u = clampf(s.freezeTimer / c.freezeDurationSeconds, 0.0f, 1.0f);
            pt = std::pow(u, c.freezeCurveExponent);
        }
        s.velocity *= pt;
    }
}

void applyGround(State& s, const Input& in, const Config& c, float dt)
{
    float maxSpeed = c.sourceMaxSpeed;
    if (c.speedLimitEnabled && c.speedLimit > 0.0f)
        maxSpeed = std::min(maxSpeed, c.speedLimit);

    const float frictionAmount = c.sourceFriction * c.surfaceFriction;
    glm::vec2 v(s.velocity.x, s.velocity.y);
    const float speed = glm::length(v);
    if (speed > 0.1f) {
        const float control = std::max(speed, c.stopspeed);
        const float drop = control * frictionAmount * dt;
        const float newSpeed = std::max(0.0f, speed - drop);
        if (newSpeed != speed)
            v *= newSpeed / speed;
    } else {
        v = glm::vec2(0.0f);
    }

    const float wishLen = glm::length(in.moveAxes);
    if (wishLen > 1e-4f) {
        const glm::vec2 wishDir = in.moveAxes / wishLen;
        const float current = glm::dot(v, wishDir);
        const float add = maxSpeed - current;
        if (add > 0.0f) {
            float accel = c.groundAcceleration * maxSpeed * dt;
            if (accel > add)
                accel = add;
            v += wishDir * accel;
        }
    }

    s.velocity.x = v.x;
    s.velocity.y = v.y;
    if (c.groundSnap && std::fabs(s.velocity.z) <= c.velocityClipEpsilon)
        s.velocity.z = 0.0f;
}

void applyAir(State& s, const Input& in, const Config& c, float dt)
{
    const float wishLen = glm::length(in.moveAxes);
    if (wishLen <= 1e-4f)
        return;

    float maxSpeed = c.sourceMaxSpeed;
    if (c.speedLimitEnabled && c.speedLimit > 0.0f)
        maxSpeed = std::min(maxSpeed, c.speedLimit);

    const glm::vec2 wishVelocity = in.moveAxes * maxSpeed;
    const float wishSpeed = glm::length(wishVelocity);
    if (wishSpeed <= 1e-4f)
        return;
    const glm::vec2 wishDir = wishVelocity / wishSpeed;

    float wishspd = c.airMaxWishspeed > 0.0f ? c.airMaxWishspeed : maxSpeed;
    wishspd = std::min(wishspd, wishSpeed);

    glm::vec2 v(s.velocity.x, s.velocity.y);
    if (glm::length(v) <= 0.1f)
        return;

    const float current = glm::dot(v, wishDir);
    const float blendedAdd = wishspd - current;
    if (blendedAdd > 0.0f) {
        const float accelerationWishSpeed =
            c.sourceAirAccelerateBugCompatible ? wishSpeed : wishspd;
        float accel = c.airAcceleration * accelerationWishSpeed * dt *
                      c.surfaceFriction * c.airSpeedGainMultiplier;
        if (accel > blendedAdd)
            accel = blendedAdd;
        v += wishDir * accel;
    }
    s.velocity.x = v.x;
    s.velocity.y = v.y;
}

void applySourceMovement(State& s, const Input& in, const Config& c, float dt)
{
    const bool jumpingNow =
        s.grounded && (in.jumpPressed || (c.autoBhopEnabled && in.jumpHeld));
    if (s.grounded && !jumpingNow)
        applyGround(s, in, c, dt);
    else if (c.airControlEnabled)
        applyAir(s, in, c, dt);
}

void tryActivateDash(State& s, const Input& in, const Config& c)
{
    if (!in.dashPressed || !s.dashAvailable)
        return;
    glm::vec2 dir(0.0f);
    const float moveLen = glm::length(in.moveAxes);
    if (moveLen > 1e-4f) {
        dir = in.moveAxes / moveLen;
    } else {
        const float camLen = glm::length(in.cameraForward);
        if (camLen > 1e-4f)
            dir = in.cameraForward / camLen;
    }
    if (glm::length(dir) <= 1e-4f)
        return;
    s.velocity.x += dir.x * c.dashImpulse;
    s.velocity.y += dir.y * c.dashImpulse;
    s.dashAvailable = false;
}

void applyJump(State& s, const Input& in, const Config& c, float dt)
{
    s.jumpIntentTimer = std::max(0.0f, s.jumpIntentTimer - dt);
    s.coyoteTimer = std::max(0.0f, s.coyoteTimer - dt);
    if (s.grounded)
        s.coyoteTimer = c.coyoteSeconds;

    if (in.jumpHeld && c.autoBhopEnabled)
        s.jumpIntentTimer = c.jumpBufferSeconds;

    const bool jumpPressedThis = in.jumpPressed || (in.jumpHeld && !s.jumpHeldPrev);
    if (jumpPressedThis)
        s.jumpIntentTimer = c.jumpBufferSeconds;

    const bool jumpReleased = !in.jumpHeld && s.jumpHeldPrev;
    if (jumpReleased) {
        s.airJumpArmed = true;
        s.airJumpLocked = false;
        s.jumpIntentTimer = 0.0f;
    }
    s.jumpHeldPrev = in.jumpHeld;

    if (s.jumpIntentTimer <= 0.0f)
        return;

    if (s.grounded || s.coyoteTimer > 0.0f) {
        s.dashAvailable = true;
        s.velocity.z = c.jumpVerticalSpeed;
        s.grounded = false;
        s.coyoteTimer = 0.0f;
        s.jumpIntentTimer = 0.0f;
        s.airJumpsLeft = c.maximumAirJumps;
        s.airJumpArmed = false;
        s.airJumpLocked = true;
        return;
    }

    if (s.airJumpsLeft > 0 && s.airJumpArmed) {
        s.velocity.z = c.jumpVerticalSpeed;
        --s.airJumpsLeft;
        s.airJumpArmed = false;
        s.airJumpLocked = true;
        s.jumpIntentTimer = 0.0f;
    }
}

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

bool approxEqual(float a, float b, float eps)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

void step(State& s, const Input& in, const Config& c)
{
    const float dt = std::min(c.dt, c.maximumDeltaSeconds);
    if (dt <= 0.0f)
        return;

    // ── PRE-collision: gravity -> freeze -> down-dash ──────────────────
    s.velocity.z += c.gravity * dt;
    if (s.velocity.z < -c.maximumFallSpeed)
        s.velocity.z = -c.maximumFallSpeed;

    updateFreeze(s, in, c, dt);

    if (in.downDashPressed && s.downDashAvailable) {
        s.velocity.z = c.downDashVerticalSpeed;
        s.downDashAvailable = false;
    }

    // [collision is supplied by the caller through State::grounded]

    // ── POST-collision: contact reset -> walk -> dash -> jump ──────────
    if (s.grounded) {
        s.airJumpsLeft = c.maximumAirJumps;
        s.airJumpArmed = true;
        s.airJumpLocked = false;
        s.dashAvailable = true;
        s.downDashAvailable = true;
        s.freezeAvailable = true;
    }

    if (!s.freezeActive)
        applySourceMovement(s, in, c, dt);

    tryActivateDash(s, in, c);
    applyJump(s, in, c, dt);

    // Collision-free integration. The old pipeline integrated inside the
    // collision owner; callers that supply grounded/contact facts still get the
    // same velocity here.
    s.position += s.velocity * dt;
}

bool runMovementAfad20aReferenceSelfTest(std::string& report)
{
    bool ok = true;
    const Config c{};
    const float dt = c.dt;

    // Free fall for one second: velocity approaches gravity * 1s.
    {
        State s{};
        Input in{};
        for (int i = 0; i < 60; ++i)
            step(s, in, c);
        ok &= check(approxEqual(s.velocity.z, c.gravity, 0.5f),
                    "free fall reaches gravity after 1s", report);
    }

    // Down-dash replaces vertical velocity (afad20a set semantics).
    {
        State s{};
        Input in{};
        in.downDashPressed = true;
        step(s, in, c);
        ok &= check(approxEqual(s.velocity.z, c.downDashVerticalSpeed, 1e-3f),
                    "down-dash sets vertical velocity", report);
    }

    // Ground dash from rest: walk applies no input, dash adds the impulse.
    {
        State s{};
        s.grounded = true;
        Input in{};
        in.cameraForward = glm::vec2(1.0f, 0.0f);
        in.dashPressed = true;
        step(s, in, c);
        ok &= check(approxEqual(s.velocity.x, c.dashImpulse, 1e-3f) &&
                        approxEqual(s.velocity.y, 0.0f, 1e-3f),
                    "ground dash adds horizontal impulse", report);
    }

    // Freeze activation hard-stops velocity.
    {
        State s{};
        s.velocity = glm::vec3(5.0f, 3.0f, -2.0f);
        Input in{};
        in.freezeHeld = true;
        in.freezePressed = true;
        step(s, in, c);
        ok &= check(approxEqual(s.velocity.x, 0.0f, 1e-3f) &&
                        approxEqual(s.velocity.y, 0.0f, 1e-3f) &&
                        approxEqual(s.velocity.z, 0.0f, 1e-3f),
                    "freeze activation stops velocity", report);
    }

    // Ground jump sets vertical velocity to the jump speed.
    {
        State s{};
        s.grounded = true;
        Input in{};
        in.jumpPressed = true;
        step(s, in, c);
        ok &= check(approxEqual(s.velocity.z, c.jumpVerticalSpeed, 1e-3f),
                    "ground jump sets jump velocity", report);
    }

    // Air-strafe gain: a wish perpendicular to existing velocity adds speed.
    {
        State s{};
        s.velocity = glm::vec3(10.0f, 0.0f, 0.0f);
        Input in{};
        in.moveAxes = glm::vec2(0.0f, 1.0f);
        const float before = glm::length(glm::vec2(s.velocity));
        step(s, in, c);
        const float after = glm::length(glm::vec2(s.velocity));
        ok &= check(after > before + 1e-3f, "air-strafe adds horizontal speed",
                    report);
    }

    // Determinism: two identical runs agree bit-for-bit.
    {
        State a{};
        State b{};
        Input in{};
        for (int i = 0; i < 240; ++i) {
            step(a, in, c);
            step(b, in, c);
        }
        ok &= check(a.position == b.position && a.velocity == b.velocity,
                    "oracle deterministic", report);
    }

    (void)dt;
    return ok;
}

} // namespace MimitaAfad20a
