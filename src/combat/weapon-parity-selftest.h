// 09 22 2026
/* purpose
* v2.0.6 weapon parity harness seed: freezes the revolver/shotgun reference
* numbers and asserts the shared single-owner hitscan damage model plus the
* frozen pellet grid produce the expected values, and that the cold
* WeaponExecution wrapper uses the same model.
* Does NOT own weapons: it only compares the owners.
*/
#pragma once

#include <string>

bool runWeaponParitySelfTest(std::string& report);
