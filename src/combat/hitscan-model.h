// 09 22 2026
/* purpose
* The single owner of the v2.0.6 hitscan damage model: base x body-part x range
* falloff (x angle). Header-only and type-agnostic (resolved scalar params, not a
* weapon definition) so the SAME implementation serves the cold EXE and the hot
* game DLL — one weapon-damage owner across the hot/cold boundary.
* Editing this header is a hot change: it is a tracked hot header, so weapon
* damage/falloff behavior can be revised live.
* Does NOT own target selection, world collision, ammo, or packets.
*/
#pragma once

#include <algorithm>
#include <cmath>

// Resolved tuning for one hitscan weapon. Callers map their own definition
// representation (cold WeaponDefinition, hot ToolDefinitionV1) onto this.
struct HitscanDamageParams {
    float baseDamage = 1.0f;
    float headshotMultiplier = 2.0f;
    float limbDamageMultiplier = 0.75f;
    // 0 or negative = no range falloff.
    float distanceFalloffStart = 0.0f;
    float minDamageFraction = 0.1f;
    float falloffExponent = 1.0f;
};

// factor = pow(clamp(1 - distance/start, minFraction, 1), exponent); 1 if no falloff.
inline float hitscanFalloffFactor(const HitscanDamageParams& p, float distance) {
    if (p.distanceFalloffStart <= 0.0f)
        return 1.0f;
    const float minFraction = p.minDamageFraction;
    const float exponent = std::max(0.01f, p.falloffExponent);
    float factor = std::clamp(1.0f - distance / p.distanceFalloffStart, minFraction, 1.0f);
    return std::pow(factor, exponent);
}

// head = headshotMultiplier, leg = limbDamageMultiplier, else 1x.
inline float hitscanPartMultiplier(const HitscanDamageParams& p, bool head, bool leg) {
    if (head)
        return std::max(1.0f, p.headshotMultiplier);
    if (leg)
        return p.limbDamageMultiplier;
    return 1.0f;
}

inline int computeHitscanDamage(const HitscanDamageParams& p, bool head, bool leg,
                                float distance, float angleFactor = 1.0f) {
    const float damage = p.baseDamage
        * hitscanPartMultiplier(p, head, leg)
        * hitscanFalloffFactor(p, distance)
        * std::clamp(angleFactor, 0.0f, 1.0f);
    return std::max(1, (int)std::lround(damage));
}

// Per-pellet knockback magnitude applied along the shot direction.
inline float hitscanKnockbackMagnitude(float damage, float knockbackPerDamage) {
    return damage * knockbackPerDamage;
}
