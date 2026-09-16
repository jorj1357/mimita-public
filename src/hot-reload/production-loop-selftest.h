// 09 15 2026
/* purpose
* Declares the headless production-loop F -> G self-test: real HotReloadSystem,
// real artifact bytes, real install, real verify/migration/switch transaction,
// asserting world/entity continuity across the generation change.
*/
#pragma once

#include <string>

bool runProductionLoopSelfTest(std::string& report);
