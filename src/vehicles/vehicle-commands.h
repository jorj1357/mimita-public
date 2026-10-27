// 2026-10-27
/* purpose
* Register the vehicle terminal surface: reload templates, spawn/despawn,
* enter/exit local driving, list state, and inject input for validation.
* Commands call the shared VehicleSystem actions used by input and future GUI.
* Does NOT own vehicle physics, JSON parsing, or rendering.
*/
#pragma once

void registerVehicleCommands();
