// 09 15 2026
/* purpose
* movement.freeze: the ONE hot freeze policy (activation eligibility, duration,
* velocity suppression while frozen, held/released transitions, exit). Called by
* the cold/server movement hook (via the generic event) and by local prediction
* (`movement.main`). Context-free: plain numbers only.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

namespace MimitaHotMovement {

void freezePolicy(GameFreezePolicyV1& io)
{
    io.outDidFreeze = 0u;
    io.outFreezeStarted = 0u;
    io.outFreezeEnded = 0u;
    io.outFreezeHeldPreviously = io.freezeHeld;

    // Freeze disabled: release any active freeze so the actor is not stuck.
    if (io.freezeEnabled == 0u) {
        if (io.freezeActive != 0u)
            io.outFreezeEnded = 1u;
        io.outFreezeActive = 0u;
        io.outFreezeAvailable = io.freezeAvailable;
        io.outFreezeTimerSeconds = io.freezeTimerSeconds;
        io.outVelocity[0] = io.velocity[0];
        io.outVelocity[1] = io.velocity[1];
        io.outVelocity[2] = io.velocity[2];
        return;
    }

    const bool pressed =
        io.freezePressed != 0u || (io.freezeHeld != 0u && io.freezeHeldPreviously == 0u);
    const bool released = io.freezeHeld == 0u && io.freezeHeldPreviously != 0u;

    float vx = io.velocity[0];
    float vy = io.velocity[1];
    float vz = io.velocity[2];
    std::uint32_t active = io.freezeActive;
    std::uint32_t available = io.freezeAvailable;
    float timer = io.freezeTimerSeconds;

    if (pressed && available != 0u) {
        active = 1u;
        available = 0u;
        timer = 0.0f;
        io.outDidFreeze = 1u;
        io.outFreezeStarted = 1u;
    }

    if (released && active != 0u) {
        active = 0u;
        io.outFreezeEnded = 1u;
    }

    // v2.0.6 freeze: while active, all-axis velocity is scaled by a piecewise
    // quadratic multiplier over the freeze duration. It is NOT hard-zeroed and
    // does not require the key to stay held; release ends it above. Gravity is
    // applied before freeze by the caller, so the scaled gravity survives.
    // The curve is duration-normalised: first half x*x*0.2, second half
    // 0.2 + x*x*0.8, matching v2.0.6 physics-freeze.cpp freezeMaxTime 5.0.
    if (active != 0u) {
        timer += io.dt;
        if (io.durationSeconds > 0.0f && timer > io.durationSeconds)
            timer = io.durationSeconds;

        const float half = (io.durationSeconds > 0.0f) ? io.durationSeconds * 0.5f : 2.5f;
        float mult;
        if (half > 0.0f && timer < half) {
            const float x = timer / half;
            mult = x * x * 0.2f;
        } else {
            const float x = (half > 0.0f) ? (timer - half) / half : 1.0f;
            mult = 0.2f + x * x * 0.8f;
        }
        vx *= mult;
        vy *= mult;
        vz *= mult;
    }

    io.outVelocity[0] = vx;
    io.outVelocity[1] = vy;
    io.outVelocity[2] = vz;
    io.outFreezeActive = active;
    io.outFreezeAvailable = available;
    io.outFreezeTimerSeconds = timer;
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onFreeze(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameFreezePolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::freezePolicy(*p);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_freezeRegistration{
    {GAME_EVENT_MOVEMENT_FREEZE, 0, 0, onFreeze, "movement.freeze"}};

#endif
