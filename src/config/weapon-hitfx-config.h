// 08 14 2026, 00 10
/* purpose
* Declares the WeaponHitFxConfig singleton and per-weapon hit-FX config structs.
* Provides JSON-driven, hot-reloadable settings loaded from config/weapon_hitfx.json for force, debris, blood, sound, presentation, and explosion bursts.
* Used by combat hit effects and explosion FX to fetch weapon-specific behavior.
* Does NOT spawn or render effects.
* Does NOT apply damage or knockback.
*/
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct WeaponHitFxDebrisConfig {
    bool enabled = true;
    int count = 12;
    float countForceScale = 4.0f;
    float speed = 3.0f;
    float speedForceScale = 15.0f;
    float lifetime = 0.5f;
    float lifetimeForceScale = 0.15f;
    float size = 0.8f;
    float sizeForceScale = 1.0f;
    float endSize = 4.0f;
    float endSizeForceScale = 12.0f;
    float gravity = 15.0f;
    glm::vec3 color{0.42f, 0.40f, 0.38f};
};

struct WeaponHitFxBloodConfig {
    bool enabled = true;
    int particleCount = 12;
    float particleCountForceScale = 100.0f;
    float bloodConeDegrees = 15.0f;
    float bloodConeForceScale = 5.0f;
    int debrisCount = 8;
    float debrisCountForceScale = 50.0f;
    float debrisConeDegrees = 35.0f;
    float debrisConeForceScale = 25.0f;
    float baseSpeed = 6.0f;
    float speedForceScale = 10.0f;
    float baseLifetime = 2.5f;
    float lifetimeForceScale = 1.0f;
    int decalCount = 8;
    float decalCountForceScale = 0.3f;
    float decalRadius = 0.25f;
    float decalRadiusForceScale = 0.022f;
    float decalMaxRadius = 4.5f;
    float decalLifetime = 60.0f;
    int maxBloodParticles = 512;
    int maxBloodDecals = 256;
};

struct WeaponHitFxSoundConfig {
    bool enabled = true;
    float baseVolume = 1.2f;
    float volumeSeverityScale = 0.6f;
    float volumeNearFactor = 1.0f;
    float pitchBase = 1.15f;
    float pitchSeverityScale = -0.3f;
    float pitchMin = 0.25f;
    float pitchMax = 3.0f;
    float nearDistance = 5.0f;
};

struct WeaponHitFxPresentationConfig {
    bool enabled = true;
    bool hitmarker = true;
    bool damageNumber = true;
    bool hitSound = true;
    bool selfDamageFeedback = false;
};

struct WeaponHitFxForceConfig {
    bool enabled = true;
    float minForce = 0.1f;
    float maxForce = 5.0f;
    float weaponMultiplier = 1.0f;
    bool angleEnabled = true;
    bool momentumEnabled = true;
};

struct WeaponHitFxExplosionBurstSphere {
    bool enabled = true;
    float startRadius = 0.5f;
    float endRadius = 8.0f;
    int lifetimeTicks = 15;
    glm::vec3 startColor{1.0f, 0.6f, 0.1f};
    glm::vec3 endColor{0.8f, 0.2f, 0.0f};
    float alphaStart = 1.0f;
    float alphaEnd = 0.0f;
    float brightnessStart = 3.0f;
    float brightnessEnd = 0.0f;
};

struct WeaponHitFxExplosionBurstSmoke {
    bool enabled = true;
    int count = 12;
    float lifetime = 1.8f;
    float size = 0.5f;
    float endSize = 2.0f;
    glm::vec3 color{0.3f, 0.3f, 0.3f};
    float alpha = 0.7f;
    float speed = 6.0f;
    float spread = 2.0f;
    float upwardBias = 2.0f;
};

struct WeaponHitFxExplosionBurstConfig {
    bool enabled = true;
    bool muzzleFlash = true;
    bool debris = true;
    bool impactTick = true;
    bool hitBurst = true;
    WeaponHitFxExplosionBurstSphere sphere;
    WeaponHitFxExplosionBurstSmoke smoke;
};

struct WeaponHitFxPerWeapon {
    WeaponHitFxForceConfig hitForce;
    WeaponHitFxDebrisConfig debris;
    WeaponHitFxBloodConfig blood;
    WeaponHitFxSoundConfig sound;
    WeaponHitFxPresentationConfig presentation;
    WeaponHitFxExplosionBurstConfig explosionBurst;
};

class WeaponHitFxConfig {
public:
    static WeaponHitFxConfig& instance();

    bool load(const std::string& path = "config/weapon_hitfx.json");
    bool pollReload();

    const WeaponHitFxForceConfig& defaultForce() const { return mDefaults.hitForce; }
    const WeaponHitFxDebrisConfig& defaultDebris() const { return mDefaults.debris; }
    const WeaponHitFxBloodConfig& defaultBlood() const { return mDefaults.blood; }
    const WeaponHitFxSoundConfig& defaultSound() const { return mDefaults.sound; }

    const WeaponHitFxForceConfig& forceFor(const std::string& weaponId) const;
    const WeaponHitFxDebrisConfig& debrisFor(const std::string& weaponId) const;
    const WeaponHitFxBloodConfig& bloodFor(const std::string& weaponId) const;
    const WeaponHitFxSoundConfig& soundFor(const std::string& weaponId) const;
    const WeaponHitFxPresentationConfig& presentationFor(const std::string& weaponId) const;
    const WeaponHitFxExplosionBurstConfig& explosionBurstFor(const std::string& weaponId) const;

private:
    WeaponHitFxConfig() = default;

    struct Config {
        WeaponHitFxForceConfig hitForce;
        WeaponHitFxDebrisConfig debris;
        WeaponHitFxBloodConfig blood;
        WeaponHitFxSoundConfig sound;
        WeaponHitFxPresentationConfig presentation;
    };

    Config mDefaults;
    std::unordered_map<std::string, WeaponHitFxPerWeapon> mPerWeapon;
    std::string mPath = "config/weapon_hitfx.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
