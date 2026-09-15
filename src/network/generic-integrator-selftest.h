// 09 15 2026
/* purpose
* Declares the headless generic integrator self-test: a typeless runtime entity
* (no Player/Npc type) runs through the SAME movement pipeline and hot policies.
* Does NOT own rendering, networking, or policy.
*/
#pragma once

#include <string>

bool runGenericIntegratorSelfTest(std::string& report);
