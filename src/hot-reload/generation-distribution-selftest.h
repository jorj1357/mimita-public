// 09 15 2026
/* purpose
* Declares the headless distributed-generation state-machine self-test.
* Does NOT own building, loading, transport, or artifact transfer.
*/
#pragma once

#include <string>

bool runGenerationDistributionSelfTest(std::string& report);
