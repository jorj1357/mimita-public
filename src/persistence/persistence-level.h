// 09 01 2026, 00 00
/* purpose
* Provide a centralized XP-to-level calculation function.
* Level is derived from totalXp, never stored as the source of truth.
* Formula is provisional and easy to replace in a future balancing pass.
* Does NOT implement persistence, rewards, or database logic.
*/

#pragma once

#include <cstdint>

int calculateLevel(int64_t totalXp);
