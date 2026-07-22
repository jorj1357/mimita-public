// 07 21 2026, 20 39
/* purpose
* Owns the in-memory weapon definition registry used by gameplay and tests.
* Normalizes derived weapon metadata before storing definitions by id.
* Provides lookup helpers for runtime systems without owning simulation behavior.
* Does NOT parse JSON, execute attacks, send network packets, or mutate player state.
* Does NOT own weapon audio, rendering, projectile physics, or damage authority.
* Does NOT specialize registration by client, server, UDP, or ICE launch mode.
*/

#include "weapon-registry.h"
#include <cstdio>

WeaponRegistry& WeaponRegistry::instance() {
    static WeaponRegistry registry;
    return registry;
}

void WeaponRegistry::registerWeapon(const WeaponDefinition& def) {
    WeaponDefinition normalized = def;
    normalized.executionType =
        weaponExecutionTypeForBehavior(normalized.behaviorType);

    if (normalized.id.empty()) {
        printf("[WEAPON] ERROR: Cannot register weapon with empty ID\n");
        return;
    }
    if (mWeapons.find(normalized.id) != mWeapons.end()) {
        printf("[WEAPON] WARNING: Overwriting existing weapon '%s'\n", normalized.id.c_str());
    }
    mWeapons[normalized.id] = normalized;
    printf("[WEAPON] Registered: %s (slot %d)\n", normalized.id.c_str(), normalized.slot);
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
