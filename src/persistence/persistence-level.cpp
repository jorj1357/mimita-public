// 09 01 2026, 00 00
/* purpose
* Implement a provisional XP-to-level curve for player progression.
* Level starts at 1, grows monotonically, 64-bit safe.
* The exact curve is a later balancing pass; totalXp is the source of truth.
* Does NOT persist XP or level to any database.
*/

#include "persistence/persistence-level.h"

#include <cmath>
#include <cstdint>

int calculateLevel(int64_t totalXp)
{
    if (totalXp <= 0)
        return 1;

    // Provisional curve: level = floor(sqrt(totalXp / 100)) + 1
    // Level 1  at     0 XP
    // Level 10 at   8100 XP
    // Level 25 at  57600 XP
    // Level 50 at 240100 XP
    // Level 78 at 600600 XP
    // Level 100 at 980100 XP
    // Grows slowly enough for long tail, no practical cap.
    const double xp = static_cast<double>(totalXp);
    const int level = static_cast<int>(std::sqrt(xp / 100.0)) + 1;
    return level > 0 ? level : 1;
}
