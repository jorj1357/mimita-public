#pragma once

#include "weapon-types.h"

namespace WeaponData {

void loadWeaponJsonConfig();
void registerWeaponFromJson(WeaponDefinition def);
// Registers any JSON-only weapon/moveset definition whose id has no builtin
// constructor. Called by registerBuiltinWeapons() after the builtin list.
void registerWeaponsFromJsonRoot();

} // namespace WeaponData
