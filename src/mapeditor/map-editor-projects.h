// 2026-11-05
/* purpose
* Register the native world-project browser commands used by the terminal,
* scenarios, and the in-game editor panel.
*/
#pragma once

#include <string>

void registerWorldProjectCommands();

// The project currently opened through the command/browser surface, or empty.
std::string currentWorldProjectId();
