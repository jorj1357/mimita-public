// 09 22 2026
/* purpose
* v2.0.6 fixed-tick combat parity harness for projectile, melee, and
* attack-policy scenarios. Freezes the reference formulas (projectile splash
* falloff, physical-contact damage/knockback) and drives the live hot owners
* (attack policy, damage policy) so a behavior change is caught against the
* reference.
* Does NOT own weapons, projectiles, or networking; it only compares owners.
*/
#pragma once

#include <string>

bool runCombatV206ParitySelfTest(std::string& report);
