// 09 24 2026
/* purpose
* Define the generic, hot-replaceable NPC scored weapon-selection policy and the
* ONE implementation shared by the cold EXE fallback and the hot provider. The
* EXE enumerates the loadout (it owns weapon definitions, ammo runtime, config,
* the explicit force-weapon case, and the legacy distance cases); the hot policy
* owns the behavior-profile scoring (range fit, damage utility, safety) and the
* anti-thrash switch threshold. This is migration Phase 5c.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own weapon definitions, firing, reload mechanics, the loop, or damage.
*/
#pragma once

#include <cmath>
#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace HotNpcWeaponSelectImpl {

inline float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// Mirror of WeaponBehaviorType numeric values (src/combat/weapon-registry.h).
enum : std::uint32_t {
    kHitscan = 0,
    kProjectile = 1,
    kMelee = 3,
    kSwordsword = 4,
    kRocketLauncher = 5,
    kGrenadeLauncher = 6,
};

inline float score(const NpcWeaponSelectPolicyV1& r,
                   const NpcWeaponCandidateV1& c)
{
    const float effRange = c.effRange;
    const float denom = effRange > 1.0f ? effRange : 1.0f;
    const float rangeFit =
        1.0f - clamp01(std::fabs(effRange - r.distance) / denom);
    const float burst = c.damage * static_cast<float>(
                                       c.pelletCount > 1 ? c.pelletCount : 1);
    const float damageUtility = clamp01(burst / 80.0f);
    float safety = clamp01(effRange / 80.0f);
    if (c.behaviorType == kRocketLauncher || c.behaviorType == kGrenadeLauncher ||
        c.behaviorType == kProjectile)
        safety *= 0.5f;
    else if (c.behaviorType == kMelee || c.behaviorType == kSwordsword)
        safety = 0.05f;
    return rangeFit * r.weaponRangeBias + damageUtility * r.weaponDamageBias +
           safety * r.weaponSafetyBias;
}

inline void evaluate(NpcWeaponSelectPolicyV1& r)
{
    r.handled = 1u;
    r.result = 1u;
    r.chosenWeaponHash = r.currentWeaponHash;
    if (r.candidateCount == 0)
        return;

    const std::uint32_t count =
        r.candidateCount < (std::uint32_t)NPC_WEAPON_SELECT_MAX
            ? r.candidateCount
            : (std::uint32_t)NPC_WEAPON_SELECT_MAX;

    float currentScore = -1e30f;
    bool currentUsable = false;
    float bestScore = -1e30f;
    std::uint64_t bestWeapon = r.currentWeaponHash;
    for (std::uint32_t i = 0; i < count; ++i) {
        const NpcWeaponCandidateV1& c = r.candidates[i];
        if (c.weaponHash == r.currentWeaponHash && r.currentWeaponHash != 0) {
            currentScore = score(r, c);
            currentUsable = c.usable != 0u;
        }
        if (c.usable) {
            const float s = score(r, c);
            if (s > bestScore) {
                bestScore = s;
                bestWeapon = c.weaponHash;
            }
        }
    }

    // Keep the current weapon unless a candidate is clearly better.
    if (currentUsable && bestWeapon != r.currentWeaponHash &&
        bestScore <= currentScore + r.weaponSwitchThreshold)
        bestWeapon = r.currentWeaponHash;

    r.chosenWeaponHash = bestWeapon;
}

} // namespace HotNpcWeaponSelectImpl

} // namespace MimitaNet
