// 09 15 2026
/* purpose
* movement.gravity: the ONE hot gravity implementation (vertical velocity change
* + terminal-speed clamp). Called by the cold/server movement hook (via the
* generic event) and by local prediction (`movement.main`). Context-free.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

namespace MimitaHotMovement {

void gravity(const GameGravityV1& in, float& outVelocityZ)
{
    float nextZ = in.velocityZ + in.gravityZ * in.dt;
    const float floorZ = -in.maximumFallSpeed;
    outVelocityZ = nextZ > floorZ ? nextZ : floorZ;
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onGravity(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameGravityV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::gravity(*p, p->outVelocityZ);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_gravityRegistration{
    {GAME_EVENT_MOVEMENT_GRAVITY, 0, 0, onGravity, "movement.gravity"}};

#endif
