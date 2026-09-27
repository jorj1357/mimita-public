#pragma once

#include "combat/weapon-types.h"

namespace SpyKnifeDamage {

struct ImpactMetrics {
    float speed = 0.0f;
    float force = 0.0f;
    float directness = 0.0f;
};

struct Result {
    float damage = 0.0f;
    float knockback = 0.0f;
};

Result evaluate(const WeaponDefinition& def, const ImpactMetrics& impact,
                bool backstab);

} // namespace SpyKnifeDamage
