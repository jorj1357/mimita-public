// 09 24 2026
/* purpose
* Hot NPC navigation-goal provider (migration Phase 5f). Registers the generic
* `npc.nav-goal` capability and owns the mapping from brain state + bounded
* memory to an abstract navigation goal. Cold owns pathfinding, steering, and
* world queries and applies the goal to the navigator.
* Editing this file changes NPC navigation intent live after the next DLL
* activation; the shared implementation keeps the fallback behavior identical.
* Does NOT own pathfinding, steering, collision, the loop, or movement physics.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-npc-nav-goal.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

// Hot formula layer: scales the MaintainDistance goal only. Editing this changes
// how far NPCs try to hold from a target live after the next hot DLL activation.
constexpr float kMaintainDistanceScale = 1.0f;

void MIMITA_GAME_CALL npcNavGoal(void* /*host*/, NpcNavGoalPolicyV1* r)
{
    if (!r)
        return;
    if (r->structSize == 0)
        r->structSize = sizeof(NpcNavGoalPolicyV1);
    r->maintainDistanceScale *= kMaintainDistanceScale;
    MimitaNet::HotNpcNavGoalImpl::evaluate(*r);
}

const GameCapabilityDescriptorV1 kNpcNavGoalProvider{
    GAME_CAP_NPC_NAV_GOAL, GAME_SIG_NPC_NAV_GOAL, 0,
    reinterpret_cast<void*>(&npcNavGoal), "npc.nav-goal"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcNavGoalProvider{
    kNpcNavGoalProvider};

#endif // MIMITA_GAME_DLL
