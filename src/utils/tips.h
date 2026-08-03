// 08 03 2026, 15 00
/* purpose
* Provides the tip content manager for the client.
* Loads gameplay tips from config/tips.json and returns random tips without
* repeating the previous one, plus the last chosen tip's 1-based number.
* Does NOT schedule, render, or push notifications.
* Does NOT own networking, chat, or server-side tip scheduling.
*/
#pragma once

#include <string>
#include <vector>

namespace Tips {

// Load tips from the JSON file. Call once at startup.
void load();

// Get a random tip that is not the same as the last one shown.
// Returns empty string if no tips are loaded.
std::string getRandomTip();

// 0-based index of the most recent getRandomTip() result, or -1 if none yet.
int lastIndex();

// Get the total number of loaded tips.
int count();

// Get a specific tip by index (0-based).
std::string getTip(int index);

} // namespace Tips
