// 09 15 2026
/* purpose
* Declares the headless real-path air-movement parity self-test: server movement
* vs local prediction (`movement.main`), both through the shared hot
* air-acceleration policy. Does NOT own rendering, networking, or policy.
*/
#pragma once

#include <string>

bool runAirMovementParitySelfTest(std::string& report);
