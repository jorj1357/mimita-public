#pragma once

#include "weapon-types.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>

class WeaponRegistry {
public:
    static WeaponRegistry& instance();

    void registerWeapon(const WeaponDefinition& def);
    const WeaponDefinition* get(const std::string& id) const;
    const WeaponDefinition* getBase(const std::string& id) const;
    void setActiveDefinitions(std::unordered_map<std::string, WeaponDefinition> definitions);
    void clearActiveDefinitions();
    bool hasActiveDefinitions() const { return !mActiveWeapons.empty(); }
    std::vector<std::string> getAllIds() const;
    const std::unordered_map<std::string, WeaponDefinition>& all() const;
    uint64_t revision() const { return mRevision; }

    bool has(const std::string& id) const;

private:
    WeaponRegistry() = default;
    std::unordered_map<std::string, WeaponDefinition> mWeapons;
    std::unordered_map<std::string, WeaponDefinition> mActiveWeapons;
    uint64_t mRevision = 1;
};
