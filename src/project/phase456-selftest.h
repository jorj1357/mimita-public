// 09 12 2026
/* purpose
* Headless self-test for phases 4-6 primitives: multi-domain scheduler,
* packages, resources, capabilities, dependency verification, subsystem
* replacement lifecycle, component schemas, world hashing, and the watcher.
* Does NOT touch the running world.
*/
#pragma once

#include <string>

bool runPhase456SelfTest(std::string& report);
