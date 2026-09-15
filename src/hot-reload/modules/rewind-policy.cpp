// 09 15 2026
/* purpose
* net.rewind: the ONE hot rewind / lag-compensation policy. Given command/history
* timing facts it owns the rewind target tick (latency + interpolation-delay
* compensation), the max-rewind clamp, and the interpolate decision. The cold
* history mechanism still stores samples and executes the historical lookup +
* collision query with the returned target. Generation mismatch is conservative
* (reject). Context-free: plain numbers only.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-rewind.h"

#include <cmath>

namespace {

constexpr double kSimHz = 60.0;

void MIMITA_GAME_CALL onRewind(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameRewindPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;

    p->handled = 0u;
    p->allow = 0u;
    p->clamped = 0u;
    p->reject = 0u;
    p->interpolate = 1u;
    p->targetTick = p->commandTick;

    // Generation mismatch: never evaluate across incompatible behavior
    // generations; reject conservatively.
    if (p->attackerGeneration != p->currentGeneration ||
        p->targetGeneration != p->currentGeneration) {
        p->reject = 1u;
        p->handled = 1u;
        return;
    }

    const int64_t interpTicks = (int64_t)std::llround(
        (double)p->interpolationDelaySeconds * kSimHz);
    const int64_t compTicks = (int64_t)std::llround(
        (double)p->compensationSeconds * kSimHz);
    const int64_t pingTicks = (int64_t)std::llround(
        (double)p->measuredLatencySeconds * kSimHz);

    int64_t rewind;
    if (p->commandTick != 0u) {
        rewind = (int64_t)p->commandTick - interpTicks - compTicks;
    } else if (p->acceptedClientTick != 0u && p->acceptedServerTick != 0u) {
        rewind = (int64_t)p->acceptedServerTick - interpTicks - compTicks - pingTicks;
    } else {
        rewind = (int64_t)p->currentTick - interpTicks - compTicks - pingTicks;
    }

    if (p->maxRewindTicks > 0u) {
        const int64_t floor = (int64_t)p->currentTick - (int64_t)p->maxRewindTicks;
        if (rewind < floor) {
            rewind = floor;
            p->clamped = 1u;
        }
    }
    if (rewind < 0)
        rewind = 0;
    if (rewind > (int64_t)p->currentTick)
        rewind = (int64_t)p->currentTick;

    p->targetTick = (std::uint32_t)rewind;
    p->allow = 1u;
    p->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_rewindRegistration{
    {GAME_EVENT_REWIND, 0, 0, onRewind, "net.rewind"}};

#endif
