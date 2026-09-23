// 09 23 2026
/* purpose
* history.select: the pose-selection half of the ONE hot lag-compensation policy
* (see hot-lagcomp-policy.h). Given the raw bracketing samples from the
* EXE-owned history store it produces the interpolated / nearest / clamped pose.
* The target-tick half (net.rewind) uses the same shared policy body, so a single
* edit to hot-lagcomp-policy.h changes both stages live.
* Context-free: plain samples only; no Player/Npc/pointer types.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-history.h"
#include "hot-reload/hot-lagcomp-policy.h"

namespace {

void MIMITA_GAME_CALL onHistorySelect(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<MimitaNet::GameHistorySelectV1*>(event->payload)
                    : nullptr;
    if (!p)
        return;
    MimitaLagComp::selectPose(p);
}

} // namespace

const MimitaHotPackage::EventRegistrar s_historySelectRegistration{
    {MimitaNet::GAME_EVENT_HISTORY_SELECT, 0, 0, onHistorySelect,
     "history.select"}};

#endif
