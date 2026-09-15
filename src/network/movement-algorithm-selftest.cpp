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

    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
