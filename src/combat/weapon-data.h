#pragma once

#include "weapon-types.h"

namespace WeaponData {

WeaponDefinition createRevolverDefinition();
WeaponDefinition createGodballDefinition();
WeaponDefinition createShotgunDefinition();

void registerBuiltinWeapons();

} // namespace WeaponData