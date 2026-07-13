#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

class Camera;
class Player;
class NpcSystem;
struct WeaponDefinition;
struct WeaponRuntime;
struct World;

struct SwordswordState {
    enum class AttackState { Idle, SlashWindup, SlashActive, SlashRecover, LungeWindup, LungeActive, LungeRecover };

    AttackState state = AttackState::Idle;
    float stateTimer = 0.0f;
    float animTimer = 0.0f;

    // Weapon capsule previous positions (for velocity calculation)
    glm::vec3 prevWeaponGrip{0.0f};
    glm::vec3 prevWeaponTip{0.0f};

    // Sword motion
    glm::vec3 swordVelocity{0.0f};
    float swordSpeed = 0.0f;

    // Per-target damage cooldown
    std::unordered_map<uint32_t, float> hitCooldowns;

    // World hit sound cooldown
    float worldHitCooldown = 0.0f;

    // Attack spheres
    struct AttackSphere {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        float radius = 1.0f;
        int lifetime = 0;       // ticks remaining
        float minDamage = 10.0f;
        float knockbackStrength = 20.0f;
        glm::vec3 kbDir{0.0f, 0.0f, 1.0f};
    };
    std::vector<AttackSphere> spheres;
    std::unordered_map<uint32_t, float> sphereHitCooldowns;

    // Freeze held state
    bool freezeHeld = false;
    float freezeSphereTimer = 0.0f;

    // Debug
    struct DebugHit {
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        float damage = 0.0f;
        float knockback = 0.0f;
    };
    std::vector<DebugHit> debugHits;
};

namespace WeaponSwordsword {

void startSlash(SwordswordState& state, const WeaponDefinition& def, Player& owner);
void startLunge(SwordswordState& state, const WeaponDefinition& def, Player& owner);

void update(SwordswordState& state, const WeaponDefinition& def,
            WeaponRuntime& runtime, Player& owner,
            NpcSystem& npcs, const Camera& camera, const World& world, float dt);

void render(const Camera& camera, const SwordswordState& state, const WeaponDefinition& def, const glm::vec3& handPos);

void initResources(SwordswordState& state);

} // namespace WeaponSwordsword
