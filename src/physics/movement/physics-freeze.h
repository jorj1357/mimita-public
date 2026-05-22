// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-freeze.h
// mar 8 2026
/**
 * purpose
 * header for freeze file
 * that exposes doFreeze(args)
 * to be called in phsics file
 */

#pragma once

#include "entities/player.h"

void doFreeze(
    Player& p,
    bool freezeHeld,
    float dt
);