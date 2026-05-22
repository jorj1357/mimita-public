// C:\important\quiet\n\mimita-priv-v7\src\combat\impact-damage.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * impactDamage(args)
 *
 * this file DOES:
 * - log impact damage inputs and future formula point
 *
 * this file DOES NOT:
 * - own gui
 */

#include "impact-damage.h"
#include <cstdio>

void impactDamage(float speed, float angleDegrees)
{
    printf("[IMPACT DAMAGE] speed=%f angle=%f\n", speed, angleDegrees);
    printf("[IMPACT DAMAGE] TODO convert speed*angle into hp loss and bounce\n");
}