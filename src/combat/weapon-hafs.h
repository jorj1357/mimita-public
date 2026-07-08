#pragma once

#include <cstdint>
#include <cstdio>

// Global HAFS debug flag — toggle via `hafsdebug` command
extern bool gHafsDebug;
#include <unordered_map>
#include <glm/glm.hpp>

struct WeaponDefinition;
struct WeaponRuntime;
class Player;
class NpcSystem;
class Camera;
struct World;

struct HafsState {
    enum class AttackType { None, Slash, Lunge };

    AttackType currentAttack = AttackType::None;
    float attackTimer = 0.0f;
    float attackDuration = 0.25f;
    glm::vec3 attackForward{0.0f, 1.0f, 0.0f};

    // Sword physics for inertia
    glm::vec3 prevSwordTip{0.0f};
    glm::vec3 prevSwordGrip{0.0f};
    glm::vec3 swordVelocity{0.0f};
    float swordAngularVelocity = 0.0f;  // degrees/sec
    float swordSpeed = 0.0f;

    // Continuous collision — per-target cooldown
    std::unordered_map<uint32_t, float> hitCooldowns;

    // Bullet blocking
    int blockedBulletsThisFrame = 0;

    // Debug stats
    float impactForce = 0.0f;
    float lastDamage = 0.0f;
    float lastKnockback = 0.0f;
    int collisionCount = 0;
};

namespace WeaponHafs {

void startSlash(HafsState& state, const WeaponDefinition& def, Player& owner);
void startLunge(HafsState& state, const WeaponDefinition& def, Player& owner);

void update(HafsState& state, const WeaponDefinition& def,
            WeaponRuntime& runtime, Player& owner,
            NpcSystem& npcs, const Camera& camera,
            const World& world, float dt);

void onBulletBlocked(HafsState& state, Player& owner);

} // namespace WeaponHafs
