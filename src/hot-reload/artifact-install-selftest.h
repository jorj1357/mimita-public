// 09 15 2026
/* purpose
* Declares the headless remote-artifact install self-test.
* Does NOT own transport, distribution, or activation.
*/
#pragma once

#include <string>

bool runArtifactInstallSelfTest(std::string& report);
