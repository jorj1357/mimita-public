// 09 16 2026
/* purpose
* net.send-policy: the hot decision for whether an outbound server snapshot is
* sent to a viewer this tick. The wire format, chunking, and transport stay
* kernel-owned; only the send decision is hot.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onNetSendPolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<NetSendPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1u;
    p->send = 1u;   // set 0 live to suppress a viewer's snapshot this tick
}

} // namespace

const MimitaHotPackage::EventRegistrar s_netSendPolicyRegistration{
    {GAME_EVENT_NET_SEND_POLICY, 0, 0, onNetSendPolicy, "net.send-policy"}};

#endif
