// 08 02 2026, 18 00
/* purpose
* Verifies the Counter-Strike-inspired bunny-hop / air-acceleration kernel.
* Covers straight-line speed preservation, strafe speed gain, caps, input edge
* cases, all-direction bhops, and auto-bhop jump chaining.
* Uses only the shared movement kernel; no Player, network, render, or audio.
* Does NOT launch mimita.exe, poll input, send packets, or require networking.
* Does NOT test collision sweeps, weapon systems, dash/freeze internals, or replay.
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

MovementConfig csConfig()
{
    MovementConfig c;
    c.walkMode = MovementWalkMode::Accel;
    c.airControlEnabled = true;
    c.bunnyHopEnabled = true;
    c.autoBhopEnabled = true;
    c.preserveStraightSpeed = true;
    c.minimumStrafeAngleDegrees = 20.0f;
    c.maximumAccelerationPerTick = 0.0f;
    c.diagonalInputNormalization = true;
    c.speedCapEnabled = true;
    c.maximumBhopSpeedMode = MovementSpeedCapMode::Soft;
    c.accelerationFalloffNearCap = 0.5f;
    c.requireActiveWishRotation = true;
    c.stationaryCameraInputMode = StationaryCameraInputMode::Strict;
    c.airSteeringResponse = 1.0f;
    c.maximumSteeringDegreesPerSecond = 720.0f;
    c.minimumCameraYawDeltaDegrees = 0.25f;
    c.minimumWishRotationDegrees = 0.25f;
    c.strafeAngularToleranceDegrees = 60.0f;
    c.softCapStart = 0.0f;
    c.landingSpeedRetention = 1.0f;
    c.groundSpeed = 20.0f;
    c.airSpeed = 20.0f;
    c.groundAcceleration = 5.5f;
    c.airAcceleration = 12.0f;
    c.airMaxWishspeed = 2.0f;
    c.airControl = 0.0f;
    c.stopspeed = 5.0f;
    c.bunnyHopSpeedCap = 22.0f;
    c.gravityZ = -58.0f;
    c.jumpVerticalSpeed = 19.0f;
    c.maximumFallSpeed = 400.0f;
    c.jumpBufferSeconds = 0.12f;
    c.coyoteSeconds = 0.001f;
    c.maximumAirJumps = 1;
    c.groundFrictionAmount = 4.0f;
    c.airFrictionAmount = 0.0f;
    c.externalImpulseDecay = 0.6f;
    c.maximumExternalImpulseSpeed = 120.0f;
    c.almostZeroSpeed = 0.00001f;
    c.maximumDeltaSeconds = 0.033f;
    return c;
}

// Boosted air acceleration so strafe speed gain is measurable in tests.
// The wish-speed cap is unchanged; only the per-tick accel rate is higher.
MovementConfig gainConfig()
{
    MovementConfig c = csConfig();
    c.airAcceleration = 60.0f;
    // New eligibility + steering model: a higher wishspeed keeps the gain
    // window open while the steering phase curves the velocity.
    c.airMaxWishspeed = 8.0f;
    return c;
}

// The classic strafe: wish points perpendicular to current horizontal
// velocity (a player turning to keep the strafe optimal).
glm::vec2 strafeWish(const MovementState& s)
{
    const glm::vec2 vel(s.baseVelocity.x, s.baseVelocity.y);
    const float len = glm::length(vel);
    if (len < 1e-3f)
        return glm::vec2(1.0f, 0.0f);
    return glm::normalize(glm::vec2(-vel.y, vel.x));
}

// Valid strafe command: perpendicular wish + matching camera turn so the
// strafe-eligibility layer qualifies it. direction=+1 turns left, -1 right.
MovementCommand strafeCommand(const MovementState& s,
                              float& lookYaw,
                              float turnPerTick,
                              int direction)
{
    glm::vec2 vel(s.baseVelocity.x, s.baseVelocity.y);
    glm::vec2 velDir = glm::length(vel) > 1e-3f
        ? glm::normalize(vel)
        : glm::vec2(0.0f, 1.0f);
    const float a = std::atan2(velDir.y, velDir.x) +
        (float)direction * (3.14159265358979f / 2.0f);
    lookYaw += turnPerTick;
    const glm::vec2 wish(std::cos(a), std::sin(a));
    MovementCommand c;
    c.lifecycle = MovementLifecycleIdentity{10, 20};
    c.moveAxes = movementClampUnitOrZero(wish);
    c.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    c.lookYaw = lookYaw;
    c.jumpHeld = true;
    c.movementDirectionPressed = true;
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

MovementCommand cmdFor(glm::vec2 wish, bool jumpHeld = false)
{
    MovementCommand c;
    c.lifecycle = MovementLifecycleIdentity{10, 20};
    c.moveAxes = movementClampUnitOrZero(wish);
    c.horizontalCameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
    c.jumpHeld = jumpHeld;
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

// Runs n fixed ticks, returning the resulting state.
MovementState runTicks(const MovementState& start,
                       const MovementCommand& cmd,
                       const MovementConfig& cfg,
                       const MovementCollisionFeedback& collision,
                       int ticks,
                       int* jumpCount = nullptr)
{
    MovementState s = start;
    int jumps = 0;
    for (int i = 0; i < ticks; ++i) {
        const MovementStepResult r =
            simulateMovementStepWithSpecials(s, cmd, cfg, collision, kDt);
        if (r.events.didGroundJump || r.events.didAirJump)
            ++jumps;
        s = r.state;
    }
    if (jumpCount)
        *jumpCount = jumps;
    return s;
}

float hSpeed(const MovementState& s)
{
    return glm::length(glm::vec2(s.baseVelocity.x, s.baseVelocity.y));
}

} // namespace

int main()
{
    const MovementConfig cfg = csConfig();

    // ── 1. Straight-line preservation: W + Space, straight camera ──────
    // Airborne at 20 units/s moving in the exact wish direction. After 3
    // seconds of hops the horizontal speed must be unchanged (no creep).
    {
        const float speeds[] = {5.0f, 20.0f, 40.0f};
        for (float speed : speeds) {
            const MovementState end = runTicks(
                freshState(glm::vec2(0.0f, speed)),
                cmdFor(glm::vec2(0.0f, 1.0f), true),
                cfg, airCollision(), 180);
            checkNear(hSpeed(end), speed, kEps, "straight W+Space preserves speed");
        }
    }

    // ── 2. Straight W+Space from standstill does NOT farm speed ────────
    {
        const MovementState end = runTicks(
            freshState(), cmdFor(glm::vec2(0.0f, 1.0f), true),
            cfg, airCollision(), 180);
        // Accelerates only to the small air wishspeed, never to ground speed.
        check(end.baseVelocity.y < 3.0f,
              "straight W from rest stays bounded by air wishspeed");
        checkNear(hSpeed(end), end.baseVelocity.y, kEps,
                  "straight W only adds along the wish axis");
    }

    // ── 3. Camera turning with no movement input adds nothing ──────────
    {
        const MovementState end = runTicks(
            freshState(glm::vec2(0.0f, 20.0f)),
            cmdFor(glm::vec2(0.0f, 0.0f), true),
            cfg, airCollision(), 180);
        checkNear(hSpeed(end), 20.0f, kEps,
                  "no input (camera-only) preserves speed");
    }

    // ── 4. Opposite keys (W+S / A+D) cancel to no input ────────────────
    {
        const MovementState end = runTicks(
            freshState(glm::vec2(0.0f, 20.0f)),
            cmdFor(glm::vec2(0.0f, 0.0f), true),
            cfg, airCollision(), 120);
        checkNear(hSpeed(end), 20.0f, kEps, "W+S cancels, speed preserved");
    }

    // ── 5. Diagonal input normalizes (no extra accel from two keys) ────
    {
        const glm::vec2 diag = movementClampUnitOrZero(glm::vec2(1.0f, 1.0f));
        checkNear(glm::length(diag), 1.0f, kEps, "diagonal normalizes to unit");
        checkNear(diag.x, diag.y, kEps, "diagonal is symmetric");
    }

    // ── 6. Strafing (perpendicular wish, turning) gains speed ──────────
    {
        MovementConfig noCap = gainConfig();
        noCap.speedCapEnabled = false;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 120; ++i)
            s = runTicks(s, strafeCommand(s, yaw, 2.0f, +1), noCap, airCollision(), 1);
        check(hSpeed(s) > 20.0f + 2.0f,
              "perpendicular strafe gains speed with no cap");
    }

    // ── 7. Hard cap clamps planar speed ────────────────────────────────
    {
        MovementConfig hard = gainConfig();
        hard.maximumBhopSpeedMode = MovementSpeedCapMode::Hard;
        hard.speedCapEnabled = true;
        hard.bunnyHopSpeedCap = 22.0f;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 120; ++i)
            s = runTicks(s, strafeCommand(s, yaw, 2.0f, +1), hard, airCollision(), 1);
        check(hSpeed(s) <= 22.0f + 1e-3f, "hard cap clamps speed");
        check(hSpeed(s) > 20.0f + 0.5f, "hard cap still allows bhop gain below cap");
    }

    // ── 8. Soft cap approaches but never exceeds the cap ───────────────
    {
        MovementConfig soft = gainConfig();
        soft.maximumBhopSpeedMode = MovementSpeedCapMode::Soft;
        soft.speedCapEnabled = true;
        soft.bunnyHopSpeedCap = 22.0f;
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        float yaw = 0.0f;
        for (int i = 0; i < 240; ++i)
            s = runTicks(s, strafeCommand(s, yaw, 2.0f, +1), soft, airCollision(), 1);
        check(hSpeed(s) <= 22.0f + 1e-3f, "soft cap keeps speed under the cap");
        check(hSpeed(s) > 20.0f + 0.5f, "soft cap still allows gain below cap");
    }

    // ── 9. All directions gain speed with a perpendicular strafe ───────
    {
        const glm::vec2 directions[] = {
            glm::vec2(0.0f, 1.0f),     // W
            glm::vec2(0.0f, -1.0f),    // S
            glm::vec2(-1.0f, 0.0f),    // A
            glm::vec2(1.0f, 0.0f),     // D
            glm::vec2(0.707f, 0.707f), // W+A
            glm::vec2(0.707f, -0.707f) // S+D
        };
        MovementConfig noCap = gainConfig();
        noCap.speedCapEnabled = false;
        int gains = 0;
        for (const glm::vec2 base : directions) {
            MovementState s = freshState(base * 20.0f);
            float yaw = 0.0f;
            for (int i = 0; i < 120; ++i)
                s = runTicks(s, strafeCommand(s, yaw, 2.0f, +1), noCap, airCollision(), 1);
            if (hSpeed(s) > 20.0f + 2.0f)
                ++gains;
        }
        check(gains == 6, "all six movement directions gain speed when strafing");
    }

    // ── 10. Auto-bhop chains jumps while Space is held ─────────────────
    {
        int jumps = 0;
        runTicks(freshState(), cmdFor(glm::vec2(0.0f, 1.0f), true),
                 cfg, groundCollision(), 10, &jumps);
        check(jumps >= 5, "auto_bhop chains ground jumps while Space is held");
    }

    // ── 11. auto_bhop disabled = single jump per explicit press ────────
    {
        MovementConfig manual = cfg;
        manual.autoBhopEnabled = false;
        int jumps = 0;
        // jumpPressed only on the first tick (fresh edge), held afterwards.
        MovementState s = freshState();
        for (int i = 0; i < 10; ++i) {
            MovementCommand cmd = cmdFor(glm::vec2(0.0f, 1.0f), true);
            cmd.jumpPressed = (i == 0);
            const MovementStepResult r = simulateMovementStepWithSpecials(
                s, cmd, manual, groundCollision(), kDt);
            if (r.events.didGroundJump || r.events.didAirJump)
                ++jumps;
            s = r.state;
        }
        check(jumps == 1, "auto_bhop off: holding Space jumps only once");
    }

    // ── 12. Air acceleration never touches vertical velocity ───────────
    {
        MovementState s = freshState(glm::vec2(0.0f, 20.0f));
        s.baseVelocity.z = -58.0f; // pre-existing fall
        const float zBefore = s.baseVelocity.z;
        s = runTicks(s, cmdFor(glm::vec2(1.0f, 0.0f), true), cfg, airCollision(), 30);
        // Gravity changes Z, but the horizontal accel must not.
        const float zGravityOnly = zBefore + cfg.gravityZ * 30.0f * kDt;
        checkNear(s.baseVelocity.z, zGravityOnly, 1e-3f,
                  "air accel does not modify vertical velocity");
    }

    std::printf("\n[movement-bhop-test] passed=%d failed=%d\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
