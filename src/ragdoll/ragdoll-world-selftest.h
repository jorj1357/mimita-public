// 09 21 2026
/* purpose
* Ragdoll world-contact self-test. Drives the real hot `ragdoll.solve` capability
* (which now routes limb collision through `collision.main`) against a floor and
* asserts limbs are constrained by world geometry. This is the dedicated
* world-contact assertion the ragdoll slice self-test intentionally omits.
* Does NOT own ragdoll policy; it exercises the live hot solver.
*/
#pragma once

#include <string>

bool runRagdollWorldSelfTest(std::string& report);
