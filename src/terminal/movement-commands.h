// 08 02 2026, 00 00
/* purpose
* Declares terminal command registration for movement tuning presets.
* Exposes registerMovementCommands() for main-systems.cpp.
* Does NOT load movement configs, run physics, or own tuning defaults.
* Does NOT edit preset files or persist selector state.
*/

#pragma once

void registerMovementCommands();
