// 09 15 2026
/* purpose
* movement.speed-policy: the ONE hot speed / wish-speed derivation (size-scale
* factor, effective max speed with a fixed speed limit, and the air wish-speed
* projection cap). Called by the cold/server movement hook (via the generic
* event) and by local prediction (`movement.main`). Context-free.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

#include <cmath>

namespace MimitaHotMovement {

void speedPolicy(const GameSpeedPolicyV1& in, float& outMaxSpeed, float& outWishspd)
{
    const float scale = in.sizeScale > 0.001f ? in.sizeScale : 0.001f;
    const float factor = std::pow(scale, in.sizeExponent);
    const float base = in.baseMaxSpeed > 0.0f ? in.baseMaxSpeed : in.baseFallbackSpeed;
    float maxSpeed = base * factor;
    if (in.speedLimitFixed && in.speedLimit > 0.0f && in.speedLimit < maxSpeed)
        maxSpeed = in.speedLimit;
    outMaxSpeed = maxSpeed;

    float wishspd = maxSpeed;
    if (in.airMaxWishspeed > 0.0f) {
        const float cap = in.airMaxWishspeed * factor;
        wishspd = cap < in.rawWishSpeed ? cap : in.rawWishSpeed;
    }
    outWishspd = wishspd;
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onSpeedPolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameSpeedPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::speedPolicy(*p, p->outMaxSpeed, p->outWishspd);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_speedPolicyRegistration{
    {GAME_EVENT_MOVEMENT_SPEED_POLICY, 0, 0, onSpeedPolicy,
     "movement.speed-policy"}};

#endif
