// 09 23 2026
/* purpose
* Declares the "live rules change does not reset the match" self-test.
* Does NOT own transport or the live server loop.
*/
#pragma once

#include <string>

bool runLiveRulesReloadSelfTest(std::string& report);
