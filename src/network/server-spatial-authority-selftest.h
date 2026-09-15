// 09 15 2026
/* purpose
* Declares the headless server spatial-authority self-test.
* Does NOT own rendering, networking, or gameplay policy.
*/
#pragma once

#include <string>

bool runServerSpatialAuthoritySelfTest(std::string& report);
