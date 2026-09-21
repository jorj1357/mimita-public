// 09 21 2026
/* purpose
* v2.0.6 movement parity harness and golden-fixture generator.
*
* Runs deterministic fixed-60Hz scenarios through:
*   1. the frozen v2.0.6 oracle (`MimitaV206::step`), which owns movement and
*      collision exactly as v2.0.6 did;
*   2. the current cold shared movement kernel (`movement-step.cpp`) with the
*      C++ preset; and
*   3. the same kernel with the JSON preset.
*
* It reports the first tick where each path diverges from the v2.0.6 oracle for
* velocity, grounded state, and position, plus the maximum velocity deviation.
* It never edits gameplay policy and never owns movement.
*
* Collision caveat: the cold kernel has no collision of its own, so the harness
* integrates its position from its own velocity and feeds it a scenario-level
* grounded flag. Position divergence therefore includes collision-ownership
* differences; velocity is the movement-formula signal.
*/
#pragma once

#include <string>

namespace MimitaV206 {

bool runMovementV206ParitySelfTest(std::string& report);
bool generateMovementV206Fixtures(std::string& report);

} // namespace MimitaV206
