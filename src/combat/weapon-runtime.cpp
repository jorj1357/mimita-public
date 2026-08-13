#include "weapon-runtime.h"
#include "weapon-registry.h"
#include "config.h"
#include "../debug/debug-log.h"
#include "../entities/player.h"
#include "perf/perf-spike.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

int initialReserveAmmoForDefinition(const WeaponDefinition& def)
{
    auto it = def.customParams.find("reserveAmmo");
    return (it != def.customParams.end()) ? (int)it->second : 1337;
}

void WeaponRuntime::reset(const WeaponDefinition& def)
{
    currentAmmo = def.magazineSize;
    reserveAmmo = initialReserveAmmoForDefinition(def);
    pendingReloadRounds = 0;
    fireCooldown = 0.0f;
    reloadTimer = 0.0f;
    shootEffectTimer = 0.0f;
    reloadBufferTimer = 0.0f;
    isReloading = false;
    isCharging = false;
    chargeAmount = 0.0f;
    godball = GodballState{};
    customFloats.clear();
    customVec3s.clear();
}

void WeaponRuntimeHelper::initRuntime(WeaponRuntime& rt, const WeaponDefinition& def) {
    rt.currentAmmo = def.magazineSize;
    rt.reserveAmmo = initialReserveAmmoForDefinition(def);
    rt.pendingReloadRounds = 0;
    rt.fireCooldown = 0.0f;
    rt.reloadTimer = 0.0f;
    rt.shootEffectTimer = 0.0f;
    rt.reloadBufferTimer = 0.0f;
    rt.isReloading = false;
    rt.isCharging = false;
    rt.chargeAmount = 0.0f;
    rt.godball = WeaponRuntime::GodballState{};
    rt.customFloats.clear();
    rt.customVec3s.clear();
    printf("[WEAPON] Runtime initialized for weapon '%s': ammo=%d reserve=%d\n",
           def.id.c_str(), rt.currentAmmo, rt.reserveAmmo);
    if (def.id == "aa12")
        Debug::log(Debug::Category::Weapons, "[AA12] Runtime init: fireCooldown=%.2f (fireDelay=%.2f)", rt.fireCooldown, def.fireDelay);
}

void WeaponRuntimeHelper::updateCooldowns(WeaponRuntime& rt, float dt) {
    rt.fireCooldown = std::max(0.0f, rt.fireCooldown - dt);
    rt.shootEffectTimer = std::max(0.0f, rt.shootEffectTimer - dt);
    if (rt.isReloading && rt.reloadTimer > 0.0f) {
        rt.reloadTimer = std::max(0.0f, rt.reloadTimer - dt);
    }
}

bool WeaponRuntimeHelper::canFire(const WeaponRuntime& rt, const WeaponDefinition& def) {
    if (rt.isReloading || rt.fireCooldown > 0.0f) {
        return false;
    }
    if (def.behaviorType != WeaponBehaviorType::Godball) {
        if (rt.currentAmmo <= 0) return false;
    }
    return true;
}

void WeaponRuntimeHelper::consumeAmmo(WeaponRuntime& rt, const WeaponDefinition& def) {
    if (def.behaviorType == WeaponBehaviorType::Godball) return;
    if (rt.currentAmmo > 0) {
        rt.currentAmmo--;
    }
}

bool WeaponRuntimeHelper::canReload(const WeaponRuntime& rt, const WeaponDefinition& def) {
    if (def.magazineSize <= 0) return false;
    if (rt.isReloading) return false;
    if (rt.currentAmmo >= def.magazineSize) return false;
    if (rt.reserveAmmo <= 0) return false;
    return true;
}

void WeaponRuntimeHelper::startReload(WeaponRuntime& rt, const WeaponDefinition& def) {
    if (!canReload(rt, def)) return;
    rt.isReloading = true;
    rt.reloadTimer = def.reloadTime;
    printf("[WEAPON] Started reload: timer=%.2f current=%d reserve=%d\n",
           def.reloadTime, rt.currentAmmo, rt.reserveAmmo);
}

