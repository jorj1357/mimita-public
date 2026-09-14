// 09 14 2026
/* purpose
* Headless self-test for the kernel capsule movement primitive: determinism,
// integration parity, and floor collision. Does NOT run the game.
*/
#pragma once

#include <string>

bool runMovementSelfTest(std::string& report);
