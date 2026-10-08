#pragma once

#include "weapon-types.h"

namespace WeaponData {

WeaponDefinition createNothingDefinition();
WeaponDefinition createRevolverDefinition();
WeaponDefinition createGodballDefinition();
WeaponDefinition createShotgunDefinition();
WeaponDefinition createSwordswordDefinition();
WeaponDefinition createOpRevolverDefinition();
WeaponDefinition createAa12Definition();
WeaponDefinition createRocketLauncherDefinition();
WeaponDefinition createGrenadeLauncherDefinition();
WeaponDefinition createAdminRevolverDefinition();
WeaponDefinition createHafsDefinition();
WeaponDefinition createProjectileRifleDefinition();
WeaponDefinition createBigShotgunDefinition();
WeaponDefinition createLargeMachineGunDefinition();

void registerBuiltinWeapons();
bool reloadBuiltinWeaponsIfChanged();

} // namespace WeaponData
