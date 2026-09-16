#pragma once

#include "weapon-types.h"

namespace WeaponData {

void loadWeaponJsonConfig();
void registerWeaponFromJson(WeaponDefinition def);
// Adopt hot tool definitions that have no builtin counterpart (hot new weapons).
void registerHotTools();

} // namespace WeaponData
