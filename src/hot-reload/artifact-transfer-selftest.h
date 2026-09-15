// 09 15 2026
/* purpose
* Declares the headless chunked artifact wire-transfer self-test.
* Does NOT own sockets, activation, or the loader.
*/
#pragma once

#include <string>

bool runArtifactTransferSelfTest(std::string& report);
