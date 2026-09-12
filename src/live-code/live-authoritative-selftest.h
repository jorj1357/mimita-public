// 09 12 2026
/* purpose
* Headless self-test for the hot authoritative damage path.
* Verifies the kernel dispatches GAME_EVENT_DAMAGE_POLICY to the hot behavior
* and applies exactly the returned value without a gameplay clamp.
* Does NOT pin tuned damage constants.
*/
#pragma once

#include <string>

bool runHotAuthoritativeSelfTest(std::string& report);
