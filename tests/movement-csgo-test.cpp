// 08 02 2026, 21 30
/* purpose
* Verifies the CS:GO-inspired ground controller, air steering/gain split, and
* strafe-eligibility layer in the shared movement kernel.
* Covers stale-lateral cleanup, stationary-camera strictness, straight W+Space
* preservation, valid/invalid strafes, caps, and single-tick combined external impulse.
* Uses only the shared movement kernel; no Player, network, render, or audio.
* Does NOT launch mimita.exe, poll input, send packets, or require networking.
*/

#include "physics/movement/movement-step.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kEps = 1e-3f;

int gPassed = 0;
int gFailed = 0;

void check(bool condition, const char* message)
{
    if (condition) {
        ++gPassed;
    } else {
        std::printf("[FAIL] %s\n", message);
        ++gFailed;
    }
}

void checkNear(float actual, float expected, float tolerance, const char* message)
{
    if (std::fabs(actual - expected) <= tolerance) {
        ++gPassed;
    } else {
        std::printf("[FAIL] %s expected=%.4f actual=%.4f\n",
                    message, expected, actual);
        ++gFailed;
    }
}

MovementConfig csgoConfig()
{
    MovementConfig c;
    c.walkMode = MovementWalkMode::Accel;
    c.airControlEnabled = true;
    c.bunnyHopEnabled = true;
    c.autoBhopEnabled = true;
    c.requireActiveWishRotation = true;
    c.stationaryCameraInputMode = StationaryCameraInputMode::Strict;
    c.airSteeringResponse = 1.0f;
    c.maximumSteeringDegreesPerSecond = 720.0f;
    c.airAcceleration = 30.0f;
    c.airMaxWishspeed = 40.0f;
    c.minimumCameraYawDeltaDegrees = 0.25f;
    c.minimumWishRotationDegrees = 0.25f;
    c.strafeAngularToleranceDegrees = 60.0f;
    c.speedCapEnabled = true;
    c.maximumBhopSpeedMode = MovementSpeedCapMode::Soft;
    c.bunnyHopSpeedCap = 40.0f;
    c.softCapStart = 30.0f;
    c.maximumAccelerationPerTick = 1.0f;
    c.preserveStraightSpeed = true;
    c.groundSpeed = 20.0f;
    c.airSpeed = 20.0f;
    c.groundAcceleration = 400.0f;
    c.groundDeceleration = 500.0f;
    c.groundDirectionChangeResponse = 650.0f;
    c.groundFrictionAmount = 80.0f;
    c.airFrictionAmount = 0.0f;
    c.externalImpulseDecay = 0.6f;
    c.maximumExternalImpulseSpeed = 120.0f;
    c.gravityZ = -58.0f;
    c.jumpVerticalSpeed = 19.0f;
    c.maximumFallSpeed = 400.0f;
    c.jumpBufferSeconds = 0.12f;
    c.coyoteSeconds = 0.001f;
    c.maximumAirJumps = 1;
    c.almostZeroSpeed = 0.00001f;
    c.maximumDeltaSeconds = 0.033f;
    return c;
}

MovementState freshState(glm::vec2 velXY = glm::vec2(0.0f))
{
    MovementState s;
    s.lifecycle = MovementLifecycleIdentity{10, 20};
    s.sizeScale = 1.0f;
    s.baseVelocity = glm::vec3(velXY.x, velXY.y, 0.0f);
    s.jump.airJumpsLeft = 1;
    s.dash.frictionOverride = 1.0f;
    return s;
}

MovementCommand cmdFor(glm::vec2 wish, float lookYaw, bool jumpHeld)
{
    MovementCommand c;
    c.lifecycle = MovementLifecycleIdentity{10, 20};
    c.moveAxes = movementClampUnitOrZero(wish);
    c.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    c.lookYaw = lookYaw;
    c.jumpHeld = jumpHeld;
    c.movementDirectionPressed = movementHasMoveInput(wish);
    return c;
}

// Wish direction rotated +90° (direction=1, left strafe) or -90° (right) from
// the current horizontal velocity. lookYaw is advanced by turnDegreesPerTick.
MovementCommand strafeCmd(const MovementState& s,
                          float& lookYaw,
                          float turnDegreesPerTick,
                          int direction)
{
    glm::vec2 vel(s.baseVelocity.x, s.baseVelocity.y);
    glm::vec2 velDir = glm::length(vel) > 1e-4f
        ? glm::normalize(vel)
        : glm::vec2(0.0f, 1.0f);
    const float a = std::atan2(velDir.y, velDir.x) +
        (float)direction * (3.14159265358979f / 2.0f);
    lookYaw += turnDegreesPerTick;
    return cmdFor(glm::vec2(std::cos(a), std::sin(a)), lookYaw, true);
}

