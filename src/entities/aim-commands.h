// 08 15 2026, 12 00
/* purpose
* Exposes the live tuning commands for the body-aim (look pitch) animation.
* Declares registerAimCommands() only; the implementation owns command wiring.
* Does NOT own the animation math, the aim-mode config, or firing logic.
*/
#pragma once

void registerAimCommands();
