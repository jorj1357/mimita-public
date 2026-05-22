
// C:\important\quiet\n\mimita-priv-v7\src\gui\hud\player-nameplates.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawPlayerNameplates(args)
 *
 * this file DOES:
 * - draw player/enemy name + hp labels
 *
 * this file DOES NOT:
 * - own player data
 * - apply damage
 */

#pragma once

struct World;
struct Camera;

void drawPlayerNameplates(
    const World& world,
    const Camera& camera
);