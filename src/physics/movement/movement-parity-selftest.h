// 09 14 2026
/* purpose
* Headless parity/determinism harness for the hot movement path: loads the hot
// package, drives the real movement.main system through the generic runtime and
// capabilities, and checks its result against the kernel capsule primitive.
* Does NOT run the game or own movement policy.
*/
#pragma once

#include <string>

bool runMovementParitySelfTest(std::string& report);
