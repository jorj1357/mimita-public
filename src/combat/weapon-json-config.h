#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "weapon-types.h"

namespace WeaponData {

void loadWeaponJsonConfig();
void registerWeaponFromJson(WeaponDefinition def);
// Adopt hot tool definitions that have no builtin counterpart (hot new weapons).
void registerHotTools();

// The one resolved path and parsed root for config/weapons.json. Other readers
// (viewmodel config, grenade-launcher physics) use these instead of re-parsing
// or hardcoding a CWD-relative path.
const std::string& configPath();
const nlohmann::json& configRoot();

} // namespace WeaponData
