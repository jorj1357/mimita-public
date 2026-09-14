// 09 14 2026
/* purpose
* Headless self-test for the generic hot combat policy path: tool-use routing,
* projectile-impact routing, a brand-new runtime tool/projectile, package tool
* state, and cold fallback when no behavior is registered.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#pragma once

#include <string>

bool runHotCombatSelfTest(std::string& report);
