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
    bool startedThisTick = false;

    if (pressed && available != 0u) {
        // afad20a: activation hard-stops stored velocity (all axes).
        vx = 0.0f;
        vy = 0.0f;
        vz = 0.0f;
        active = 1u;
        available = 0u;
        timer = 0.0f;
        startedThisTick = true;
        io.outDidFreeze = 1u;
        io.outFreezeStarted = 1u;
    }

    if (released && active != 0u) {
        active = 0u;
        io.outFreezeEnded = 1u;
    }

    // afad20a freeze: while held, stored velocity is scaled by the pass-through
    // curve pow(timer/duration, 4). At t=0 movement is fully suppressed; by the
    // end of the duration it passes through completely. Release ends it above.
    if (active != 0u && io.freezeHeld != 0u && !startedThisTick) {
        timer += io.dt;
        if (io.durationSeconds > 0.0f && timer > io.durationSeconds)
            timer = io.durationSeconds;

        float passThrough = 1.0f;
        if (io.durationSeconds > 0.0f) {
            float u = timer / io.durationSeconds;
            if (u < 0.0f) u = 0.0f;
            if (u > 1.0f) u = 1.0f;
            passThrough = u * u * u * u;
        }
        vx *= passThrough;
        vy *= passThrough;
        vz *= passThrough;
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
