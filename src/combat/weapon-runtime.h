#pragma once

#include "weapon-types.h"
#include <unordered_map>
#include <string>

class Player;

int initialReserveAmmoForDefinition(const WeaponDefinition& def);
void resetAllWeaponRuntimesForSpawn(Player& player, const char* caller = "unknown");

struct WeaponRuntimeHelper {
    static void initRuntime(WeaponRuntime& rt, const WeaponDefinition& def);
    static void updateCooldowns(WeaponRuntime& rt, float dt);
    static bool canFire(const WeaponRuntime& rt, const WeaponDefinition& def);
    static void consumeAmmo(WeaponRuntime& rt, const WeaponDefinition& def);
    static bool canReload(const WeaponRuntime& rt, const WeaponDefinition& def);
    static void startReload(WeaponRuntime& rt, const WeaponDefinition& def);
    static void tickReload(WeaponRuntime& rt, const WeaponDefinition& def, float dt);
};
