#include "weapon-registry.h"
#include <cstdio>

WeaponRegistry& WeaponRegistry::instance() {
    static WeaponRegistry registry;
    return registry;
}

void WeaponRegistry::registerWeapon(const WeaponDefinition& def) {
    if (def.id.empty()) {
        printf("[WEAPON] ERROR: Cannot register weapon with empty ID\n");
        return;
    }
    if (mWeapons.find(def.id) != mWeapons.end()) {
        printf("[WEAPON] WARNING: Overwriting existing weapon '%s'\n", def.id.c_str());
    }
    mWeapons[def.id] = def;
    printf("[WEAPON] Registered: %s (slot %d)\n", def.id.c_str(), def.slot);
}

const WeaponDefinition* WeaponRegistry::get(const std::string& id) const {
    auto it = mWeapons.find(id);
    if (it != mWeapons.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> WeaponRegistry::getAllIds() const {
    std::vector<std::string> ids;
    ids.reserve(mWeapons.size());
    for (const auto& pair : mWeapons) {
        ids.push_back(pair.first);
    }
    return ids;
}

const std::unordered_map<std::string, WeaponDefinition>& WeaponRegistry::all() const {
    return mWeapons;
}

bool WeaponRegistry::has(const std::string& id) const {
    return mWeapons.find(id) != mWeapons.end();
}