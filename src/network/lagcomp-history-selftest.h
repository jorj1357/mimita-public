// 09 23 2026
/* purpose
* Declares the cold/hot lag-compensation history oracle self-test.
* Does NOT own transport or the live server loop.
*/
#pragma once

#include <string>

bool runLagcompHistorySelfTest(std::string& report);
