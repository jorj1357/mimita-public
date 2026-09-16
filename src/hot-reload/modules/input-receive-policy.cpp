// 09 16 2026
/* purpose
* input.receive-policy: the hot decision for whether a received InputPacket is
* processed. Default: accept. Exposes receive facts so a dropped input stream is
* diagnosable live.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onInputReceivePolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<InputReceivePolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1u;
    p->accept = 1u;
    // Spec phase 1: server adopts the validated client movement as
    // authoritative so it tracks the player exactly. Flip to 0 to return to
    // server-side simulation (movement.actors).
    p->adoptState = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_inputReceivePolicyRegistration{
    {GAME_EVENT_INPUT_RECEIVE_POLICY, 0, 0, onInputReceivePolicy,
     "input.receive-policy"}};

#endif
