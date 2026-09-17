// 09 16 2026
/* purpose
* net.reliable-policy: the hot decision for reliable-event delivery. It owns
* whether an event is retried and whether an expired/exhausted event may mark
* the connection unhealthy. Retry timing/ttl/attempts stay NetworkingConfig.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onNetReliablePolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<NetReliablePolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1u;
    p->reliable = 1u;
    p->retry = 1u;
    // Conservative default: an expired/exhausted reliable event never drops the
    // connection (the connection-health timer owns disconnects). Set 0 live to
    // let reliable failures mark the connection unhealthy again.
    p->keepConnection = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_netReliablePolicyRegistration{
    {GAME_EVENT_NET_RELIABLE_POLICY, 0, 0, onNetReliablePolicy,
     "net.reliable-policy"}};

#endif
