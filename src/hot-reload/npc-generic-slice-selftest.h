// 09 24 2026
/* purpose
* Falsifiable headless proof of the generic NPC actor vertical slice: generic
* identity, generic actor components (transform/velocity/health/origin/lifecycle/
* avatar), the generic actor-state read/write envelope, origin-aware
* reconciliation, and the one generic destruction path. Exercises the component
* layer directly (no DLL swap, no live server) so every branch is observable.
* Does NOT own gameplay state or networking.
*/
#pragma once

#include <string>

bool runNpcGenericSliceSelfTest(std::string& report);
