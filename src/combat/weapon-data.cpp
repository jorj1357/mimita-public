#include "weapon-data.h"
#include "weapon-registry.h"
#include "../debug/debug-log.h"

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
    def.soundHit = "godballhit";
    def.soundDryFire = "";
    def.soundEquip = "";

    def.customParams["ballRadius"] = 0.5f;
    def.customParams["ballMass"] = 0.18f;
    def.customParams["ropeLength"] = 2.8f;
    def.customParams["ropeStiffness"] = 50.0f;
    def.customParams["ropeDamping"] = 1.0f;
    def.customParams["linearDamping"] = 0.0f;
    def.customParams["baseDamagePerTick"] = 10.0f;
    def.customParams["damageTickInterval"] = 0.15f;
    def.customParams["speedDamageFactor"] = 3.0f;
    def.customParams["maxDamageCap"] = 200.0f;
    def.customParams["relativeVelocityFactor"] = 2.0f;
    def.customParams["swingDirectionFactor"] = 2.0f;

    return def;
}

WeaponDefinition createShotgunDefinition() {
    WeaponDefinition def;
    def.id = "shotgun";
    def.displayName = "Shotgun";
    def.slot = 3;

    def.modelPath = "assets/objects/weapons/mimita-shotgun-v1.glb";
    def.viewModelOffset = {0.0f, 0.0f, 0.5f};
    def.viewModelRotation = {0.0f, 0.0f, 0.0f};
    def.weaponScale = 1.0f;

    def.damage = 12.0f;
    def.headshotMultiplier = 2.0f;
    def.fireDelay = 0.25f;
    def.reloadTime = 1.5f;
    def.magazineSize = 2;
    def.pelletCount = 15;

    def.spread = 3.0f;
    def.recoil = 130.0f;
    def.projectileSpeed = 0.0f;
    def.projectileRadius = 0.0f;
    def.projectileLifetime = 0.0f;

    def.fireMode = WeaponFireMode::SemiAuto;
    def.behaviorType = WeaponBehaviorType::Hitscan;
    def.hitscan = true;
    def.usesPhysicsProjectile = false;

    def.soundShoot = "shotgunshoot";
    def.soundReload = "shotgunreload";
    def.soundHit = "player_hurt";
    def.soundDryFire = "ui/click";
    def.soundEquip = "shotgunequip";

    def.customParams["distanceFalloffStart"] = 60.0f;
    def.customParams["minDamageFraction"] = 0.05f;
    def.customParams["minAngleFactor"] = 0.15f;
    def.customParams["gridSpreadDegrees"] = 3.0f;

    return def;
}

WeaponDefinition createSwordswordDefinition() {
    WeaponDefinition def;
    def.id = "swordsword";
    def.displayName = "Swordsword";
    def.slot = 4;

    def.modelPath = "";
    def.viewModelOffset = {0.5f, 0.8f, 0.3f};
    def.viewModelRotation = {0.0f, 0.0f, 0.0f};
    def.weaponScale = 1.0f;

    def.damage = 35.0f;
    def.headshotMultiplier = 1.5f;
    def.fireDelay = 0.3f;
    def.reloadTime = 0.0f;
    def.magazineSize = 0;
    def.pelletCount = 1;

    def.spread = 0.0f;
    def.recoil = 10.0f;
    def.projectileSpeed = 0.0f;
    def.projectileRadius = 0.0f;
    def.projectileLifetime = 0.0f;

    def.fireMode = WeaponFireMode::SemiAuto;
    def.behaviorType = WeaponBehaviorType::Swordsword;
    def.hitscan = false;
    def.usesPhysicsProjectile = false;

    def.soundShoot = "";
    def.soundReload = "";
    def.soundHit = "";
    def.soundDryFire = "";
    def.soundEquip = "";

    def.customParams["range"] = 4.0f;
    def.customParams["arcDegrees"] = 120.0f;
    def.customParams["slashDamage"] = 35.0f;
    def.customParams["slashKnockback"] = 25.0f;
    def.customParams["slashSpeed"] = 22.0f;
    def.customParams["slashDuration"] = 0.25f;
    def.customParams["lungeDamage"] = 55.0f;
    def.customParams["lungeKnockback"] = 40.0f;
    def.customParams["lungeSpeed"] = 35.0f;
    def.customParams["lungeDuration"] = 0.3f;
    def.customParams["lungeRange"] = 6.0f;
    def.customParams["lungeCooldown"] = 0.5f;
    def.customParams["bladeLength"] = 1.5f;

    return def;
}

