// 09 22 2026
/* purpose
* Self-test for the rewound hitscan hitbox bridge: publish -> read -> clear.
* Proves the cold server and hot behavior share the exact same body-part boxes.
* Does NOT own weapons, damage, or networking.
*/
#pragma once

#include <string>

bool runHitscanTargetSelfTest(std::string& report);
