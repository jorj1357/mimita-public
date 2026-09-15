// 09 15 2026
/* purpose
* movement.speed-clamp: the ONE hot post-acceleration horizontal speed clamp /
* preservation policy. Cold fills inputs; the cold physics layer may report
* collision results, but the clamp/preservation rule is hot.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

#include <cmath>

namespace MimitaHotMovement {

void speedClamp(GameSpeedClampV1& io)
{
    float vx = io.velocity[0];
    float vy = io.velocity[1];
    if (io.enabled != 0u && io.speedLimit > 0.0f) {
        const float speed = std::sqrt(vx * vx + vy * vy);
        if (speed > io.speedLimit && speed > 0.0f) {
            const float scale = io.speedLimit / speed;
            vx *= scale;
            vy *= scale;
        }
    }
    io.outVelocity[0] = vx;
    io.outVelocity[1] = vy;
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onSpeedClamp(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameSpeedClampV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::speedClamp(*p);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_speedClampRegistration{
    {GAME_EVENT_MOVEMENT_SPEED_CLAMP, 0, 0, onSpeedClamp,
     "movement.speed-clamp"}};

#endif
