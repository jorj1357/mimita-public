// 09 21 2026
/* purpose
* Determinism self-test for the fixed pellet grid and single-ray spread cycle.
* Verifies the spread pattern is identical across calls and independent of the
* (now inert) seed, and that client and server generators agree exactly.
* Does NOT own weapon definitions, damage, or collision.
*/
#pragma once

#include <string>

bool runPelletPatternSelfTest(std::string& report);
