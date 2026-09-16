// 09 16 2026
/* purpose
* input.send-policy: the hot decision for whether the client sends an
* InputPacket this frame. Exposes the gate facts so a stuck input stream is
* diagnosable and fixable live. Default: send whenever input is present and due,
* even if the generation bootstrap gate is closed (that gate previously blocked
* ordinary movement input).
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onInputSendPolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<InputSendPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1u;
    const std::uint32_t need =
        GAME_INPUT_GATE_CONNECTED | GAME_INPUT_GATE_LOCAL_PLAYER |
        GAME_INPUT_GATE_INPUT_PRESENT | GAME_INPUT_GATE_DUE;
    p->send = ((p->gateFlags & need) == need) ? 1u : 0u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_inputSendPolicyRegistration{
    {GAME_EVENT_INPUT_SEND_POLICY, 0, 0, onInputSendPolicy, "input.send-policy"}};

#endif
