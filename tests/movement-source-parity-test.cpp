// 08 15 2026, 16 12
/* purpose
* Verifies the Source (CS/Quake PM_) movement kernel in the shared movement step.
* Covers ground accel to maxspeed, stopspeed friction, Source air projection,
* jump velocity preservation, bug-compatible acceleration, dash impulse
* persistence through input, and source-style external impulse carry + bleed.
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

// MiMITA-feel speed (20) with Source-shaped behavior. Air is pure strafing.
MovementConfig sourceConfig()
{
    MovementConfig c;
    c.walkMode = MovementWalkMode::Source;
    c.airControlEnabled = true;
    c.autoBhopEnabled = true;
    c.groundSpeed = 20.0f;
    c.sourceMaxSpeed = 20.0f;
    c.groundAcceleration = 5.5f;
    c.sourceFriction = 5.2f;
    c.stopspeed = 3.75f;
    c.airAcceleration = 12.0f;
    c.airMaxWishspeed = 1.875f; // Source 30/320 ratio scaled to maxspeed 20.
    c.sourceAirAccelerateBugCompatible = true;
    c.airControl = 1.5f;
    c.airSpeedGainMultiplier = 1.0f;
    c.gravityZ = -40.0f;
    c.jumpVerticalSpeed = 15.1f;
    c.maximumFallSpeed = 175.0f;
    c.jumpBufferSeconds = 0.2f;
    c.coyoteSeconds = 0.0f;
    c.maximumAirJumps = 0;
    c.groundSnap = true;
    c.velocityClipEpsilon = 0.01f;
    c.surfaceFriction = 1.0f;
    c.landingOverspeedBleed = 1.0f;
    c.dashGraceSeconds = 0.25f;
    c.dashFrictionMultiplier = 0.0f;
    c.groundDashImpulse = 100.0f;
    c.airDashImpulse = 50.0f;
    c.downDashVerticalSpeed = -100.0f;
    c.impulseFrictionMode = MovementImpulseFrictionMode::Source;
    c.impulseCarrySeconds = 0.1f;
    c.externalImpulseDecay = 0.0f;
    c.maximumExternalImpulseSpeed = 175.0f;
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
    s.dash.frictionOverride = 1.0f;
    return s;
}

MovementCommand cmdFor(glm::vec2 wish, bool jumpHeld = false, bool dashPressed = false)
{
    MovementCommand c;
    c.lifecycle = MovementLifecycleIdentity{10, 20};
    c.moveAxes = movementClampUnitOrZero(wish);
    c.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    c.lookYaw = 0.0f;
    c.jumpHeld = jumpHeld;
    c.dashPressed = dashPressed;
    c.movementDirectionPressed = movementHasMoveInput(wish);
    return c;
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
        cmd.sequence = (uint32_t)i + 1;
        const MovementStepResult r =
            simulateMovementStepWithSpecials(s, cmd, cfg, collision, kDt);
        s = r.state;
    }
    return s;
}

// Simulate holding a strafe key while the camera turns turnDegreesPerTick each
// tick. The world wish follows the camera exactly like the real input layer
// (holding A/D = camera-left/right recomputed every tick). key: -1 = A, +1 = D.
// The camera starts facing +y (the "look" direction). Pass a forward velocity
// (0,+20) for moving forward, or a backward velocity (0,-20) for backing up —
// the camera still faces +y, exactly like a player holding S.
MovementState runStrafeKeyTicks(const MovementState& start,
                                const MovementConfig& cfg,
                                int key,
                                float turnDegreesPerTick,
                                int ticks)
{
    MovementState s = start;
    float yaw = 90.0f; // camera always faces +y at the start
    for (int i = 0; i < ticks; ++i) {
        s.yaw = yaw;               // becomes previousYaw inside the kernel
        yaw += turnDegreesPerTick; // this tick's camera yaw
        const float rad = glm::radians(yaw);
        const glm::vec2 fwd(std::cos(rad), std::sin(rad));
        // Matches the real input layer: right = cross(forward, up) flattened.
        const glm::vec2 right(fwd.y, -fwd.x);
        const glm::vec2 wish = key < 0 ? -right : right; // A = -right, D = +right
        MovementCommand c = cmdFor(wish);
        c.lookYaw = yaw;
        const MovementStepResult r =
            simulateMovementStepWithSpecials(s, c, cfg, airCollision(), kDt);
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
    const MovementConfig cfg = sourceConfig();

    // ── 1. Ground: hold W accelerates to maxSpeed and stays there ────────
    {
        MovementState s = freshState();
        s.ground.onGround = true;
        const MovementState end =
            runTicks(s, cmdFor(glm::vec2(0.0f, 1.0f)), cfg, groundCollision(), 240);
        checkNear(hSpeed(end), 20.0f, 0.25f,
                  "ground W reaches maxSpeed 20");
        check(end.baseVelocity.x > -0.05f && end.baseVelocity.x < 0.05f,
              "ground W keeps lateral velocity ~0");
    }

    // ── 2. Ground: release keys -> stopspeed friction brings you to rest ──
    {
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        s.ground.onGround = true;
        const MovementState end =
            runTicks(s, cmdFor(glm::vec2(0.0f)), cfg, groundCollision(), 90);
        checkNear(hSpeed(end), 0.0f, 0.05f,
                  "ground friction stops the player after key release");
    }

    // ── 3. Air: A alone fills only Source's small projection cap ─────────
    {
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        const MovementState end =
            runTicks(s, cmdFor(glm::vec2(-1.0f, 0.0f)), cfg, airCollision(), 60);
        std::printf("[AIR] A-alone(60t) hspeed=%.2f vx=%.2f\n",
                    hSpeed(end), end.baseVelocity.x);
        checkNear(end.baseVelocity.x, -1.875f, 0.01f,
                  "air: A alone fills only the 1.875 projection cap");
        check(hSpeed(end) < 20.1f,
              "air: A alone does not become direct lateral air-walk");
    }

    // ── 3b. Standstill jump + A/W = no horizontal launch ─────────────────
    {
        MovementState s = freshState();
        s.ground.onGround = true;
        const MovementState withA = runTicks(
            s, cmdFor(glm::vec2(-1.0f, 0.0f), true), cfg, groundCollision(), 1);
        const MovementState withW = runTicks(
            s, cmdFor(glm::vec2(0.0f, 1.0f), true), cfg, groundCollision(), 1);
        check(withA.baseVelocity.z > 0.0f && withW.baseVelocity.z > 0.0f,
              "jump: A and W cases both leave the ground");
        checkNear(hSpeed(withA), 0.0f, 0.01f,
                  "jump: zero velocity + hold A adds no horizontal speed");
        checkNear(hSpeed(withW), 0.0f, 0.01f,
                  "jump: zero velocity + hold W adds no horizontal speed");
    }

    // ── 3c. Source bug compatibility chooses acceleration wishspeed ───────
    {
        MovementConfig fixed = cfg;
        fixed.sourceAirAccelerateBugCompatible = false;
        const MovementState bug = runTicks(
            freshState(glm::vec2(0.0f, 20.0f)),
            cmdFor(glm::vec2(-1.0f, 0.0f)), cfg, airCollision(), 1);
        const MovementState noBug = runTicks(
            freshState(glm::vec2(0.0f, 20.0f)),
            cmdFor(glm::vec2(-1.0f, 0.0f)), fixed, airCollision(), 1);
        checkNear(bug.airDebug.accelSpeed, 1.875f, 0.01f,
                  "air: Valve bug-compatible accel uses uncapped wishspeed");
        checkNear(noBug.airDebug.accelSpeed, 0.375f, 0.01f,
                  "air: fixed accel option uses capped wishspd");
    }

    // ── 3d. Air: Source projection gain — the CS matching cases ──────────
    {
        const int n = 120;

        // 1. forward + A + turn left = matching strafe -> clear gain.
        const MovementState fwdLeft =
            runStrafeKeyTicks(freshState(glm::vec2(0.0f, 20.0f)), cfg, -1, +2.0f, n);
        // 2. forward + A + turn right = counter-strafe -> little/no gain.
        const MovementState fwdRight =
            runStrafeKeyTicks(freshState(glm::vec2(0.0f, 20.0f)), cfg, -1, -2.0f, n);
        // 3. backward + A + turn right = matching (basis flipped) -> gain.
        const MovementState backRight =
            runStrafeKeyTicks(freshState(glm::vec2(0.0f, -20.0f)), cfg, -1, -2.0f, n);
        // 4. backward + A + turn left = counter -> little/no gain.
        const MovementState backLeft =
            runStrafeKeyTicks(freshState(glm::vec2(0.0f, -20.0f)), cfg, -1, +2.0f, n);
        // 5. zero horizontal velocity + A + mouse turn = no free speed.
        const MovementState zero =
            runStrafeKeyTicks(freshState(), cfg, -1, +2.0f, n);

        std::printf("[AIR GAIN] fwd+A+left=%.2f  fwd+A+right=%.2f  "
                    "back+A+right=%.2f  back+A+left=%.2f  zero=%.3f\n",
                    hSpeed(fwdLeft), hSpeed(fwdRight),
                    hSpeed(backRight), hSpeed(backLeft), hSpeed(zero));

        check(hSpeed(fwdLeft) > 20.5f,
              "air 1: forward + A + left turn gains speed (matching strafe)");
        check(hSpeed(fwdRight) < hSpeed(fwdLeft) - 3.0f,
              "air 2: forward + A + right turn gains far less (counter-strafe)");
        check(hSpeed(backRight) > 20.5f,
              "air 3: backward + A + right turn gains speed (basis flipped)");
        check(hSpeed(backLeft) < hSpeed(backRight) - 3.0f,
              "air 4: backward + A + left turn gains far less (counter-strafe)");
        checkNear(hSpeed(zero), 0.0f, 0.01f,
                  "air 5: zero horizontal velocity + A + turn gains no speed");
    }

    // ── 4. Air: no input preserves horizontal speed and direction ────────
    {
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        const MovementState end =
            runTicks(s, cmdFor(glm::vec2(0.0f)), cfg, airCollision(), 180);
        checkNear(hSpeed(end), 20.0f, kEps,
                  "air: no input preserves speed");
        checkNear(end.baseVelocity.x, 0.0f, kEps,
                  "air: no input preserves direction");
    }

    // ── 5. Source jump ordering preserves horizontal velocity ────────────
    {
        MovementState s = freshState(glm::vec2(0.0f, 30.0f)); // overspeed
        s.ground.onGround = false;
        s.jump.jumpIntentTimerSeconds = 0.2f; // autobhop buffered from air
        const MovementState end =
            runTicks(s, cmdFor(glm::vec2(0.0f, 1.0f), true), cfg, groundCollision(), 1);
        checkNear(end.baseVelocity.y, 30.0f, 0.01f,
                  "jump: CheckJump-style ordering preserves horizontal speed");
        check(end.baseVelocity.z > 10.0f,
              "jump: autobhop still jumps on the landing tick");
    }

    // ── 5b. Default MiMITA mode retains direct legacy air movement ───────
    {
        MovementConfig mimita = cfg;
        mimita.walkMode = MovementWalkMode::Override;
        mimita.airSpeed = 20.0f;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        const MovementState end = runTicks(
            s, cmdFor(glm::vec2(-1.0f, 0.0f)), mimita, airCollision(), 1);
        checkNear(end.baseVelocity.x, -20.0f, 0.01f,
                  "mimita mode preserves legacy direct air direction");
    }

    // ── 6. Dash impulse persists through input during dash grace ─────────
    {
        // Dash forward while holding W: baseVelocity jumps toward maxSpeed + impulse.
        MovementState s = freshState();
        s.ground.onGround = true;
        s.baseVelocity.y = 20.0f;
        const MovementState dashed =
            runTicks(s, cmdFor(glm::vec2(0.0f, 1.0f), false, true), cfg, groundCollision(), 1);
        check(dashed.dash.didDash, "dash activates");
        check(dashed.baseVelocity.y > 100.0f,
              "dash adds a one-time impulse on top of walk speed");

        // Keep holding W: the boost is protected by dash grace (friction 0).
        const MovementState held =
            runTicks(dashed, cmdFor(glm::vec2(0.0f, 1.0f)), cfg, groundCollision(), 5);
        check(held.baseVelocity.y > 100.0f,
              "dash impulse carries while W is held (grace window)");

        // Release W, then re-press W: impulse is not erased by input changes.
        const MovementState released =
            runTicks(held, cmdFor(glm::vec2(0.0f)), cfg, groundCollision(), 3);
        const MovementState repressed =
            runTicks(released, cmdFor(glm::vec2(0.0f, 1.0f)), cfg, groundCollision(), 3);
        check(repressed.baseVelocity.y > 100.0f,
              "dash impulse survives release + re-press of W (input never erases it)");

        // After the grace window passes, ground friction slowly bleeds the boost.
        const MovementState afterGrace =
            runTicks(repressed, cmdFor(glm::vec2(0.0f, 1.0f)), cfg, groundCollision(), 30);
        check(afterGrace.baseVelocity.y < 100.0f,
              "dash boost slowly bleeds via friction after the grace window");
    }

    // ── 7. Source external impulse: carries, then bleeds via ground friction ──
    {
        MovementState s = freshState();
        s.ground.onGround = true;
        s.externalImpulse = glm::vec3(30.0f, 0.0f, 0.0f);
        const glm::vec3 before = s.externalImpulse;

        // Carry window (0.1s = 6 ticks): impulse is untouched.
        const MovementState carrying =
            runTicks(s, cmdFor(glm::vec2(0.0f, 1.0f)), cfg, groundCollision(), 6);
        checkNear(carrying.externalImpulse.x, before.x, 0.01f,
                  "impulse carries untouched through the carry window");

        // After carry: ground friction bleeds it (30 * 5.2/60 = 2.6/tick).
        const MovementState bleeding =
            runTicks(carrying, cmdFor(glm::vec2(0.0f, 1.0f)), cfg, groundCollision(), 12);
        check(bleeding.externalImpulse.x < before.x * 0.5f,
              "impulse bleeds via ground friction after the carry window");
        check(bleeding.externalImpulse.x > 0.0f,
              "impulse is not erased by holding W (input never clears it)");
    }

    std::printf("\n[movement-source-parity-test] passed=%d failed=%d\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
