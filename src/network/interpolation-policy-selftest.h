// 09 15 2026
/* purpose
* Declares the headless hot interpolation-policy self-test.
* Does NOT own rendering, networking, or the transport.
*/
#pragma once

#include <string>

bool runInterpolationPolicySelfTest(std::string& report);
