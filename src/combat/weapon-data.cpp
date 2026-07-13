#include "weapon-data.h"
#include "weapon-json-config.h"
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

    return def;
}

WeaponDefinition createSwordswordDefinition() {
    WeaponDefinition def;
    def.id = "swordsword";
    def.displayName = "Swordsword";
    def.slot = 11;

    def.modelPath = "assets/objects/weapons/mimita-hafs-v1.glb";
    def.viewModelOffset = {0.0f, 0.5f, 1.5f};
    def.viewModelRotation = {0.0f, 0.0f, 0.0f};
    def.weaponScale = 1.0f;

    def.damage = 10.0f;
    def.headshotMultiplier = 1.0f;
    def.fireDelay = 0.25f;
    def.reloadTime = 0.0f;
    def.magazineSize = 0;
    def.pelletCount = 1;

    def.spread = 0.0f;
    def.recoil = 0.0f;
    def.projectileSpeed = 0.0f;
    def.projectileRadius = 0.0f;
    def.projectileLifetime = 0.0f;

    def.fireMode = WeaponFireMode::SemiAuto;
    def.behaviorType = WeaponBehaviorType::Swordsword;
    def.hitscan = false;
    def.usesPhysicsProjectile = false;

    def.soundShoot = "weapon/hafs/hafsswing";
    def.soundReload = "";
    def.soundHit = "weapon/hafs/hafsknockback";
    def.soundDryFire = "";
    def.soundEquip = "weapon/hafs/hafsequip";
    def.poseId = "hafs";

    // Global multipliers
    def.customParams["globalDamageMultiplier"] = 10.0f;
    def.customParams["globalKnockbackMultiplier"] = 10.0f;
    def.customParams["knockbackHorizontalMultiplier"] = 1.0f;
    def.customParams["knockbackVerticalMultiplier"] = 0.1f;
    def.customParams["selfKnockbackMultiplier"] = 0.8f;

    // Dimensions
    def.customParams["bladeLength"] = 4.0f;
    def.customParams["bladeRadius"] = 0.9f;

    // Idle pose
    def.customParams["idleOffsetX"] = 0.0f;
    def.customParams["idleOffsetY"] = 0.5f;
    def.customParams["idleOffsetZ"] = 1.5f;
    def.customParams["idleRotationPitch"] = 0.0f;
    def.customParams["idleRotationYaw"] = 0.0f;
    def.customParams["idleRotationRoll"] = 0.0f;

    // Attack rotation
    def.customParams["attackRotationPitch"] = 30.0f;
    def.customParams["attackRotationYaw"] = -20.0f;
    def.customParams["attackRotationRoll"] = 0.0f;

    // Lunge rotation
    def.customParams["lungeRotationPitch"] = 10.0f;
    def.customParams["lungeRotationYaw"] = 0.0f;
    def.customParams["lungeRotationRoll"] = 0.0f;

    // Render
    def.customParams["renderColorR"] = 0.5f;
    def.customParams["renderColorG"] = 0.5f;
    def.customParams["renderColorB"] = 0.55f;

    // Force multipliers
    def.customParams["slashForceMultiplier"] = 0.5f;
    def.customParams["dashForceMultiplier"] = 1.0f;
    def.customParams["lungeForceMultiplier"] = 1.1f;

    // Slash timing (3x faster)
    def.customParams["slashWindupTime"] = 0.027f;
    def.customParams["slashActiveTime"] = 0.05f;
    def.customParams["slashRecoverTime"] = 0.033f;
    def.customParams["slashBaseDamage"] = 5.0f;
    def.customParams["slashSpeedDamageFactor"] = 18.0f;
    def.customParams["slashAngleDamageFactor"] = 12.0f;
    def.customParams["slashMaxDamage"] = 999.0f;
    def.customParams["slashBaseKnockback"] = 25.0f;
    def.customParams["slashSpeedKnockbackFactor"] = 3.0f;
    def.customParams["slashMaxKnockback"] = 600.0f;
    def.customParams["slashCooldown"] = 0.25f;

    // Lunge timing
    def.customParams["lungeWindupTime"] = 0.10f;
    def.customParams["lungeActiveTime"] = 0.20f;
    def.customParams["lungeRecoverTime"] = 0.12f;
    def.customParams["lungeForceSpikeMultiplier"] = 5.0f;
    def.customParams["lungeForceSpikeCenter"] = 0.5f;
    def.customParams["lungeForceSpikeWidth"] = 0.15f;
    def.customParams["lungeBaseDamage"] = 5.0f;
    def.customParams["lungeSpeedDamageFactor"] = 28.0f;
    def.customParams["lungeAngleDamageFactor"] = 12.0f;
    def.customParams["lungeMaxDamage"] = 999.0f;
    def.customParams["lungeBaseKnockback"] = 50.0f;
    def.customParams["lungeSpeedKnockbackFactor"] = 6.0f;
    def.customParams["lungeMaxKnockback"] = 600.0f;
    def.customParams["lungeCooldown"] = 0.5f;
    def.customParams["lungeDrag"] = 3.0f;

    // Shared
    def.customParams["damageTickInterval"] = 0.05f;
    def.customParams["worldHitSoundMinSpeed"] = 5.0f;
    def.customParams["worldHitSoundCooldown"] = 0.3f;
    def.customParams["knockbackSoundMinForce"] = 40.0f;

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

    def.modelPath = "assets/objects/weapons/mimita-rpg-v3.glb";
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
    def.customParams["knockbackHorizontalMultiplier"] = 1.0f;
    def.customParams["knockbackVerticalMultiplier"] = 1.0f;
    def.customParams["firingRecoilStrength"] = 30.0f;
    def.customParams["reserveAmmo"] = 1337.0f;

    // Projectile visual defaults
    def.customParams["projectileVisualTexture"] = 1.0f; // flag: use default path
    def.customParams["projectileVisualLength"] = 1.5f;
    def.customParams["projectileVisualRadius"] = 0.18f;
    def.customParams["projectileVisualScaleX"] = 1.0f;
    def.customParams["projectileVisualScaleY"] = 1.0f;
    def.customParams["projectileVisualScaleZ"] = 1.0f;
    def.customParams["projectileVisualRotationOffsetX"] = 0.0f;
    def.customParams["projectileVisualRotationOffsetY"] = 0.0f;
    def.customParams["projectileVisualRotationOffsetZ"] = 0.0f;
    def.customParams["projectileVisualTextureTilingU"] = 1.0f;
    def.customParams["projectileVisualTextureTilingV"] = 1.0f;
    def.customParams["projectileFillAlpha"] = 1.0f;
    def.customParams["projectileOutlineEnabled"] = 1.0f;
    def.customParams["projectileOutlineColorR"] = 1.0f;
    def.customParams["projectileOutlineColorG"] = 0.8f;
    def.customParams["projectileOutlineColorB"] = 0.2f;
    def.customParams["projectileOutlineAlpha"] = 0.4f;
    def.customParams["projectileOutlineScale"] = 1.15f;
    def.customParams["projectileGlowEnabled"] = 1.0f;
    def.customParams["projectileGlowColorR"] = 1.0f;
    def.customParams["projectileGlowColorG"] = 0.6f;
    def.customParams["projectileGlowColorB"] = 0.0f;
    def.customParams["projectileGlowAlpha"] = 0.15f;
    def.customParams["projectileGlowRadiusMultiplier"] = 3.0f;

    // Smoke effect defaults
    def.customParams["smokeEnabled"] = 1.0f;
    def.customParams["smokeEmissionRate"] = 50.0f;
    def.customParams["smokeParticlesPerEmission"] = 2.0f;
    def.customParams["smokeSpawnOffsetX"] = 0.0f;
    def.customParams["smokeSpawnOffsetY"] = 0.0f;
    def.customParams["smokeSpawnOffsetZ"] = -0.5f;
    def.customParams["smokeSpawnRadius"] = 0.08f;
    def.customParams["smokeInheritVelocity"] = -0.1f;
    def.customParams["smokeSpeed"] = 1.0f;
    def.customParams["smokeSpeedRandom"] = 0.5f;
    def.customParams["smokeSpreadDegrees"] = 25.0f;
    def.customParams["smokeLifetime"] = 1.2f;
    def.customParams["smokeLifetimeRandom"] = 0.25f;
    def.customParams["smokeSize"] = 0.25f;
    def.customParams["smokeEndSize"] = 0.8f;
    def.customParams["smokeSizeRandom"] = 0.1f;
    def.customParams["smokeGravity"] = 0.0f;
    def.customParams["smokeDrag"] = 1.5f;
    def.customParams["smokeColorR"] = 0.7f;
    def.customParams["smokeColorG"] = 0.7f;
    def.customParams["smokeColorB"] = 0.7f;
    def.customParams["smokeColorA"] = 0.8f;
    def.customParams["smokeEndColorR"] = 0.2f;
    def.customParams["smokeEndColorG"] = 0.2f;
    def.customParams["smokeEndColorB"] = 0.2f;
    def.customParams["smokeEndColorA"] = 0.0f;
    def.customParams["flightSoundVolumeDb"] = 15.0f;

    return def;
}

