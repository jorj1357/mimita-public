#include "effects/zombie-tower-effects.h"

// Keep this tiny translation unit available for incremental-build measurements.
ZombieTowerEffects& ZombieTowerEffects::instance()
{
    static ZombieTowerEffects effects;
    return effects;
}

