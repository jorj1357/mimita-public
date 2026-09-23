// 09 23 2026
/* purpose
* Declares the fixed-tick behavior parity harness. It compares the C++ and JSON
* implementations of the movement, collision, and animation contracts for the
* same fixed ticks and reports the first/max divergence, so a source switch is
* evidence-based rather than assumed. Report-only: it never rejects a candidate
* on known tuning differences, but it does reject non-finite output.
* Hot-only header. Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

// Returns true when the comparison completed with finite output. Prints a
// per-domain summary and any divergences; the active sources are also written to
// `message`.
bool runBehaviorParitySelfTest(char* message, std::uint32_t messageSize);
