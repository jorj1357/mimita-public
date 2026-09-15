// 09 15 2026
/* purpose
* Declares the headless content-addressed artifact-cache/acquisition self-test.
* Does NOT own building, loading, transport, or activation.
*/
#pragma once

#include <string>

bool runArtifactCacheSelfTest(std::string& report);
