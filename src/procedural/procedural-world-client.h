// 09 28 2026, 00 00
/* purpose
* Client-side application of the server-authoritative procedural-world state.
* Reconstructs one visible + collidable exit barrier (a PhysicalEntity) for the
* active room from the replicated seed/room/mode, and removes it when the server
* unlocks the exit or disables the mode.
* Does NOT decide room completion, spawn rooms, or mutate server state.
*/

#pragma once

#include "world/world.h"

class Camera;
class Player;

void clientProceduralWorldTick(World& world);
void clientProceduralWorldReset();
void clientProceduralTeleportShieldStart();
void clientProceduralTeleportShieldTick();
void clientProceduralTeleportShieldRender(const Player& player,
                                          const Camera& camera);
void clientProceduralTeleportShieldRenderUi();
