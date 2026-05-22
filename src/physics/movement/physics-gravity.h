// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-gravity.h
// feb 10 2026
/**
 * purpose
 * HEADER FOR THE FILE THAT 
 * handles all gravity
 * should expose like
 * applygravity(args)
 * and just gets called by other files
 * maibe called bi the specific files? or the phsics main file itself, idk
 */

 #pragma once

class Player;

// Applies gravity to vertical velocity only.
// - Uses physics/config.h
// - Debug heavy
// - No collision, no ground snap
// - Caller is responsible for grounded handling
void doGravity(
    Player& p,
    float dt
);
