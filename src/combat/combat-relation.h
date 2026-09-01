// 09 01 2026, 00 00
/* purpose
* Defines the combat relationship model for team-based gameplay.
* Classifies interactions as Self, Friendly, or Enemy for filtering.
* Does NOT contain damage logic, rendering, or network code.
* Does NOT modify player state or weapon behavior.
*/

#pragma once

#include <cstdint>

enum class CombatRelation : uint8_t {
    Self = 0,
    Friendly = 1,
    Enemy = 2
};

// Returns the combat relationship between attacker and victim.
// attackerTeam/victimTeam: -1 = no team (FFA), 0 = red, 1 = blue
inline CombatRelation getCombatRelation(uint32_t attackerId, int attackerTeam,
                                        uint32_t victimId, int victimTeam)
{
    if (attackerId == victimId)
        return CombatRelation::Self;
    if (attackerTeam >= 0 && victimTeam >= 0 && attackerTeam == victimTeam)
        return CombatRelation::Friendly;
    return CombatRelation::Enemy;
}
