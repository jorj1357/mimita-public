// C:\important\quiet\n\mimita-priv-v7\src\combat\weapon-hit.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * weaponHit(args)
 *
 * this file DOES:
 * - evaluate one weapon hit skeleton
 *
 * this file DOES NOT:
 * - draw gui
 * - own impact damage
 */

#pragma once

struct Player;

void weaponHit(Player& attacker);