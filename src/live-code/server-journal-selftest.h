// 09 23 2026
/* purpose
* Declares the headless server-journal + exit-cause self-test.
* Does NOT own transport or the live server loop.
*/
#pragma once

#include <string>

bool runServerJournalSelfTest(std::string& report);
