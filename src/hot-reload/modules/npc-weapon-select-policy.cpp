// 09 24 2026
/* purpose
* Hot NPC scored weapon-selection provider (migration Phase 5c). Registers the
* generic `npc.weapon-select` capability and owns the behavior-profile scoring
* (range fit, damage utility, safety) and the anti-thrash switch threshold. Cold
* enumerates the loadout and applies the chosen weapon.
* Editing this file changes NPC weapon choice live after the next DLL
* activation; the shared implementation keeps the fallback behavior identical.
* Does NOT own weapon definitions, firing, reload mechanics, the loop, or damage.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-npc-weapon-select.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

// Hot formula layer: scales the anti-thrash switch margin. Editing this changes
// how eagerly NPCs swap weapons live after the next hot DLL activation.
constexpr float kWeaponSwitchThresholdMultiplier = 1.0f;

void MIMITA_GAME_CALL npcWeaponSelect(void* /*host*/,
                                      NpcWeaponSelectPolicyV1* r)
{
    if (!r)
        return;
    if (r->structSize == 0)
        r->structSize = sizeof(NpcWeaponSelectPolicyV1);
    const float original = r->weaponSwitchThreshold;
    r->weaponSwitchThreshold = original * kWeaponSwitchThresholdMultiplier;
    MimitaNet::HotNpcWeaponSelectImpl::evaluate(*r);
    r->weaponSwitchThreshold = original;  // restore the in/out field
}

const GameCapabilityDescriptorV1 kNpcWeaponSelectProvider{
    GAME_CAP_NPC_WEAPON_SELECT, GAME_SIG_NPC_WEAPON_SELECT, 0,
    reinterpret_cast<void*>(&npcWeaponSelect), "npc.weapon-select"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_npcWeaponSelectProvider{
    kNpcWeaponSelectProvider};

#endif // MIMITA_GAME_DLL
