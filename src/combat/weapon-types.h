// 07 21 2026, 20 45
/* purpose
* Declares weapon definition, runtime, behavior, execution, and shot result data.
* Keeps weapon config ownership neutral so local, client, and server paths share metadata.
* Provides compact runtime fields needed by ammo, cooldown, and authoritative reconciliation.
* Does NOT implement weapon simulation, networking, rendering, or audio playback.
* Does NOT own JSON parsing, packet transport, or server damage authority.
* Does NOT specialize runtime state by client/server launch mode.
*/

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

class Camera;
class Player;
class NpcSystem;
struct World;

enum class WeaponBehaviorType {
    Hitscan,
    Projectile,
    Godball,
    Melee,
    Swordsword,
    RocketLauncher,
    GrenadeLauncher,
    Hafs,
    QuickHit,
    SpyKnife
};

enum class WeaponExecutionType {
    Hitscan,
    Projectile,
    PhysicalContact
};

enum class WeaponNetworkMode {
    Normal,
    ClientBatchedHitscan
};

inline WeaponExecutionType weaponExecutionTypeForBehavior(WeaponBehaviorType behavior)
{
    switch (behavior)
    {
    case WeaponBehaviorType::Hitscan:
        return WeaponExecutionType::Hitscan;
    case WeaponBehaviorType::RocketLauncher:
    case WeaponBehaviorType::GrenadeLauncher:
    case WeaponBehaviorType::Projectile:
        return WeaponExecutionType::Projectile;
    case WeaponBehaviorType::Godball:
    case WeaponBehaviorType::Swordsword:
    case WeaponBehaviorType::Melee:
    case WeaponBehaviorType::Hafs:
    case WeaponBehaviorType::QuickHit:
    case WeaponBehaviorType::SpyKnife:
        return WeaponExecutionType::PhysicalContact;
    }
    return WeaponExecutionType::Hitscan;
}

enum class WeaponFireMode {
    SemiAuto,
    Automatic,
    Charge
};

struct WeaponDefinition {
    std::string id;
    std::string displayName;
    int slot = 0;

    std::string modelPath;
    glm::vec3 viewModelOffset{0.0f};
    glm::vec3 viewModelRotation{0.0f};
    glm::vec3 attachmentOffset{0.0f};
    glm::vec3 attachmentRotation{0.0f};
    float weaponScale = 1.0f;

    float damage = 0.0f;
    float headshotMultiplier = 2.0f;
    float fireDelay = 0.1f;
    float reloadTime = 1.0f;
    int magazineSize = 6;
    int pelletCount = 1;

    float spread = 0.0f;
    float recoil = 0.0f;
    float projectileSpeed = 0.0f;
    float projectileRadius = 0.0f;
    float projectileLifetime = 5.0f;

    // ── Impulse / knockback (Source/TF2-style, hot reloadable) ───────────
    // Self-push (recoil impulse) on fire. 0 = fall back to the player setting.
    float shooterKnockback = 0.0f;
    // Vertical fraction of the self-push.
    float shooterKnockbackVertical = 0.0f;
    // Global scale on self-side impulses.
    float selfImpulseMultiplier = 1.0f;
    // Flat victim impulse per hit (added to the per-damage term).
    float victimKnockback = 0.0f;
    // Victim impulse per 1 damage dealt (higher damage => more knockback).
    float victimKnockbackPerDamage = 0.15f;
    // Vertical fraction of the victim impulse.
    float victimKnockbackVerticalFraction = 0.12f;
    // Scales victim impulse when the target is an enemy.
    float enemyImpulseMultiplier = 1.0f;

    WeaponFireMode fireMode = WeaponFireMode::SemiAuto;
    WeaponBehaviorType behaviorType = WeaponBehaviorType::Hitscan;
    WeaponExecutionType executionType = WeaponExecutionType::Hitscan;
    bool hitscan = true;
    bool usesPhysicsProjectile = false;
    WeaponNetworkMode networkMode = WeaponNetworkMode::Normal;

    std::string soundShoot;
    std::string soundReload;
    std::string soundHit;
    std::string soundDryFire;
    std::string soundEquip;
    std::string poseId;  // which weapon pose config to use (empty = use own id)

    float soundPitchVariation = 0.05f;   // ±5% pitch randomization per shot
    float soundVolumeVariation = 0.05f;  // ±5% volume randomization per shot

    glm::vec3 tint{1.0f};  // RGB multiplier for rendering (1,1,1 = no tint)

    float beamThickness = 0.0f;  // hitscan collision radius (0 = thin ray)
    // World/occlusion radius for hitscan (0 = thin world rays). Separate from
    // beamThickness so a thick entity-hit beam doesn't clip wall edges and
    // break the shotgun pellet pattern at the aim direction.
    float beamWorldThickness = 0.0f;
    bool weaponCollisionEnabled = true;
    bool restricted = false;  // admin/dev weapons — not granted in normal loadout

    std::unordered_map<std::string, float> customParams;

    bool validate() const;
};

struct WeaponRuntime {
    int currentAmmo = 0;
    int reserveAmmo = 0;
    int pendingReloadRounds = 0;

    float fireCooldown = 0.0f;
    float reloadTimer = 0.0f;
    float shootEffectTimer = 0.0f;
    float reloadBufferTimer = 0.0f;

    bool isReloading = false;
    bool isCharging = false;
    float chargeAmount = 0.0f;

    struct OpFireBatchState {
        uint32_t nextSequence = 1;
        uint32_t startTick = 0;
        uint32_t endTick = 0;
        uint32_t shotsFired = 0;
        uint64_t lastSendMs = 0;
        struct Hit {
            uint32_t targetId = 0;
            int16_t damage = 0;
            uint8_t bodyPart = 0;
            uint8_t relativeTick = 0;
        } hits[96]{};
        uint8_t hitCount = 0;
        bool active = false;
    } opFireBatch;

    uint32_t authoritativeStateRevision = 0;
    uint32_t authoritativeSpawnGeneration = 0;

    struct GodballState {
        uint32_t ballEntityId = 0;
        glm::vec3 ballPosition{0.0f};
        glm::vec3 ballVelocity{0.0f};
        bool ballSpawned = false;
        float overlapDamageTimer = 0.0f;
        std::unordered_map<uint32_t, float> targetCooldowns;
    } godball;

    std::unordered_map<std::string, float> customFloats;
    std::unordered_map<std::string, glm::vec3> customVec3s;

    void reset(const WeaponDefinition& def);
};



struct DamageContext {
    float baseDamage;
    float distance;
    float angleFactor;
    std::string bodyPart;
    glm::vec3 hitPosition;
    glm::vec3 hitNormal;
    glm::vec3 shotDirection;
    uint32_t shooterId;
    std::string shooterName;
};

enum class AudioEventType {
    Shoot,
    Reload,
    DryFire,
    Equip,
    HitWorld,
    HitEntity
};

struct RevolverShotResult {
    bool fired = false;
    bool hitEntity = false;
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
    // The actual fired direction (aim.direction / camera forward in camforward
    // mode), kept separate from the hit geometry so network requests always
    // send the real aim direction instead of one derived from a contact point.
    glm::vec3 direction{0.0f};
    std::string bodyPart;
    float damage = 0.0f;
    uint32_t targetId = 0;
    bool targetIsRemotePlayer = false;
    bool targetIsRemoteNpc = false;
    bool hitWorld = false;
    glm::vec3 hitNormal{0.0f};
    glm::vec3 knockbackImpulse{0.0f};
    bool autoReloadTriggered = false;  // set when fire() starts auto-reload on empty
};
