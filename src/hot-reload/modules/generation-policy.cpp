// 09 16 2026
/* purpose
* net.generation-policy: the hot decision when the client and server hot
* generations differ. Default: allow world participation so a mismatch cannot
* silently wedge the session. Editable live: set allowWorld = 0 to block until
* both peers converge.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onGenerationPolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<GenerationPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1u;
    // Keep the session playable across a transient mismatch. The movement code
    // is ABI-stable (reserved slots), so the client can still drive the server.
    p->allowWorld = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_generationPolicyRegistration{
    {GAME_EVENT_GENERATION_POLICY, 0, 0, onGenerationPolicy,
     "net.generation-policy"}};

#endif
