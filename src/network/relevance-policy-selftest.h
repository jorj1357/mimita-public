// 09 15 2026
/* purpose
* Declares the headless generic relevance-policy self-test.
* Does NOT own rendering, networking, or the transport.
*/
#pragma once

#include <string>

bool runRelevancePolicySelfTest(std::string& report);
