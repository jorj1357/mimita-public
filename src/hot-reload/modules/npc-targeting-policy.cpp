// 09 23 2026
/* purpose
* Hot NPC targeting policy module. Registers the generic `net.npc-targeting`
* capability and serves the shared hostility/score formulas from
* `hot-reload/hot-npc-targeting.h`. Editing that header (or this file) changes
* NPC target selection live: the cold bridge in `network/server-npcs.cpp`
* prefers this provider over its compiled fallback, with no EXE call site.
* Does NOT own targeting storage, movement, or damage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-npc-targeting.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL npcHostility(void* /*host*/, GameNpcHostilityV1* request)
{
    HotNpcTargetingImpl::hostile(*request);
}

void MIMITA_GAME_CALL npcTargetScore(void* /*host*/, GameNpcTargetScoreV1* request)
{
    HotNpcTargetingImpl::score(*request);
}

const GameNpcTargetingPolicyV1 kNpcTargetingPolicy{
    sizeof(GameNpcTargetingPolicyV1), 1, &npcHostility, &npcTargetScore,
    "net.npc-targeting"};

const GameNpcTargetingPolicyV1* MIMITA_GAME_CALL lookupNpcTargeting(void* /*host*/)
{
    return &kNpcTargetingPolicy;
}

const GameCapabilityDescriptorV1 kNpcTargetingProvider{
    GAME_CAP_NPC_TARGETING, GAME_SIG_NPC_TARGETING, 0,
    reinterpret_cast<void*>(&lookupNpcTargeting), "net.npc-targeting"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcTargetingProviderRegistrar{
    kNpcTargetingProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_npcTargetingRequirement{
    GAME_CAP_NPC_TARGETING, GAME_SIG_NPC_TARGETING, 0};

#endif // MIMITA_GAME_DLL
