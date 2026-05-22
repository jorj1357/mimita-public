// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-ground-return.cpp
// feb 10 2026
/**
 * purpose
 * handles all logic for ground return
 * just exposes like doGroundReturn(args) or something
 * gravities here are called from gravit function file
 * like applyGravity(args)
 */

#pragma once

class Player;

// Handles ground-return (slam down)
// - Uses physics/config.h
// - Debug heavy
// - No gravity, no collision, no snap
// - Caller decides when grounded is updated
void doGroundReturn(
    Player& p,
    bool groundReturnPressed,
    float dt
);
