#include "weapon-runtime.h"
#include <algorithm>
#include <cstdio>

void WeaponRuntimeHelper::initRuntime(WeaponRuntime& rt, const WeaponDefinition& def) {
    rt.currentAmmo = def.magazineSize;
    auto it = def.customParams.find("reserveAmmo");
    rt.reserveAmmo = (it != def.customParams.end()) ? (int)it->second : 1337;
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
    if (rt.reloadTimer > 0.0f) {
        rt.reloadTimer -= dt;
        if (rt.reloadTimer <= 0.0f) {
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
}