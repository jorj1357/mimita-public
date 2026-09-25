// 09 24 2026
/* purpose
* Hot NPC combat-decision provider (migration Phase 5d). Registers the generic
* `npc.combat-decision` capability and owns the fire gate and fire-aggression
* blend. Cold owns the firing mechanics and applies the decision.
* Editing this file changes NPC fire behavior live after the next DLL
* activation; the shared implementation keeps the fallback behavior identical.
* Does NOT own hitscan/projectile mechanics, the loop, transport, or damage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-npc-combat-decision.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

// Hot formula layer: extra aggression added on top of the config bonus. Editing
// this changes how eagerly NPCs fire live after the next hot DLL activation.
constexpr float kAggressionBias = 0.0f;

void MIMITA_GAME_CALL npcCombatDecision(void* /*host*/,
                                        NpcCombatDecisionPolicyV1* r)
{
    if (!r)
        return;
    if (r->structSize == 0)
        r->structSize = sizeof(NpcCombatDecisionPolicyV1);
    MimitaNet::HotNpcCombatDecisionImpl::evaluate(*r);
    if (r->outAggression + kAggressionBias < 0.0f)
        r->outAggression = 0.0f;
    else if (r->outAggression + kAggressionBias > 1.0f)
        r->outAggression = 1.0f;
    else
        r->outAggression += kAggressionBias;
}

const GameCapabilityDescriptorV1 kNpcCombatDecisionProvider{
    GAME_CAP_NPC_COMBAT_DECISION, GAME_SIG_NPC_COMBAT_DECISION, 0,
    reinterpret_cast<void*>(&npcCombatDecision), "npc.combat-decision"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcCombatDecisionProvider{
    kNpcCombatDecisionProvider};

#endif // MIMITA_GAME_DLL