WeaponDefinition createGrenadeLauncherDefinition() {
    WeaponDefinition def;
    def.id = "grenade_launcher";
    def.displayName = "Grenade Launcher";
    def.slot = 8;

    def.modelPath = "assets/objects/weapons/mimita-nadelauncher-v1.glb";
    def.viewModelOffset = {0.0f, 0.0f, 0.0f};
    def.viewModelRotation = {0.0f, 0.0f, 0.0f};
    def.weaponScale = 1.0f;

    def.damage = 0.0f;
    def.headshotMultiplier = 1.0f;
    def.fireDelay = 1.0f;
    def.reloadTime = 1.5f;
    def.magazineSize = 4;
    def.pelletCount = 1;

    def.spread = 0.0f;
    def.recoil = 30.0f;
    def.projectileSpeed = 0.0f;
    def.projectileRadius = 0.25f;
    def.projectileLifetime = 0.0f;

    def.fireMode = WeaponFireMode::SemiAuto;
    def.behaviorType = WeaponBehaviorType::GrenadeLauncher;
    def.hitscan = false;
    def.usesPhysicsProjectile = false;

    def.soundShoot = "grenadelauncher/grenadelaunchershoot";
    def.soundReload = "grenadelauncher/grenadelauncherload";
    def.soundHit = "";
    def.soundDryFire = "ui/click";
    def.soundEquip = "";

    def.customParams["splashRadius"] = 8.0f;
    def.customParams["splashExponent"] = 2.0f;
    def.customParams["rocketDirectDamage"] = 150.0f;
    def.customParams["knockbackStrength"] = 160.0f;
    def.customParams["firingRecoilStrength"] = 20.0f;
    def.customParams["reserveAmmo"] = 1337.0f;

    // Projectile visual defaults
    def.customParams["projectileVisualTexture"] = 1.0f;
    def.customParams["projectileVisualLength"] = 1.8f;
    def.customParams["projectileVisualRadius"] = 0.28f;
    def.customParams["projectileVisualScaleX"] = 1.0f;
    def.customParams["projectileVisualScaleY"] = 1.0f;
    def.customParams["projectileVisualScaleZ"] = 1.0f;
    def.customParams["projectileVisualRotationOffsetX"] = 0.0f;
    def.customParams["projectileVisualRotationOffsetY"] = 0.0f;
    def.customParams["projectileVisualRotationOffsetZ"] = 0.0f;
    def.customParams["projectileVisualTextureTilingU"] = 1.0f;
    def.customParams["projectileVisualTextureTilingV"] = 1.0f;
    def.customParams["projectileFillAlpha"] = 1.0f;
    def.customParams["projectileOutlineEnabled"] = 1.0f;
    def.customParams["projectileOutlineColorR"] = 1.0f;
    def.customParams["projectileOutlineColorG"] = 0.8f;
    def.customParams["projectileOutlineColorB"] = 0.2f;
    def.customParams["projectileOutlineAlpha"] = 0.4f;
    def.customParams["projectileOutlineScale"] = 1.15f;
    def.customParams["projectileGlowEnabled"] = 1.0f;
    def.customParams["projectileGlowColorR"] = 1.0f;
    def.customParams["projectileGlowColorG"] = 0.6f;
    def.customParams["projectileGlowColorB"] = 0.0f;
    def.customParams["projectileGlowAlpha"] = 0.15f;
    def.customParams["projectileGlowRadiusMultiplier"] = 3.0f;

    // Spark effect defaults
    def.customParams["sparkEnabled"] = 1.0f;
    def.customParams["sparkEmissionRate"] = 45.0f;
    def.customParams["sparkParticlesPerEmission"] = 2.0f;
    def.customParams["sparkSpawnOffsetX"] = 0.0f;
    def.customParams["sparkSpawnOffsetY"] = 0.0f;
    def.customParams["sparkSpawnOffsetZ"] = -0.7f;
    def.customParams["sparkSpawnRadius"] = 0.04f;
    def.customParams["sparkInheritVelocity"] = 0.15f;
    def.customParams["sparkSpeed"] = 5.0f;
    def.customParams["sparkSpeedRandom"] = 2.0f;
    def.customParams["sparkSpreadDegrees"] = 35.0f;
    def.customParams["sparkLifetime"] = 0.35f;
    def.customParams["sparkLifetimeRandom"] = 0.15f;
    def.customParams["sparkSize"] = 0.05f;
    def.customParams["sparkEndSize"] = 0.01f;
    def.customParams["sparkSizeRandom"] = 0.005f;
    def.customParams["sparkGravity"] = 8.0f;
    def.customParams["sparkDrag"] = 0.2f;
    def.customParams["sparkColorR"] = 1.0f;
    def.customParams["sparkColorG"] = 0.75f;
    def.customParams["sparkColorB"] = 0.2f;
    def.customParams["sparkColorA"] = 1.0f;
    def.customParams["sparkEndColorR"] = 1.0f;
    def.customParams["sparkEndColorG"] = 0.1f;
    def.customParams["sparkEndColorB"] = 0.0f;
    def.customParams["sparkEndColorA"] = 0.0f;

    return def;
}

