// 09 12 2026
/* purpose
* Declares the headless entity/component vertical-slice self-test.
* Verifies stable ids, components, control sources, rocket owner resolution,
* deterministic projectile simulation, and identity across a DLL reload.
* Does NOT own the registry or the systems it checks.
*/
#pragma once

#include <string>

bool runEntitySliceSelfTest(std::string& report);
