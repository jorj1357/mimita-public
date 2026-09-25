// 09 24 2026
/* purpose
* Define the generic, hot-replaceable NPC combat-decision policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the firing mechanics (shared weapon system) and config; the hot policy
* owns the fire gate (cooldown, weapon, range cap, ammo/reload, line of sight)
* and the fire aggression blend. Migration Phase 5d.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own hitscan/projectile mechanics, the loop, transport, or damage.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace HotNpcCombatDecisionImpl {

inline float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

inline void evaluate(NpcCombatDecisionPolicyV1& r)
{
    r.handled = 1u;
    r.result = 1u;
    r.shouldFire = 0u;
    r.startReload = 0u;
    r.outReloadSeconds = 0.0f;

    // Fire aggression: mind baseline plus situational terms, then the config
    // bonus (mirrors computeFireAggression + aggressionBonus).
    const float lowHealth = (1.0f - r.healthFrac) * 0.3f;
    const float closeTarget = r.targetDistance < 5.0f ? 0.3f : 0.0f;
    const float recentlyHit = r.recentlyHit ? 0.4f : 0.0f;
    const float visible = r.visible ? 0.2f : 0.0f;
    r.outAggression = clamp01(
        clamp01(r.effectiveAggressionBase + lowHealth + closeTarget + recentlyHit +
                visible) +
        r.aggressionBonus);

    // Fire gate.
    if (r.attackCooldown > 0.0f)
        return;
    if (!r.hasWeapon)
        return;
    if (r.distance > r.rangeCap)
        return;
    if (r.ammoCurrent <= 0 && r.ammoReserve <= 0)
        return;
    if (r.ammoCurrent <= 0 && r.ammoReserve > 0 && !r.isReloading) {
        r.startReload = 1u;
        r.outReloadSeconds = r.reloadTime;
        return;
    }
    if (r.isReloading)
        return;
    if (r.ammoCurrent <= 0)
        return;
    if (r.losBlocked)
        return;
    r.shouldFire = 1u;
}

} // namespace HotNpcCombatDecisionImpl

} // namespace MimitaNet
