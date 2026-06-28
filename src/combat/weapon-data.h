#pragma once

#include "weapon-types.h"

namespace WeaponData {

WeaponDefinition createRevolverDefinition();
WeaponDefinition createGodballDefinition();
WeaponDefinition createShotgunDefinition();
WeaponDefinition createSwordswordDefinition();
WeaponDefinition createOpRevolverDefinition();
WeaponDefinition createAa12Definition();
WeaponDefinition createRocketLauncherDefinition();

void registerBuiltinWeapons();

} // namespace WeaponData