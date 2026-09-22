// 09 15 2026
/* purpose
* movement.air-accelerate: the ONE hot air-acceleration implementation. It owns
* the actual math (project velocity onto wishdir, derive remaining headroom,
* apply a diminishing-returns gain, modify the velocity vector) — not just
* constants. The same function is called by the cold/server movement hook (via
* the generic event) AND by local prediction (`movement.main`), and works for any
* generic runtime actor. Context-free: plain numbers only, no ServerPlayer/Npc/
* renderer/packet state.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

#include <cmath>

namespace MimitaHotMovement {

// The single air-acceleration algorithm. Context-free.
void airAccelerate(const GameAirAccelerateV1& in, float outVelocity[2])
{
    if (in.movementModel == 1u) {
        // afad20a / GoldSrc air rule: WASD supplies a wish direction, but cannot
        // launch an actor from rest. Speed is gained when the wish direction
        // pivots against existing horizontal velocity (mouse-steered air-strafe).
        // Acceleration is linear and projection-limited, scaled by the air speed
        // gain multiplier.
        const float horizontalSpeed = std::sqrt(
            in.velocity[0] * in.velocity[0] +
            in.velocity[1] * in.velocity[1]);
        if (horizontalSpeed <= 0.1f) {
            outVelocity[0] = in.velocity[0];
            outVelocity[1] = in.velocity[1];
            return;
        }
        const float projected =
            in.velocity[0] * in.wishDir[0] + in.velocity[1] * in.wishDir[1];
        const float addSpeed = in.wishspd - projected;
        if (addSpeed <= 0.0f) {
            outVelocity[0] = in.velocity[0];
            outVelocity[1] = in.velocity[1];
            return;
        }
        float accelSpeed =
            in.airAcceleration * in.wishSpeed * in.dt * in.surfaceFriction *
            in.airSpeedGainMultiplier;
        if (accelSpeed > addSpeed)
            accelSpeed = addSpeed;
        outVelocity[0] = in.velocity[0] + in.wishDir[0] * accelSpeed;
        outVelocity[1] = in.velocity[1] + in.wishDir[1] * accelSpeed;
        return;
    }

    // 1. Project the current velocity onto the wish direction.
    float projected = in.velocity[0] * in.wishDir[0] + in.velocity[1] * in.wishDir[1];
    if (projected < 0.0f)
        projected = 0.0f;

    // 2. Remaining headroom along the wish direction.
    const float addSpeed = in.wishspd - projected;
    if (addSpeed <= 0.0f) {
        outVelocity[0] = in.velocity[0];
        outVelocity[1] = in.velocity[1];
        return;
    }

    // 3. Diminishing-returns gain: the closer the projected speed is to the
    //    wish-speed cap, the smaller the applied acceleration (algorithm, not a
    //    constant change).
    const float base = in.airAcceleration * in.wishSpeed * in.dt * in.surfaceFriction;
    const float headroom = in.wishspd > 0.0f ? addSpeed / in.wishspd : 0.0f;
    float gain = base * headroom * in.airSpeedGainMultiplier;
    if (gain > addSpeed)
        gain = addSpeed;
    if (gain < 0.0f)
        gain = 0.0f;

    // 4. Modify the velocity vector.
    outVelocity[0] = in.velocity[0] + in.wishDir[0] * gain;
    outVelocity[1] = in.velocity[1] + in.wishDir[1] * gain;
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onAirAccelerate(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameAirAccelerateV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::airAccelerate(*p, p->outVelocity);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_airAccelerateRegistration{
    {GAME_EVENT_MOVEMENT_AIR_ACCELERATE, 0, 0, onAirAccelerate,
     "movement.air-accelerate"}};

#endif
