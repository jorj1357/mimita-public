// 09 24 2026
/* purpose
* Hot NPC target-selection provider (migration Phase 5b). Registers the generic
* `npc.target-select` capability and owns scored aggregation, stickiness, the
* anti-thrash switch threshold, and the legacy nearest-hostile fallback. Cold
* enumerates scored candidates and applies the chosen target.
* Editing this file changes NPC target selection live after the next DLL
* activation; the shared implementation keeps the fallback behavior identical.
* Does NOT own candidate enumeration, combat, the loop, transport, or damage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-npc-target-select.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

// Hot formula layer: scales the anti-thrash switch margin. Editing this changes
// how eagerly NPCs swap targets live after the next hot DLL activation.
constexpr float kSwitchThresholdMultiplier = 1.0f;

void MIMITA_GAME_CALL npcTargetSelect(void* /*host*/,
                                      NpcTargetSelectPolicyV1* r)
{
    if (!r)
        return;
    if (r->structSize == 0)
        r->structSize = sizeof(NpcTargetSelectPolicyV1);
    const float original = r->targetSwitchThreshold;
    r->targetSwitchThreshold = original * kSwitchThresholdMultiplier;
    MimitaNet::HotNpcTargetSelectImpl::evaluate(*r);
    r->targetSwitchThreshold = original;  // restore the in/out field
}

const GameCapabilityDescriptorV1 kNpcTargetSelectProvider{
    GAME_CAP_NPC_TARGET_SELECT, GAME_SIG_NPC_TARGET_SELECT, 0,
    reinterpret_cast<void*>(&npcTargetSelect), "npc.target-select"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcTargetSelectProvider{
    kNpcTargetSelectProvider};

#endif // MIMITA_GAME_DLL
