// 09 15 2026
/* purpose
* movement.dash: the ONE hot dash / down-dash policy (activation, direction
// choice, ground/air impulse composition, availability transitions, vertical
// response). Called by the cold/server movement hook (via the generic event) and
* by local prediction (`movement.main`). Context-free: plain numbers only.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

#include <cmath>

namespace MimitaHotMovement {

// v2.0.6 dash quality: impulse scales down the longer the player has been
// airborne with movement held. 0..1 ticks = perfect (1.0).
static float v206DashQualityMultiplier(std::uint32_t ticks)
{
    if (ticks <= 1u) return 1.00f;
    if (ticks == 2u) return 0.85f;
    if (ticks == 3u) return 0.70f;
    if (ticks == 4u) return 0.55f;
    return 0.40f;
}

void dashPolicy(GameDashPolicyV1& io)
{
    float vx = io.velocity[0];
    float vy = io.velocity[1];
    float vz = io.velocity[2];

    io.outDashAvailable = io.dashAvailable;
    io.outDownDashAvailable = io.downDashAvailable;
    io.outDidDash = 0u;
    io.outDidDownDash = 0u;
    io.outUsedMoveInput = 0u;

    if (io.dashPressed != 0u && io.dashEnabled != 0u && io.dashAvailable != 0u) {
        float dx = io.moveAxes[0];
        float dy = io.moveAxes[1];
        const float len = std::sqrt(dx * dx + dy * dy);
        bool usedMoveInput = false;
        if (len > 1e-4f) {
            dx /= len;
            dy /= len;
            usedMoveInput = true;
        } else {
            dx = io.cameraForward[0];
            dy = io.cameraForward[1];
            const float l2 = std::sqrt(dx * dx + dy * dy);
            if (l2 > 1e-4f) {
                dx /= l2;
                dy /= l2;
            } else {
                dx = 0.0f;
                dy = 0.0f;
            }
        }
        if (dx != 0.0f || dy != 0.0f) {
            // v2.0.6: ground dash uses the full ground impulse; air dash scales
            // by dash quality (airborne movement ticks). Additive either way.
            const float impulse =
                io.grounded != 0u
                    ? io.groundDashImpulse
                    : io.airDashImpulse * v206DashQualityMultiplier(io.dashMovementTicks);
            vx += dx * impulse;
            vy += dy * impulse;
            io.outDashAvailable = 0u;
            io.outDidDash = 1u;
            io.outUsedMoveInput = usedMoveInput ? 1u : 0u;
        }
    }

    if (io.downDashPressed != 0u && io.downDashEnabled != 0u &&
        io.downDashAvailable != 0u) {
        // v2.0.6 down-dash is additive: it preserves existing vertical momentum.
        vz += io.downDashVerticalSpeed;
        io.outDownDashAvailable = 0u;
        io.outDidDownDash = 1u;
    }

    io.outVelocity[0] = vx;
    io.outVelocity[1] = vy;
    io.outVelocity[2] = vz;
}

} // namespace MimitaHotMovement

namespace {

void MIMITA_GAME_CALL onDash(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameDashPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    MimitaHotMovement::dashPolicy(*p);
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_dashRegistration{
    {GAME_EVENT_MOVEMENT_DASH, 0, 0, onDash, "movement.dash"}};

#endif
