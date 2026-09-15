// 09 14 2026
/* purpose
* Declares the headless Counter-Strike objective-round self-test.
* Does NOT own rendering, networking, or objective policy.
*/
#pragma once

#include <string>

bool runCounterstrikeSelfTest(std::string& report);
