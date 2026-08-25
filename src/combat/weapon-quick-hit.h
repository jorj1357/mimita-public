// 08 25 2026, 00 00
/* purpose
* Declares QuickHit client-side state and per-frame update for the physical punch weapon.
* Owns input routing, attack state machine, sound restart, pose snap, and capsule computation.
* Does NOT own server-authoritative damage, swept collision validation, or episode batching.
* Does NOT own packet transport, reliable gameplay events, or server rewind compensation.
* Does NOT render remote player capsules or replicate hit effects over the network.
*/

#pragma once

#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>

#include "physics/physics-types.h"

struct WeaponDefinition;
struct WeaponRuntime;
class Player;
class NpcSystem;
class Camera;
struct World;

struct QuickHitState {
    bool active = false;
    uint32_t activeTicksRemaining = 0;
    uint32_t visualReturnTicksRemaining = 0;
    uint32_t attackSequenceId = 0;

    glm::vec3 attackForward{0.0f, 1.0f, 0.0f};

    Capsule previousArmCapsule;
    Capsule currentArmCapsule;
    bool hasPreviousCapsule = false;

    std::unordered_map<uint32_t, float> hitCooldowns;
};

namespace WeaponQuickHit {

void startAttack(QuickHitState& state, const WeaponDefinition& def,
                 Player& owner, Camera& camera);

void update(QuickHitState& state, const WeaponDefinition& def,
            WeaponRuntime& runtime, Player& owner,
            NpcSystem& npcs, const Camera& camera,
            const World& world, float dt);

Capsule computeArmCapsule(const Player& owner, const WeaponDefinition& def);

} // namespace WeaponQuickHit
