// 2026-10-27
/* purpose
* Draw the seated vehicle state (engine, mode, gear, RPM, speed, clutch/brake)
* using the existing HUD text path.
* Does NOT simulate or own vehicle state.
*/
#pragma once

void drawVehicleHud();

// True while the local player is seated in a Rocket League arena profile.
// Other HUD owners use this to suppress combat-only elements.
bool vehicleArenaHudActive();
