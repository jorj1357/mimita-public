// C:\important\quiet\n\mimita-priv-v7\src\physics\physics-mini.h
// feb 10 2026
/**
 * purpose
 * header so that
 * the rewrite twhere 
 * 1 big phsics file = 12 littel movement files
 * is better
 */

#pragma once

struct Player;
struct World;
struct InputState;

void physicsMainUpdate(
    Player& player,
    const World& world,
    const InputState& input,
    float dt
);