WeaponDefinition createOpRevolverDefinition() {
    WeaponDefinition def = createRevolverDefinition();
    def.id = "op_revolver";
    def.displayName = "OP Revolver";
    def.slot = 5;
    def.fireDelay = 0.01f;
    def.magazineSize = 999;
    def.customParams["reserveAmmo"] = 9999.0f;
    def.fireMode = WeaponFireMode::Automatic;
    def.poseId = "revolver";
    return def;
}

WeaponDefinition createAa12Definition() {
    WeaponDefinition def = createShotgunDefinition();
    def.id = "aa12";
    def.displayName = "AA12";
    def.slot = 6;
    def.fireDelay = 0.1f;
    def.magazineSize = 999;
    def.customParams["reserveAmmo"] = 9999.0f;
    def.fireMode = WeaponFireMode::Automatic;
    def.poseId = "shotgun";
    Debug::log(Debug::Category::Weapons, "[AA12] Registered: aa12 (slot 6, auto, fireDelay=%.2f, ammo=%d, reserve=%.0f)",
               def.fireDelay, def.magazineSize, def.customParams["reserveAmmo"]);
    return def;
}

WeaponDefinition createRocketLauncherDefinition() {
    WeaponDefinition def;
    def.id = "rocket_launcher";
    def.displayName = "Rocket Launcher";
    def.slot = 7;

    def.modelPath = "";
    def.viewModelOffset = {0.0f, 0.0f, 0.0f};
    def.viewModelRotation = {0.0f, 0.0f, 0.0f};
    def.weaponScale = 1.0f;

    def.damage = 0.0f;
    def.headshotMultiplier = 1.0f;
    def.fireDelay = 0.7f;
    def.reloadTime = 2.0f;
    def.magazineSize = 4;
    def.pelletCount = 1;

    def.spread = 0.0f;
    def.recoil = 50.0f;
    def.projectileSpeed = 45.0f;
    def.projectileRadius = 0.4f;
    def.projectileLifetime = 5.0f;

    def.fireMode = WeaponFireMode::SemiAuto;
    def.behaviorType = WeaponBehaviorType::RocketLauncher;
    def.hitscan = false;
    def.usesPhysicsProjectile = false;

    def.poseId = "shotgun";
    def.soundShoot = "rocketlauncher/rocketshoot";
    def.soundReload = "";
    def.soundHit = "";
    def.soundDryFire = "ui/click";
    def.soundEquip = "";

    def.customParams["rocketSpeed"] = 45.0f;
    def.customParams["rocketRadius"] = 0.4f;
    def.customParams["splashRadius"] = 12.0f;
    def.customParams["splashExponent"] = 2.0f;
    def.customParams["rocketDirectDamage"] = 150.0f;
    def.customParams["knockbackStrength"] = 160.0f;
    def.customParams["selfKnockbackMultiplier"] = 1.0f;
    def.customParams["firingRecoilStrength"] = 30.0f;
    def.customParams["reserveAmmo"] = 1337.0f;

    return def;
}

void registerBuiltinWeapons() {
    WeaponRegistry::instance().registerWeapon(createRevolverDefinition());
    WeaponRegistry::instance().registerWeapon(createGodballDefinition());
    WeaponRegistry::instance().registerWeapon(createShotgunDefinition());
    WeaponRegistry::instance().registerWeapon(createSwordswordDefinition());
    WeaponRegistry::instance().registerWeapon(createOpRevolverDefinition());
    WeaponRegistry::instance().registerWeapon(createAa12Definition());
    WeaponRegistry::instance().registerWeapon(createRocketLauncherDefinition());
    Debug::log(Debug::Category::Weapons, "[AA12] Registered builtin weapons: revolver, godball, shotgun, swordsword, op_revolver, aa12, rocket_launcher");
}

} // namespace WeaponData
