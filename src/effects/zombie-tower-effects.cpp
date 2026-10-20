#include "effects/zombie-tower-effects.h"

ZombieTowerEffects& ZombieTowerEffects::instance()
{
    static ZombieTowerEffects effects;
    return effects;
}

