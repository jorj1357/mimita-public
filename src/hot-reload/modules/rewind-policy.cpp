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
#include "hot-reload/hot-lagcomp-policy.h"

namespace {

void MIMITA_GAME_CALL onRewind(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GameRewindPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;

    // ONE shared lag-compensation policy body (also used by history.select), so
    // a single edit to hot-lagcomp-policy.h changes both stages live.
    p->targetTick = MimitaLagComp::selectTargetTick(
        *p, &p->handled, &p->allow, &p->clamped, &p->reject);
    p->interpolate = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_rewindRegistration{
    {GAME_EVENT_REWIND, 0, 0, onRewind, "net.rewind"}};

#endif
