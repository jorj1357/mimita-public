// 09 14 2026
/* purpose
* Falsifiable proof that capability providers are generic, generation-scoped,
* signature-validated, and rollback-safe with no name-specific kernel routing.
* Exercises the runtime registry directly (no DLL swap) so every branch is
* observable headless.
* Does NOT own gameplay state.
*/
#pragma once

#include <string>

bool runCapabilitySelfTest(std::string& report);
