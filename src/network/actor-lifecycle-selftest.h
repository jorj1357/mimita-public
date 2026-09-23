// 09 23 2026
/* purpose
* Declares the headless generic actor-lifecycle boundary self-test.
* Does NOT own transport or the live server loop.
*/
#pragma once

#include <string>

bool runActorLifecycleSelfTest(std::string& report);
