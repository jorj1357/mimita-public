// 09 15 2026
/* purpose
* Implements the headless hot movement-algorithm self-test: the air-acceleration
* ALGORITHM is hot-owned. The test drives the generic movement.air-accelerate
* fact with plain-number inputs and shows the hot function's velocity differs
* from the cold built-in formula (so behavior is function-determined, not just a
* constant), is deterministic, and is reusable for any actor. No
* Player/NPC-specific movement ABI.
* Does NOT own rendering/presentation or the network transport.
*/
#include "network/movement-algorithm-selftest.h"

#include <cmath>
#include <string>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

bool approx(float a, float b, float eps = 1e-3f)
{
    return std::fabs(a - b) <= eps;
}

// The cold built-in air formula for comparison (see movement-step.cpp).
void coldAir(const GameAirAccelerateV1& p, float& outX, float& outY)
{
    float addSpeed = p.wishspd - p.currentSpeed;
    float base = p.airAcceleration * p.wishSpeed * p.dt * p.surfaceFriction *
                 p.airSpeedGainMultiplier;
    float gain = base < addSpeed ? base : addSpeed;
    if (gain < 0.0f)
        gain = 0.0f;
    outX = p.velocity[0] + p.wishDir[0] * gain;
    outY = p.velocity[1] + p.wishDir[1] * gain;
}

GameAirAccelerateV1 makeInput()
{
    GameAirAccelerateV1 p{};
    p.velocity[0] = 10.0f;
    p.velocity[1] = 0.0f;
    p.wishDir[0] = 0.70710678f;
    p.wishDir[1] = 0.70710678f;
    p.wishSpeed = 20.0f;
    p.wishspd = 20.0f;
    p.maxSpeed = 20.0f;
    p.airAcceleration = 12.0f;
    p.surfaceFriction = 1.0f;
    p.airSpeedGainMultiplier = 1.0f;
    p.dt = 1.0f / 60.0f;
    p.currentSpeed = 7.0710678f;
    p.blendedAddSpeed = 12.928932f;
    p.handled = 0;
    return p;
}

bool dispatch(GameAirAccelerateV1& p)
{
    return LiveBehavior::dispatchGameplayEvent64(
        GAME_EVENT_MOVEMENT_AIR_ACCELERATE, &p, sizeof(p), 0, 0, 0);
}

} // namespace

