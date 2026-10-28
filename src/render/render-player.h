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
void beginPlayerTrailBatch();
void drawPlayerTrailBatch(const Camera& cam);
void renderNetworkPlayer(
    const Player& player,
    const Camera& cam,
    uint32_t networkEntityId,
    bool isLocal,
    int localTeam = -1,
    int actorTeam = -1);

// Bounded, read-only diagnostics for the appearance layers that can hide the
// actor (outline and wireframe). 0 = all layers, 1 = outline only,
// 2 = wireframe only, 3 = body only. Debug logging is off by default.
void setPlayerVisualDebugEnabled(bool enabled);
bool playerVisualDebugEnabled();
void setPlayerVisualLayerFilter(int filter);
int playerVisualLayerFilter();
void requestPlayerVisualDump();
