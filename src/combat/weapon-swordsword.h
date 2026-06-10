#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

class Camera;
class Player;
class NpcSystem;
struct WeaponDefinition;
struct WeaponRuntime;

struct SwordswordState {
    enum class AttackType { None, Slash, Lunge };

    AttackType currentAttack = AttackType::None;
    float attackTimer = 0.0f;
    float attackDuration = 0.0f;
    glm::vec3 attackForward{0.0f, 1.0f, 0.0f};
    std::unordered_map<uint32_t, bool> hitTargets;

    float slashAngle = 0.0f;
    float lungeReach = 0.0f;

    glm::vec3 handPos{0.0f};
    glm::vec3 bladeEnd{0.0f};
    glm::vec3 bladeDirection{0.0f, 1.0f, 0.0f};
    float bladeLength = 1.5f;

    float swayTimer = 0.0f;
    float swayOffset = 0.0f;

    struct DebugHit {
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        bool hit = false;
    };
    std::vector<DebugHit> debugHits;
    glm::vec3 debugArcStart{0.0f};
    glm::vec3 debugArcEnd{0.0f};
    glm::vec3 debugTraceStart{0.0f};
    glm::vec3 debugTraceEnd{0.0f};
    float debugArcAngle = 0.0f;
    float debugArcRange = 0.0f;
};

namespace WeaponSwordsword {
    glm::vec3 getHandPosition(const Player& player);

    void update(SwordswordState& state, const WeaponDefinition& def,
                WeaponRuntime& runtime, Player& owner,
                const Camera& camera, NpcSystem& npcs, float dt);

    void startSlash(SwordswordState& state, const WeaponDefinition& def,
                    Player& owner, const Camera& camera);

    void startLunge(SwordswordState& state, const WeaponDefinition& def,
                    Player& owner, const Camera& camera);

    void render(const Camera& camera, const SwordswordState& state,
                const glm::vec3& handPos);

    void renderDebug(const Camera& camera, const SwordswordState& state,
                     const glm::vec3& handPos);
}
