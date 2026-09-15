// 09 15 2026
/* purpose
* movement.ground-move: the ONE hot ground-movement implementation (PM_Friction
* + accelerate along wishdir). The same function is called by the cold/server
* movement hook (via the generic event) AND by local prediction
* (`movement.main`). Context-free: plain numbers only.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

#include <cmath>

namespace MimitaHotMovement {

void groundMove(const GameGroundMoveV1& in, float outVelocity[2])
{
    float vx = in.velocity[0];
    float vy = in.velocity[1];

    // PM_Friction: horizontal speed drop every grounded tick.
    const float speed = std::sqrt(vx * vx + vy * vy);
    if (speed > 0.1f) {
        const float control = speed > in.stopspeed ? speed : in.stopspeed;
        const float drop = control * in.frictionAmount * in.dt;
        float newSpeed = speed - drop;
        if (newSpeed < 0.0f)
            newSpeed = 0.0f;
        if (newSpeed != speed) {
            const float scale = newSpeed / speed;
            vx *= scale;
            vy *= scale;
        }
    } else {
        vx = 0.0f;
        vy = 0.0f;
    }

    // PM_Accelerate along wishdir (wishspeed capped to maxspeed).
    if (in.hasInput) {
        const float currentSpeed = vx * in.wishDir[0] + vy * in.wishDir[1];
        const float addSpeed = in.wishSpeed - currentSpeed;
        if (addSpeed > 0.0f) {
            float accelSpeed = in.groundAcceleration * in.wishSpeed * in.dt;
            if (accelSpeed > addSpeed)
                accelSpeed = addSpeed;
            vx += in.wishDir[0] * accelSpeed;
            vy += in.wishDir[1] * accelSpeed;
        }
    }

    outVelocity[0] = vx;
    outVelocity[1] = vy;
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onGroundMove(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameGroundMoveV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::groundMove(*p, p->outVelocity);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_groundMoveRegistration{
    {GAME_EVENT_MOVEMENT_GROUND_MOVE, 0, 0, onGroundMove, "movement.ground-move"}};

#endif
