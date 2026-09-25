// 09 24 2026
/* purpose
* Hot NPC movement/facing intent provider (migration Phase 5a). Registers the
* generic `npc.intent` capability and owns the facing-mode timer, desired
* facing, turn-speed limiting, and the final MovementIntent/AimIntent. Cold
* fills the raw navigation/action facts and applies the returned intent.
* Editing this file changes NPC facing/aim behavior live after the next DLL
* activation; the shared implementation keeps the fallback behavior identical.
* Does NOT own navigation, combat, the loop, transport, or damage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-npc-intent.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

// Hot formula layer. Editing this multiplier changes NPC facing responsiveness
// live after the next hot DLL activation; the raw turn speed comes from config.
constexpr float kTurnSpeedMultiplier = 1.0f;

void MIMITA_GAME_CALL npcIntent(void* /*host*/, NpcIntentPolicyV1* r)
{
    if (!r)
        return;
    if (r->structSize == 0)
        r->structSize = sizeof(NpcIntentPolicyV1);
    const float originalTurnSpeed = r->turnSpeed;
    r->turnSpeed = originalTurnSpeed * kTurnSpeedMultiplier;
    MimitaNet::HotNpcIntentImpl::evaluate(*r);
    r->turnSpeed = originalTurnSpeed;  // restore the in/out field for the caller
}

const GameCapabilityDescriptorV1 kNpcIntentProvider{
    GAME_CAP_NPC_INTENT, GAME_SIG_NPC_INTENT, 0,
    reinterpret_cast<void*>(&npcIntent), "npc.intent"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcIntentProvider{
    kNpcIntentProvider};

#endif // MIMITA_GAME_DLL
