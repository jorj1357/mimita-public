#pragma once

#include <string>
#include <vector>

// ── Tips Manager ──────────────────────────────────────────────────────
// Loads tips from config/tips.json and provides random selection.
// Never repeats the same tip twice in a row.

namespace Tips {

// Load tips from the JSON file. Call once at startup.
void load();

// Get a random tip that is not the same as the last one shown.
// Returns empty string if no tips are loaded.
std::string getRandomTip();

// Get the total number of loaded tips.
int count();

// Get a specific tip by index (0-based).
std::string getTip(int index);

} // namespace Tips
