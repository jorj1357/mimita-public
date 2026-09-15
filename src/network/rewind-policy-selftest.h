// 09 15 2026
/* purpose
* Declares the headless hot rewind-policy self-test.
* Does NOT own rendering, networking, or the transport.
*/
#pragma once

#include <string>

bool runRewindPolicySelfTest(std::string& report);