MovementCollisionFeedback airCollision()
{
    return MovementCollisionFeedback{};
}

MovementCollisionFeedback groundCollision()
{
    MovementCollisionFeedback c;
    c.onGround = true;
    c.hasWorldContact = true;
    c.realWorldContactThisFrame = true;
    c.groundNormal = glm::vec3(0.0f, 0.0f, 1.0f);
    return c;
}

MovementState runTicks(const MovementState& start,
                       MovementCommand cmd,
                       const MovementConfig& cfg,
                       const MovementCollisionFeedback& collision,
                       int ticks)
{
    MovementState s = start;
    for (int i = 0; i < ticks; ++i) {
        const MovementStepResult r =
            simulateMovementStepWithSpecials(s, cmd, cfg, collision, kDt);
        s = r.state;
    }
    return s;
}

float hSpeed(const MovementState& s)
{
    return glm::length(glm::vec2(s.baseVelocity.x, s.baseVelocity.y));
}

} // namespace

int main()
{
    const MovementConfig cfg = csgoConfig();

    // ── 1. Ground: releasing A removes stale lateral velocity quickly ──
    {
        // vel = (-5, 20): stale leftward drift + forward. Hold W only.
        MovementState s = freshState(glm::vec2(-5.0f, 20.0f));
        s.ground.onGround = true;
        const MovementCommand cmd = cmdFor(glm::vec2(0.0f, 1.0f), 0.0f, false);
        const MovementState end = runTicks(s, cmd, cfg, groundCollision(), 20);
        check(std::fabs(end.baseVelocity.x) < 0.05f,
              "ground A-release clears stale lateral velocity");
        checkNear(end.baseVelocity.y, 20.0f, 1.0f,
                  "ground forward speed preserved after lateral cleanup");
    }

    // ── 2. Stationary-camera air input (strict): A/D does nothing ──────
    {
        const MovementState end = runTicks(
            freshState(glm::vec2(0.0f, 20.0f)),
            cmdFor(glm::vec2(-1.0f, 0.0f), 0.0f, true), cfg, airCollision(), 120);
        checkNear(end.baseVelocity.x, 0.0f, kEps,
                  "strict: holding A with a fixed camera adds no lateral velocity");
        checkNear(end.baseVelocity.y, 20.0f, kEps,
                  "strict: holding A with a fixed camera preserves forward speed");
    }

    // ── 3. Stationary-camera rapid A/D alternation adds no speed ───────
    {
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        for (int i = 0; i < 60; ++i) {
            const glm::vec2 wish = (i % 2 == 0) ? glm::vec2(-1.0f, 0.0f)
                                                : glm::vec2(1.0f, 0.0f);
            s = runTicks(s, cmdFor(wish, 0.0f, true), cfg, airCollision(), 1);
        }
        checkNear(hSpeed(s), 20.0f, kEps,
                  "strict: rapid A/D with a fixed camera adds no speed");
    }

    // ── 4. Camera turn with no input adds nothing ──────────────────────
    {
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i) {
            yaw += 2.0f;
            s = runTicks(s, cmdFor(glm::vec2(0.0f), yaw, true), cfg, airCollision(), 1);
        }
        checkNear(hSpeed(s), 20.0f, kEps,
                  "camera turning with no movement input adds no speed");
    }

    // ── 5. Straight W+Space preserves speed ────────────────────────────
    {
        const float speeds[] = {5.0f, 20.0f, 40.0f};
        for (float speed : speeds) {
            const MovementState end = runTicks(
                freshState(glm::vec2(0.0f, speed)),
                cmdFor(glm::vec2(0.0f, 1.0f), 0.0f, true), cfg, airCollision(), 180);
            checkNear(hSpeed(end), speed, kEps, "straight W+Space preserves speed");
        }
    }

    // ── 6. Valid A-left strafe gains speed ─────────────────────────────
    {
        MovementConfig noCap = cfg;
        noCap.speedCapEnabled = false;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i)
            s = runTicks(s, strafeCmd(s, yaw, 2.0f, +1), noCap, airCollision(), 1);
        check(hSpeed(s) > 20.0f + 5.0f, "A + turn left gains speed (valid strafe)");
        check(s.baseVelocity.x < 0.0f,
              "A + turn left curves velocity left (pole steering)");
    }

    // ── 7. Valid D-right strafe gains speed ────────────────────────────
    {
        MovementConfig noCap = cfg;
        noCap.speedCapEnabled = false;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i)
            s = runTicks(s, strafeCmd(s, yaw, -2.0f, -1), noCap, airCollision(), 1);
        check(hSpeed(s) > 20.0f + 5.0f, "D + turn right gains speed (valid strafe)");
    }

    // ── 8. Any-key rule: A + turn right also gains ─────────────────────
    {
        MovementConfig noCap = cfg;
        noCap.speedCapEnabled = false;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i)
            s = runTicks(s, strafeCmd(s, yaw, -2.0f, +1), noCap, airCollision(), 1);
        check(hSpeed(s) > 20.0f + 2.0f, "A + turn right gains speed (any-key rule)");
    }

    // ── 8b. W + turn gains speed (any-key rule) ────────────────────────
    {
        MovementConfig noCap = cfg;
        noCap.speedCapEnabled = false;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i) {
            yaw += 2.0f;
            const float yr = glm::radians(yaw);
            s = runTicks(s, cmdFor(glm::vec2(std::cos(yr), std::sin(yr)), yaw, true),
                         noCap, airCollision(), 1);
        }
        check(hSpeed(s) > 20.0f + 2.0f, "W + turn gains speed (any-key rule)");
    }

    // ── 9. Backward strafe gains speed (direction-agnostic rule) ───────
    {
        MovementConfig noCap = cfg;
        noCap.speedCapEnabled = false;
        MovementState s = freshState(glm::vec2(0.0f, -20.0f)); // moving backward
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i)
            s = runTicks(s, strafeCmd(s, yaw, -2.0f, -1), noCap, airCollision(), 1);
        check(hSpeed(s) > 20.0f + 2.0f, "backward strafe gains speed");
    }

    // ── 10. Hard cap clamps speed; knockback combines into velocity ─────
    {
        MovementConfig hard = cfg;
        hard.maximumBhopSpeedMode = MovementSpeedCapMode::Hard;
        hard.bunnyHopSpeedCap = 30.0f;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        s.externalImpulse = glm::vec3(6.0f, 0.0f, 0.0f);
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i)
            s = runTicks(s, strafeCmd(s, yaw, 2.0f, +1), hard, airCollision(), 1);
        check(hSpeed(s) <= 30.0f + 1e-3f, "hard cap clamps combined speed");
        check(s.externalImpulse.x == 0.0f,
              "external impulse is combined into velocity on the first tick");
    }

    // ── 11. Soft cap fades new gain, stays under the cap ───────────────
    {
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 240; ++i)
            s = runTicks(s, strafeCmd(s, yaw, 2.0f, +1), cfg, airCollision(), 1);
        check(hSpeed(s) <= 40.0f + 1e-3f, "soft cap keeps speed under the cap");
        check(hSpeed(s) > 20.0f, "soft cap still allows gain below the cap");
    }

    // ── 12. Speed-gain strength is independent of steering rate ────────
    {
        MovementConfig noCap = cfg;
        noCap.speedCapEnabled = false;
        MovementConfig weakGain = noCap;
        weakGain.airAcceleration = 1.0f;
        MovementState a = freshState(glm::vec2(0.0f, 20.0f));
        MovementState b = freshState(glm::vec2(0.0f, 20.0f));
        float ya = 0.0f, yb = 0.0f;
        for (int i = 0; i < 60; ++i) {
            a = runTicks(a, strafeCmd(a, ya, 2.0f, +1), noCap, airCollision(), 1);
            b = runTicks(b, strafeCmd(b, yb, 2.0f, +1), weakGain, airCollision(), 1);
        }
        check(hSpeed(a) > hSpeed(b) + 1.0f,
              "air speed-gain acceleration changes peak speed independently");
    }

    // ── 13. Ground strafing never runs air acceleration ────────────────
    {
        MovementConfig noCap = cfg;
        noCap.speedCapEnabled = false;
        // Hold W and turn the camera on the ground: the ground controller
        // keeps speed at ground speed; air acceleration never runs.
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        s.ground.onGround = true;
        float yaw = 0.0f;
        for (int i = 0; i < 60; ++i) {
            yaw += 2.0f;
            s = runTicks(s, cmdFor(glm::vec2(0.0f, 1.0f), yaw, true),
                         noCap, groundCollision(), 1);
        }
        checkNear(hSpeed(s), 20.0f, 0.5f,
                  "ground movement never gains bhop speed");
    }

    std::printf("\n[movement-csgo-test] passed=%d failed=%d\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
