#include "pch.h"
#include "combat/spyknife-damage.h"

#include <algorithm>
#include <cmath>

namespace {
float paramOr(const WeaponDefinition& def, const char* name, float fallback)
{
    const auto it = def.customParams.find(name);
    return it == def.customParams.end() ? fallback : it->second;
}
}

namespace SpyKnifeDamage {

Result evaluate(const WeaponDefinition& def, const ImpactMetrics& impact,
                bool backstab)
{
    if (backstab) {
        return {std::max(0.0f, paramOr(def, "backstabDamagePerTick", 999.0f)),
                std::max(0.0f, paramOr(def, "backstabKnockback", 20.0f))};
    }

    const float minDamage = std::max(0.0f,
        paramOr(def, "minDamage", paramOr(def, "baseDamage", 5.0f)));
    const float maxDamage = std::max(minDamage,
        paramOr(def, "maxDamage", 999.0f));
    const float speed = std::max(0.0f, impact.speed);
    const float force = std::max(0.0f, impact.force);
    const float directness = std::clamp(impact.directness, 0.0f, 1.0f);
    const float angle = std::pow(directness, std::max(0.0f,
        paramOr(def, "angleDamageExponent", 1.0f)));
    const float rawImpact = (speed * std::max(0.0f,
        paramOr(def, "speedDamageScale", 0.35f)) + force * std::max(0.0f,
        paramOr(def, "forceDamageScale", 0.65f))) * angle;
    const float curvedImpact = std::pow(std::max(0.0f, rawImpact *
        std::max(0.0f, paramOr(def, "impactDamageScale", 1.0f))),
        std::max(1.0f, paramOr(def, "impactDamageExponent", 1.35f)));
    const float damage = std::clamp(minDamage + curvedImpact,
                                    minDamage, maxDamage);

    const float baseKnockback = paramOr(def, "baseKnockback", 30.0f);
    const float knockback = std::clamp(
        baseKnockback + speed * paramOr(def, "speedKnockbackFactor", 4.0f) +
            directness * paramOr(def, "angleKnockbackFactor", 2.0f),
        0.0f, paramOr(def, "maxKnockback", 200.0f));
    return {damage, knockback};
}

} // namespace SpyKnifeDamage
