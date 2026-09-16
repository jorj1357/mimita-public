// 09 15 2026
/* purpose
* Declares the headless generic content-resource self-test.
* Does NOT own transport, GPU upload, or simulation publication.
*/
#pragma once

#include <string>

bool runContentResourceSelfTest(std::string& report);