bool runMovementAlgorithmSelfTest(std::string& report)
{
    bool ok = true;
    HotReloadSystem::instance().startup();
    ok &= check(HotReloadSystem::instance().status().activeGeneration != 0,
                "hot package active", report);

    GameAirAccelerateV1 p = makeInput();
    const bool handled = dispatch(p);
    ok &= check(handled && p.handled == 1u,
                "hot movement algorithm handles the air-accelerate step", report);

    // Hot algorithm: projected-speed diminishing-returns gain.
    // projected = 7.071; addSpeed = 12.929; base = 4.0; headroom = 0.64645;
    // gain = 2.5858 -> out = (10 + 0.7071*2.5858, 0.7071*2.5858) = (11.828, 1.828)
    ok &= check(approx(p.outVelocity[0], 11.8284f) && approx(p.outVelocity[1], 1.8284f),
                "hot algorithm output matches its (projected-gain) function", report);

    float coldX = 0.0f, coldY = 0.0f;
    coldAir(makeInput(), coldX, coldY);
    ok &= check(!approx(p.outVelocity[0], coldX, 0.01f) ||
                    !approx(p.outVelocity[1], coldY, 0.01f),
                "hot algorithm differs from the cold built-in formula (function, "
                "not a constant)", report);

    // Determinism.
    {
        GameAirAccelerateV1 a = makeInput();
        GameAirAccelerateV1 b = makeInput();
        dispatch(a);
        dispatch(b);
        ok &= check(approx(a.outVelocity[0], b.outVelocity[0]) &&
                        approx(a.outVelocity[1], b.outVelocity[1]),
                    "hot movement algorithm is deterministic for identical input",
                    report);
    }

    // Generic + reusable: inputs are plain numbers, so any actor (server
    // simulation, prediction, or a runtime entity) uses the same function.
    {
        GameAirAccelerateV1 stopped{};
        stopped.wishDir[0] = 0.0f;
        stopped.wishDir[1] = 1.0f;
        stopped.wishSpeed = 20.0f;
        stopped.wishspd = 20.0f;
        stopped.airAcceleration = 12.0f;
        stopped.surfaceFriction = 1.0f;
        stopped.airSpeedGainMultiplier = 1.0f;
        stopped.dt = 1.0f / 60.0f;
        stopped.currentSpeed = 0.0f;
        // velocity is zero and wishDir perpendicular: projected = 0, addSpeed=20,
        // headroom=1, gain=4 -> (0, 4).
        dispatch(stopped);
        ok &= check(stopped.handled == 1u && approx(stopped.outVelocity[1], 4.0f) &&
                        approx(stopped.outVelocity[0], 0.0f),
                    "same hot function works for a generic numeric actor state",
                    report);
    }

    // Single shared implementation: the event path (used by server authority)
    // and the direct `MimitaHotMovement::airAccelerate` call (used by local
    // prediction) are the same function. Here we re-derive the shared formula
    // and assert the event handler reproduces it exactly.
    {
        auto sharedAir = [](const GameAirAccelerateV1& s, float& ox, float& oy) {
            float projected =
                s.velocity[0] * s.wishDir[0] + s.velocity[1] * s.wishDir[1];
            if (projected < 0.0f)
                projected = 0.0f;
            float addSpeed = s.wishspd - projected;
            if (addSpeed <= 0.0f) {
                ox = s.velocity[0];
                oy = s.velocity[1];
                return;
            }
            const float base =
                s.airAcceleration * s.wishSpeed * s.dt * s.surfaceFriction;
            const float headroom = s.wishspd > 0.0f ? addSpeed / s.wishspd : 0.0f;
            float gain = base * headroom * s.airSpeedGainMultiplier;
            if (gain > addSpeed)
                gain = addSpeed;
            if (gain < 0.0f)
                gain = 0.0f;
            ox = s.velocity[0] + s.wishDir[0] * gain;
            oy = s.velocity[1] + s.wishDir[1] * gain;
        };
        GameAirAccelerateV1 viaEvent = makeInput();
        dispatch(viaEvent);
        float sx = 0.0f, sy = 0.0f;
        sharedAir(makeInput(), sx, sy);
        ok &= check(approx(viaEvent.outVelocity[0], sx) &&
                        approx(viaEvent.outVelocity[1], sy),
                    "one shared air implementation: event path == shared function",
                    report);
    }

    // Shared-policy parity precondition: the SAME hot function drives two
    // independent movement contexts (authoritative + prediction) identically
    // over a multi-tick sequence, using only generic numeric state.
    {
        auto sim = [](float vx, float vy, int ticks, float& ox, float& oy) {
            for (int i = 0; i < ticks; ++i) {
                GameAirAccelerateV1 s{};
                s.velocity[0] = vx;
                s.velocity[1] = vy;
                s.wishDir[0] = 0.70710678f;
                s.wishDir[1] = 0.70710678f;
                s.wishSpeed = 20.0f;
                s.wishspd = 20.0f;
                s.airAcceleration = 12.0f;
                s.surfaceFriction = 1.0f;
                s.airSpeedGainMultiplier = 1.0f;
                s.dt = 1.0f / 60.0f;
                s.currentSpeed = vx * s.wishDir[0] + vy * s.wishDir[1];
                s.blendedAddSpeed = s.wishspd - s.currentSpeed;
                if (!dispatch(s) || !s.handled)
                    break;
                vx = s.outVelocity[0];
                vy = s.outVelocity[1];
            }
            ox = vx;
            oy = vy;
        };
        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        sim(5.0f, 0.0f, 150, ax, ay);
        sim(5.0f, 0.0f, 150, bx, by);
        ok &= check(approx(ax, bx) && approx(ay, by),
                    "shared hot policy is context-free: identical multi-tick "
                    "results in two contexts", report);
        ok &= check(!approx(ax, 5.0f) || !approx(ay, 0.0f),
                    "shared hot policy integrates velocity over the sequence",
                    report);
    }

    // Ground-move shared implementation (friction + acceleration).
    {
        GameGroundMoveV1 g{};
        g.velocity[0] = 10.0f;
        g.velocity[1] = 0.0f;
        g.wishDir[0] = 1.0f;
        g.wishDir[1] = 0.0f;
        g.wishSpeed = 20.0f;
        g.groundAcceleration = 20.0f;
        g.frictionAmount = 3.25f;
        g.stopspeed = 0.0f;
        g.dt = 1.0f / 60.0f;
        g.hasInput = 1u;
        g.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_GROUND_MOVE, &g,
                                              sizeof(g), 0, 0, 0);
        // friction: 10 * 3.25/60 = 0.54167 -> 9.45833; accel: min(20*20/60,
        // 20-9.45833)=6.66667 -> 16.125.
        ok &= check(g.handled == 1u && approx(g.outVelocity[0], 16.125f, 0.01f) &&
                        approx(g.outVelocity[1], 0.0f),
                    "hot ground-move owns the friction + acceleration function",
                    report);

        GameGroundMoveV1 g2{};
        g2.velocity[0] = 3.0f;
        g2.velocity[1] = 4.0f;
        g2.wishDir[0] = 0.0f;
        g2.wishDir[1] = 0.0f;
        g2.wishSpeed = 20.0f;
        g2.groundAcceleration = 20.0f;
        g2.frictionAmount = 1.0f;
        g2.stopspeed = 0.0f;
        g2.dt = 1.0f / 60.0f;
        g2.hasInput = 0u;
        g2.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_GROUND_MOVE, &g2,
                                              sizeof(g2), 0, 0, 0);
        // No input: friction only. speed=5, drop=5/60=0.08333 -> scale 0.98333.
        ok &= check(g2.handled == 1u && approx(g2.outVelocity[0], 2.95f, 0.01f) &&
                        approx(g2.outVelocity[1], 3.93333f, 0.01f),
                    "hot ground-move owns the friction-only function", report);
    }

    // Gravity shared implementation.
    {
        GameGravityV1 g{};
        g.velocityZ = 0.0f;
        g.gravityZ = -40.0f;
        g.maximumFallSpeed = 175.0f;
        g.dt = 1.0f / 60.0f;
        g.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_GRAVITY, &g,
                                              sizeof(g), 0, 0, 0);
        ok &= check(g.handled == 1u && approx(g.outVelocityZ, -40.0f / 60.0f, 1e-3f),
                    "hot gravity owns the vertical velocity change", report);

        GameGravityV1 g2{};
        g2.velocityZ = -200.0f;
        g2.gravityZ = -40.0f;
        g2.maximumFallSpeed = 175.0f;
        g2.dt = 1.0f / 60.0f;
        g2.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_GRAVITY, &g2,
                                              sizeof(g2), 0, 0, 0);
        ok &= check(g2.handled == 1u && approx(g2.outVelocityZ, -175.0f, 1e-3f),
                    "hot gravity owns the terminal-speed clamp", report);
    }

    // Speed / wish-speed derivation shared implementation.
    {
        GameSpeedPolicyV1 sp{};
        sp.baseMaxSpeed = 20.0f;
        sp.baseFallbackSpeed = 20.0f;
        sp.sizeScale = 4.0f;
        sp.sizeExponent = 0.5f;
        sp.speedLimit = 0.0f;
        sp.airMaxWishspeed = 0.0f;
        sp.rawWishSpeed = 40.0f;
        sp.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_SPEED_POLICY, &sp,
                                              sizeof(sp), 0, 0, 0);
        // sizeScale 4 ^ 0.5 = 2 -> maxSpeed 40.
        ok &= check(sp.handled == 1u && approx(sp.outMaxSpeed, 40.0f, 1e-3f),
                    "hot speed policy derives size-scaled max speed", report);

        GameSpeedPolicyV1 sp2{};
        sp2.baseMaxSpeed = 20.0f;
        sp2.baseFallbackSpeed = 20.0f;
        sp2.sizeScale = 1.0f;
        sp2.sizeExponent = 0.5f;
        sp2.airMaxWishspeed = 10.0f;
        sp2.rawWishSpeed = 20.0f;
        sp2.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_SPEED_POLICY, &sp2,
                                              sizeof(sp2), 0, 0, 0);
        ok &= check(sp2.handled == 1u && approx(sp2.outWishspd, 10.0f, 1e-3f),
                    "hot speed policy derives the air wish-speed cap", report);
    }

    // Jump shared implementation.
    {
        auto runJump = [](GameJumpPolicyV1 j) {
            LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_JUMP, &j,
                                                  sizeof(j), 0, 0, 0);
            return j;
        };
        GameJumpPolicyV1 base{};
        base.jumpSpeed = 15.0f;
        base.dt = 1.0f / 60.0f;
        base.jumpBufferSeconds = 0.1f;
        base.maximumAirJumps = 1u;

        // Grounded jump.
        GameJumpPolicyV1 g = runJump([&] {
            GameJumpPolicyV1 j = base;
            j.grounded = 1u;
            j.jumpPressed = 1u;
            j.jumpHeld = 1u;
            return j;
        }());
        ok &= check(g.handled == 1u && g.outDidGroundJump == 1u &&
                        approx(g.outVelocityZ, 15.0f) && g.outGrounded == 0u &&
                        g.airJumpsLeft == 1,
                    "hot jump owns the grounded jump", report);

        // Air jump (armed, one available).
        GameJumpPolicyV1 a = runJump([&] {
            GameJumpPolicyV1 j = base;
            j.grounded = 0u;
            j.jumpPressed = 1u;
            j.jumpHeld = 1u;
            j.airJumpsLeft = 1;
            j.airJumpArmed = 1u;
            return j;
        }());
        ok &= check(a.handled == 1u && a.outDidAirJump == 1u &&
                        approx(a.outVelocityZ, 15.0f) && a.airJumpsLeft == 0,
                    "hot jump owns the air jump and decrements count", report);

        // Second air jump denied when none left.
        GameJumpPolicyV1 d = runJump([&] {
            GameJumpPolicyV1 j = base;
            j.grounded = 0u;
            j.jumpPressed = 1u;
            j.jumpHeld = 1u;
            j.airJumpsLeft = 0;
            j.airJumpArmed = 1u;
            return j;
        }());
        ok &= check(d.handled == 1u && d.outDidAirJump == 0u &&
                        approx(d.outVelocityZ, 0.0f),
                    "hot jump denies a second air jump", report);

        // Deterministic repeat.
        GameJumpPolicyV1 r1 = runJump([&] {
            GameJumpPolicyV1 j = base;
            j.grounded = 1u;
            j.jumpPressed = 1u;
            j.jumpHeld = 1u;
            return j;
        }());
        GameJumpPolicyV1 r2 = runJump([&] {
            GameJumpPolicyV1 j = base;
            j.grounded = 1u;
            j.jumpPressed = 1u;
            j.jumpHeld = 1u;
            return j;
        }());
        ok &= check(r1.outVelocityZ == r2.outVelocityZ &&
                        r1.outDidGroundJump == r2.outDidGroundJump &&
                        r1.airJumpsLeft == r2.airJumpsLeft,
                    "hot jump is deterministic for identical input", report);
    }

    // Dash / down-dash shared implementation.
    {
        auto runDash = [](GameDashPolicyV1 d) {
            LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_DASH, &d,
                                                  sizeof(d), 0, 0, 0);
            return d;
        };
        GameDashPolicyV1 base{};
        base.groundDashImpulse = 20.0f;
        base.airDashImpulse = 30.0f;
        base.downDashVerticalSpeed = -50.0f;
        base.dashEnabled = 1u;
        base.downDashEnabled = 1u;

        GameDashPolicyV1 g = runDash([&] {
            GameDashPolicyV1 d = base;
            d.moveAxes[0] = 1.0f;
            d.grounded = 1u;
            d.dashPressed = 1u;
            d.dashAvailable = 1u;
            return d;
        }());
        ok &= check(g.handled == 1u && g.outDidDash == 1u &&
                        approx(g.outVelocity[0], 20.0f) && g.outDashAvailable == 0u,
                    "hot dash owns the grounded dash", report);

        GameDashPolicyV1 a = runDash([&] {
            GameDashPolicyV1 d = base;
            d.moveAxes[0] = 1.0f;
            d.grounded = 0u;
            d.dashPressed = 1u;
            d.dashAvailable = 1u;
            return d;
        }());
        ok &= check(a.handled == 1u && a.outDidDash == 1u &&
                        approx(a.outVelocity[0], 30.0f),
                    "hot dash owns the airborne dash", report);

        GameDashPolicyV1 n = runDash([&] {
            GameDashPolicyV1 d = base;
            d.moveAxes[0] = 1.0f;
            d.grounded = 1u;
            d.dashPressed = 1u;
            d.dashAvailable = 0u;
            return d;
        }());
        ok &= check(n.handled == 1u && n.outDidDash == 0u &&
                        approx(n.outVelocity[0], 0.0f),
                    "hot dash denies when unavailable", report);

        GameDashPolicyV1 c = runDash([&] {
            GameDashPolicyV1 d = base;
            d.cameraForward[0] = 0.0f;
            d.cameraForward[1] = 1.0f;
            d.grounded = 1u;
            d.dashPressed = 1u;
            d.dashAvailable = 1u;
            return d;
        }());
        ok &= check(c.handled == 1u && c.outDidDash == 1u &&
                        approx(c.outVelocity[1], 20.0f) && approx(c.outVelocity[0], 0.0f),
                    "hot dash uses camera fallback direction", report);

        GameDashPolicyV1 dd = runDash([&] {
            GameDashPolicyV1 d = base;
            d.downDashPressed = 1u;
            d.downDashAvailable = 1u;
            return d;
        }());
        ok &= check(dd.handled == 1u && dd.outDidDownDash == 1u &&
                        approx(dd.outVelocity[2], -50.0f) &&
                        dd.outDownDashAvailable == 0u,
                    "hot dash owns the down-dash vertical response", report);
    }

    // Freeze shared implementation.
    {
        auto runFreeze = [](GameFreezePolicyV1 f) {
            LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_FREEZE, &f,
                                                  sizeof(f), 0, 0, 0);
            return f;
        };
        GameFreezePolicyV1 base{};
        base.velocity[0] = 5.0f;
        base.velocity[1] = 2.0f;
        base.velocity[2] = -3.0f;
        base.dt = 1.0f / 60.0f;
        base.freezeEnabled = 1u;
        base.freezeAvailable = 1u;

        GameFreezePolicyV1 e = runFreeze([&] {
            GameFreezePolicyV1 f = base;
            f.freezePressed = 1u;
            f.freezeHeld = 1u;
            return f;
        }());
        ok &= check(e.handled == 1u && e.outDidFreeze == 1u &&
                        e.outFreezeActive == 1u && e.outFreezeAvailable == 0u &&
                        approx(e.outVelocity[0], 0.0f) &&
                        approx(e.outVelocity[2], 0.0f),
                    "hot freeze owns activation and velocity suppression", report);

        GameFreezePolicyV1 h = runFreeze([&] {
            GameFreezePolicyV1 f = base;
            f.freezeActive = 1u;
            f.freezeAvailable = 0u;
            f.freezeHeld = 1u;
            f.freezeHeldPreviously = 1u;
            return f;
        }());
        ok &= check(h.handled == 1u && h.outDidFreeze == 0u &&
                        h.outFreezeActive == 1u && approx(h.outVelocity[0], 0.0f),
                    "hot freeze keeps the actor frozen while held", report);

        GameFreezePolicyV1 r = runFreeze([&] {
            GameFreezePolicyV1 f = base;
            f.freezeActive = 1u;
            f.freezeHeld = 0u;
            f.freezeHeldPreviously = 1u;
            return f;
        }());
        ok &= check(r.handled == 1u && r.outFreezeEnded == 1u &&
                        r.outFreezeActive == 0u,
                    "hot freeze owns release/exit", report);

        GameFreezePolicyV1 d = runFreeze([&] {
            GameFreezePolicyV1 f = base;
            f.freezeActive = 1u;
            f.freezeHeld = 1u;
            f.freezeHeldPreviously = 1u;
            f.durationSeconds = 0.5f;
            f.freezeTimerSeconds = 0.5f;
            return f;
        }());
        ok &= check(d.handled == 1u && approx(d.outFreezeTimerSeconds, 0.5f),
                    "hot freeze clamps timer to the max duration", report);
    }

    // Post-step speed clamp shared implementation.
    {
        GameSpeedClampV1 c{};
        c.velocity[0] = 10.0f;
        c.velocity[1] = 0.0f;
        c.speedLimit = 5.0f;
        c.enabled = 1u;
        c.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_SPEED_CLAMP, &c,
                                              sizeof(c), 0, 0, 0);
        ok &= check(c.handled == 1u && approx(c.outVelocity[0], 5.0f) &&
                        approx(c.outVelocity[1], 0.0f),
                    "hot speed clamp owns horizontal max-speed enforcement",
                    report);

        GameSpeedClampV1 c2{};
        c2.velocity[0] = 10.0f;
        c2.velocity[1] = 0.0f;
        c2.speedLimit = 5.0f;
        c2.enabled = 0u;
        c2.handled = 0;
        LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MOVEMENT_SPEED_CLAMP, &c2,
                                              sizeof(c2), 0, 0, 0);
        ok &= check(c2.handled == 1u && approx(c2.outVelocity[0], 10.0f),
                    "hot speed clamp preserves velocity when disabled", report);
    }

    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
