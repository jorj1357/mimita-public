// 09 24 2026
/* purpose
* Hot NPC AI state-selection provider (migration Phase 5e). Registers the generic
* `npc.state-select` capability and owns the state scoring, randomness, anti-
* thrash current-state penalty, and the stuck/hit/no-target guards. Cold owns
* the world/navigation query, the mind scalars, and weapon range.
* Editing this file changes NPC state transitions live after the next DLL
* activation; the shared implementation keeps the fallback behavior identical.
* Does NOT own navigation, movement generation, the loop, or rendering.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-npc-state-select.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

// Hot formula layer: scales the per-candidate randomness spread. Editing this
// changes how varied NPC state choices are live after the next DLL activation.
// It does not change how many RNG draws occur, so the sequence stays stable.
constexpr float kStateRandomnessScale = 1.0f;

void MIMITA_GAME_CALL npcStateSelect(void* /*host*/, NpcStateSelectPolicyV1* r)
{
    if (!r)
        return;
    if (r->structSize == 0)
        r->structSize = sizeof(NpcStateSelectPolicyV1);
    r->randomnessScale *= kStateRandomnessScale;
    MimitaNet::HotNpcStateSelectImpl::evaluate(*r);
}

const GameCapabilityDescriptorV1 kNpcStateSelectProvider{
    GAME_CAP_NPC_STATE_SELECT, GAME_SIG_NPC_STATE_SELECT, 0,
    reinterpret_cast<void*>(&npcStateSelect), "npc.state-select"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcStateSelectProvider{
    kNpcStateSelectProvider};

#endif // MIMITA_GAME_DLL
