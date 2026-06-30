#pragma once

#include "weapon-types.h"

namespace WeaponData {

void loadWeaponJsonConfig();
void registerWeaponFromJson(WeaponDefinition def);

} // namespace WeaponData
