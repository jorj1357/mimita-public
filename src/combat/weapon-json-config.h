#pragma once

#include "weapon-types.h"
#include <nlohmann/json.hpp>
#include <string>

namespace WeaponData {

void loadWeaponJsonConfig();
nlohmann::json readWeaponDefinitionRoot();
const nlohmann::json& cachedWeaponDefinitionRoot();
bool hasCachedWeaponDefinitionRoot();
std::string weaponDefinitionSignature();
void registerWeaponFromJson(WeaponDefinition def, bool builtin = true);
// Registers any JSON-only weapon/moveset definition whose id has no builtin
// constructor. Called by registerBuiltinWeapons() after the builtin list.
void registerWeaponsFromJsonRoot();

} // namespace WeaponData
