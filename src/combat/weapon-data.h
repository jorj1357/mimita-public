#pragma once

#include "weapon-types.h"

namespace WeaponData {

WeaponDefinition createRevolverDefinition();
WeaponDefinition createGodballDefinition();
WeaponDefinition createShotgunDefinition();
WeaponDefinition createSwordswordDefinition();
WeaponDefinition createOpRevolverDefinition();

void registerBuiltinWeapons();

} // namespace WeaponData