WeaponDefinition createAdminRevolverDefinition() {
    WeaponDefinition def = createRevolverDefinition();
    def.id = "admin_revolver";
    def.displayName = "Admin Revolver";
    def.slot = 9;
    def.fireDelay = 0.0001f;
    def.spread = 3.0f;
    def.magazineSize = 9999;
    def.customParams["reserveAmmo"] = 123456.0f;
    def.fireMode = WeaponFireMode::Automatic;
    def.poseId = "revolver";
    return def;
}

WeaponDefinition createHafsDefinition() {
    WeaponDefinition def;
    def.id = "hafs";
    def.displayName = "HAFS";
    def.slot = 10;
    def.modelPath = "assets/objects/weapons/mimita-hafs-v1.glb";
    def.behaviorType = WeaponBehaviorType::Hafs;
    def.damage = 80.0f;
    def.fireDelay = 0.0f;
    def.magazineSize = 0;
    def.soundShoot = "weapon/hafs/hafsswing";
    def.soundEquip = "weapon/hafs/hafsequip";
    def.poseId = "hafs";
    def.customParams["lungeSpeed"] = 25.0f;
    return def;
}

void registerBuiltinWeapons() {
    loadWeaponJsonConfig();
    registerWeaponFromJson(createRevolverDefinition());
    registerWeaponFromJson(createGodballDefinition());
    registerWeaponFromJson(createShotgunDefinition());
    registerWeaponFromJson(createSwordswordDefinition());
    registerWeaponFromJson(createOpRevolverDefinition());
    registerWeaponFromJson(createAa12Definition());
    registerWeaponFromJson(createRocketLauncherDefinition());
    registerWeaponFromJson(createGrenadeLauncherDefinition());
    registerWeaponFromJson(createAdminRevolverDefinition());
    registerWeaponFromJson(createHafsDefinition());
    Debug::log(Debug::Category::Weapons, "[WEAPON] Registered builtin weapons: revolver, godball, shotgun, swordsword, op_revolver, aa12, rocket_launcher, grenade_launcher, admin_revolver, hafs");
}

} // namespace WeaponData
