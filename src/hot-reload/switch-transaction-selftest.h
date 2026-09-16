// 09 15 2026
/* purpose
* Declares the headless switch-transaction self-test.
* Does NOT own transport, loading, or distribution.
*/
#pragma once

#include <string>

bool runSwitchTransactionSelfTest(std::string& report);