void WeaponRuntimeHelper::tickReload(WeaponRuntime& rt, const WeaponDefinition& def, float dt) {
    if (!rt.isReloading) return;
    rt.reloadTimer -= dt;
    if (rt.reloadTimer > 0.0f) return;

    bool oneAtATime = false;
    auto it = def.customParams.find("reloadOneAtATime");
    if (it != def.customParams.end()) oneAtATime = (it->second != 0.0f);

    if (oneAtATime) {
        // Load +1 grenade (TF2-style), restart if more needed
        int loaded = std::min(1, rt.reserveAmmo);
        rt.currentAmmo += loaded;
        rt.reserveAmmo -= loaded;
        printf("[WEAPON] Reload +1: ammo=%d reserve=%d\n",
               rt.currentAmmo, rt.reserveAmmo);
        if (rt.currentAmmo < def.magazineSize && rt.reserveAmmo > 0) {
            rt.reloadTimer = def.reloadTime; // restart for next round
        } else {
            rt.isReloading = false;
            rt.reloadTimer = 0.0f;
            printf("[WEAPON] Reload complete: ammo=%d reserve=%d\n",
                   rt.currentAmmo, rt.reserveAmmo);
        }
    } else {
        int needed = def.magazineSize - rt.currentAmmo;
        int loaded = std::min(needed, rt.reserveAmmo);
        rt.currentAmmo += loaded;
        rt.reserveAmmo -= loaded;
        rt.isReloading = false;
        rt.reloadTimer = 0.0f;
        printf("[WEAPON] Reload complete: ammo=%d reserve=%d\n",
               rt.currentAmmo, rt.reserveAmmo);
    }
}

#include <filesystem>

static FILE* gSpawnDebugFile = nullptr;
static void openSpawnDebugLog()
{
    if (gSpawnDebugFile) return;
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    gSpawnDebugFile = fopen("logs/spawn_weapon_debug.txt", "a");
    if (gSpawnDebugFile)
        fprintf(gSpawnDebugFile, "=== spawn weapon state log ===\n");
}
static void closeSpawnDebugLog()
{
    if (gSpawnDebugFile) { fclose(gSpawnDebugFile); gSpawnDebugFile = nullptr; }
}

void resetAllWeaponRuntimesForSpawn(Player& player, const char* caller)
{
    MIMITA_PERF_SCOPE("WeaponRuntime::ResetAllForSpawn");
    if (DebugConfig::DEBUG_WEAPON_SPAWN_LOG) {
        openSpawnDebugLog();
        const WeaponRegistry& reg = WeaponRegistry::instance();
        for (auto& kv : player.weaponRuntimes)
        {
            const std::string& weaponId = kv.first;
            WeaponRuntime& rt = kv.second;
            const WeaponDefinition* def = reg.get(weaponId);
            if (!def) continue;

            int oldAmmo = rt.currentAmmo;
            int oldReserve = rt.reserveAmmo;
            bool oldReloading = rt.isReloading;
            float oldReloadTimer = rt.reloadTimer;
            float oldCooldown = rt.fireCooldown;

            rt.reset(*def);

            if (gSpawnDebugFile)
            {
                fprintf(gSpawnDebugFile, "Player/NPC weapon=%s caller=%s\n", weaponId.c_str(), caller);
                fprintf(gSpawnDebugFile, "  Before: ammo=%d reserve=%d reloading=%d reloadTimer=%.2f cooldown=%.2f\n",
                        oldAmmo, oldReserve, (int)oldReloading, oldReloadTimer, oldCooldown);
                fprintf(gSpawnDebugFile, "  After:  ammo=%d reserve=%d reloading=%d reloadTimer=%.2f cooldown=%.2f\n",
                        rt.currentAmmo, rt.reserveAmmo, (int)rt.isReloading, rt.reloadTimer, rt.fireCooldown);
            }
        }
        closeSpawnDebugLog();
    } else {
        const WeaponRegistry& reg = WeaponRegistry::instance();
        for (auto& kv : player.weaponRuntimes)
        {
            const std::string& weaponId = kv.first;
            WeaponRuntime& rt = kv.second;
            const WeaponDefinition* def = reg.get(weaponId);
            if (!def) continue;
            rt.reset(*def);
        }
    }
}
