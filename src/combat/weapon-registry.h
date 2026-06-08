#pragma once

#include "weapon-types.h"
#include <unordered_map>
#include <vector>
#include <string>

class WeaponRegistry {
public:
    static WeaponRegistry& instance();

    void registerWeapon(const WeaponDefinition& def);
    const WeaponDefinition* get(const std::string& id) const;
    std::vector<std::string> getAllIds() const;
    const std::unordered_map<std::string, WeaponDefinition>& all() const;

    bool has(const std::string& id) const;

private:
    WeaponRegistry() = default;
    std::unordered_map<std::string, WeaponDefinition> mWeapons;
};