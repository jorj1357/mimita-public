// C:\important\quiet\n\mimita-priv-v7\src\render\render-player.h
// feb 10 2026
/**
 * purpose
 * rned erplauer
 * render plauer
 * wrapper
 * small wrapper so that the game faster i 
 * think
 */

#pragma once

#include <cstdint>

struct Player;
class Camera;

void renderPlayer(const Player& player, const Camera& cam);
void renderNetworkPlayer(
    const Player& player,
    const Camera& cam,
    uint32_t networkEntityId,
    bool isLocal);
