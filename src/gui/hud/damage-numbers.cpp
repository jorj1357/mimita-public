// C:\important\quiet\n\mimita-priv-v7\src\gui\hud\damage-numbers.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * damageNumbers(args)
 *
 * this file DOES:
 * - hold/update/render floating number system skeleton
 *
 * this file DOES NOT:
 * - decide when hits happen
 */

#include "damage-numbers.h"
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>   // REQUIRED for std::remove_if

struct DamageNumberStub
{
    std::string text;
    float age = 0.0f;
    float lifetime = 1.0f;
};

static std::vector<DamageNumberStub> gDamageNumbers;

void damageNumbers(float dt)
{
    printf("[HUD DAMAGE NUMBERS] begin count=%zu dt=%f\n", gDamageNumbers.size(), dt);

    for (size_t i = 0; i < gDamageNumbers.size(); ++i)
    {
        gDamageNumbers[i].age += dt;
    }

    gDamageNumbers.erase(
        std::remove_if(
            gDamageNumbers.begin(),
            gDamageNumbers.end(),
            [](const DamageNumberStub& n){ return n.age >= n.lifetime; }
        ),
        gDamageNumbers.end()
    );
    
    printf("[HUD DAMAGE NUMBERS] end count=%zu\n", gDamageNumbers.size());
}