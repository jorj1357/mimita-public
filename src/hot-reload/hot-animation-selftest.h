// 09 16 2026
/* purpose
* Hot animation candidate self-test. Deterministic invariant/determinism checks
* over the procedural clip library and the versioned animation state contract.
* The DLL GameSelfTest hook calls this BEFORE a candidate generation is
* activated, so a malformed or internally inconsistent animation candidate is
* rejected and the previous generation keeps running.
* Does NOT own runtime policy, draw submission, or actor state.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#pragma once

#if defined(MIMITA_GAME_DLL)

#include <cstdint>

// Returns true when the animation code passes its deterministic checks; on
// failure writes a short reason into `message` (bounded by messageSize).
bool runAnimationSelfTest(char* message, std::uint32_t messageSize);

#endif // MIMITA_GAME_DLL
