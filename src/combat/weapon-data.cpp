#include "weapon-data.h"
#include "weapon-registry.h"

#include <cstdio>

namespace WeaponData {

WeaponDefinition createRevolverDefinition() {
    WeaponDefinition def;
    def.id = "revolver";
    def.displayName = "Revolver";
    def.slot = 1;

    def.modelPath = "assets/objects/weapons/mimita-revolver-v1.glb";
    def.viewModelOffset = {0.0f, 0.0f, 0.0f};
    def.viewModelRotation = {0.0f, 0.0f, 0.0f};
    def.weaponScale = 1.0f;

    def.damage = 50.0f;
    def.headshotMultiplier = 2.0f;
    def.fireDelay = 0.08f;
    def.reloadTime = 1.0f;
    def.magazineSize = 6;
    def.pelletCount = 1;

    def.spread = 0.0f;
    def.recoil = 99.0f;
    def.projectileSpeed = 0.0f;
    def.projectileRadius = 0.0f;
    def.projectileLifetime = 0.0f;

    def.fireMode = WeaponFireMode::SemiAuto;
    def.behaviorType = WeaponBehaviorType::Hitscan;
    def.hitscan = true;
    def.usesPhysicsProjectile = false;

    def.soundShoot = "revolvershoot";
    def.soundReload = "revolverreload";
    def.soundHit = "player_hurt";
    def.soundDryFire = "ui/click";
    def.soundEquip = "revolverequip";

    def.customParams["distanceFalloffStart"] = 110.0f;
    def.customParams["minDamageFraction"] = 0.1f;
    def.customParams["minAngleFactor"] = 0.15f;

    return def;
}

WeaponDefinition createGodballDefinition() {
    WeaponDefinition def;
    def.id = "godball";
    def.displayName = "Godball";
    def.slot = 2;

    def.modelPath = "";
    def.viewModelOffset = {0.0f, 0.0f, 0.0f};
    def.viewModelRotation = {0.0f, 0.0f, 0.0f};
    def.weaponScale = 1.0f;

    def.damage = 10.0f;
    def.headshotMultiplier = 1.0f;
    def.fireDelay = 0.0f;
    def.reloadTime = 0.0f;
    def.magazineSize = 0;
    def.pelletCount = 1;

    def.spread = 0.0f;
    def.recoil = 0.0f;
    def.projectileSpeed = 0.0f;
    def.projectileRadius = 0.5f;
    def.projectileLifetime = 0.0f;

    def.fireMode = WeaponFireMode::Automatic;
    def.behaviorType = WeaponBehaviorType::Godball;
    def.hitscan = false;
    def.usesPhysicsProjectile = false;

    def.soundShoot = "";
    def.soundReload = "";
    def.soundHit = "player_hurt";
    def.soundDryFire = "";
    def.soundEquip = "";

    def.customParams["ballRadius"] = 0.5f;
    def.customParams["ballMass"] = 0.1f;
    def.customParams["ropeLength"] = 2.0f;
    def.customParams["ropeStiffness"] = 50.0f;
    def.customParams["ropeDamping"] = 2.0f;
    def.customParams["linearDamping"] = 0.02f;
    def.customParams["angularDamping"] = 0.01f;
    def.customParams["baseDamagePerTick"] = 10.0f;
    def.customParams["damageTickInterval"] = 0.1f;
    def.customParams["speedDamageFactor"] = 3.0f;
    def.customParams["maxDamageCap"] = 200.0f;
    def.customParams["relativeVelocityFactor"] = 2.0f;
    def.customParams["swingDirectionFactor"] = 2.0f;

    return def;
}

void registerBuiltinWeapons() {
    WeaponRegistry::instance().registerWeapon(createRevolverDefinition());
    WeaponRegistry::instance().registerWeapon(createGodballDefinition());
    printf("[WEAPON] Registered builtin weapons: revolver, godball\n");
}

} // namespace WeaponData