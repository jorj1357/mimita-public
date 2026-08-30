// 08 29 2026, 00 00
/* purpose
* Declares SpyKnife client-side state with configurable box hitbox.
* Box collision runs at 60Hz tick rate, not per-frame.
* Force-based damage: speed + angle + directness determine damage.
* Does NOT own server-authoritative damage validation or packet transport.
*/

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

#include "physics/physics-types.h"

struct WeaponDefinition;
struct WeaponRuntime;
class Player;
class NpcSystem;
class Camera;
struct World;

enum class SpyKnifeAnimState : uint8_t {
    Idle = 0,
    Ready = 1,
    Swinging = 2,
    Returning = 3
};

struct SpyKnifeHitResult {
    uint32_t targetId = 0;
    bool isBackstab = false;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 victimPosition{0.0f};
};

struct SpyKnifeState {
    bool active = false;
    uint32_t swingTick = 0;
    uint32_t attackSequenceId = 0;

    SpyKnifeAnimState animState = SpyKnifeAnimState::Idle;

    Capsule previousBladeCapsule;
    bool hasPreviousBladeCapsule = false;

    uint32_t readyTargetId = 0;
    bool hasReadyTarget = false;

    std::unordered_map<uint32_t, float> hitCooldowns;
    std::unordered_map<uint32_t, bool> backstabSoundPlayed;
    std::vector<SpyKnifeHitResult> pendingRemoteHits;
};

namespace WeaponSpyKnife {

void startSwing(SpyKnifeState& state, const WeaponDefinition& def,
                Player& owner, Camera& camera);

void update(SpyKnifeState& state, const WeaponDefinition& def,
            WeaponRuntime& runtime, Player& owner,
            std::unordered_map<uint32_t, Player>* remoteNpcs,
            const Camera& camera,
            const World& world, float dt);

std::vector<SpyKnifeHitResult> collectRemoteHits(SpyKnifeState& state);

bool isBackstabGeometry(const Player& attacker, const Player& victim,
                        const WeaponDefinition& def);

} // namespace WeaponSpyKnife
