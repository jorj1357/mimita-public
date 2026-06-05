#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "input/input-state.h"

class Camera;
struct World;

enum class NpcActionType {
    Idle,
    Wander,
    Chase,
    Strafe,
    Evade,
    Jump,
    Dash,
    Attack
};

struct NpcDifficultyTuning {
    float reactionDelay = 0.45f;
    float actionInterval = 0.55f;
    float aggression = 0.35f;
    float dodgeChance = 0.10f;
    float aimErrorRadians = 0.35f;
    float movementPrecision = 0.45f;
    float awarenessRange = 18.0f;
    float prediction = 0.15f;
};

struct NpcSensorContext {
    bool hasTarget = false;
    bool visibleTarget = false;
    glm::vec3 targetPos{0.0f};
    glm::vec3 targetVel{0.0f};
    glm::vec3 toTarget{0.0f};
    glm::vec3 predictedTarget{0.0f};
    float targetDistance = 0.0f;
    glm::vec3 selfVel{0.0f};
    bool grounded = false;
    bool likelyBlocked = false;
};

struct NpcAction {
    NpcActionType type = NpcActionType::Idle;
    std::string name = "idle";
    glm::vec3 direction{0.0f};
    glm::vec3 pathTarget{0.0f};
    float score = 0.0f;
    bool jumpHeld = false;
    bool dashPressed = false;
    bool attackPressed = false;
};

class Npc {
public:
    std::uint32_t id = 0;
    float difficulty = 1.0f;
    NpcDifficultyTuning tuning;
    Player body;
    NpcSensorContext sensors;
    NpcAction chosenAction;

    float reactionTimer = 0.0f;
    float actionTimer = 0.0f;
    float wanderTimer = 0.0f;
    float dashCooldown = 0.0f;
    float attackCooldown = 0.0f;
    float lastTargetLogDistance = -1.0f;
    glm::vec3 wanderDirection{1.0f, 0.0f, 0.0f};
    glm::vec3 previousPosition{0.0f};
    unsigned int rngState = 1;

    Npc(std::uint32_t id, float difficulty, glm::vec3 spawn);
};

class NpcSystem {
public:
    void spawnPrototypeScene();
    void clear();
    void update(const World& world, const Player& player, float dt);
    void render(const Camera& camera) const;
    void drawDebug(const Camera& camera) const;
    std::vector<DebugVis::NpcDebugInfo> debugInfo() const;
    const std::vector<Npc>& all() const { return npcs; }

private:
    std::vector<Npc> npcs;
};
