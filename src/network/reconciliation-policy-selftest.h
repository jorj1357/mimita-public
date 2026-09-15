// 09 15 2026
/* purpose
* Declares the headless hot reconciliation-policy self-test.
* Does NOT own rendering, networking, or the transport.
*/
#pragma once

#include <string>

bool runReconciliationPolicySelfTest(std::string& report);
