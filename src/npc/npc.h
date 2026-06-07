#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "input/input-state.h"
#include "npc/npc-state-machine.h"

class Camera;
struct World;

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
    glm::vec3 targetPos{0.0f};
    glm::vec3 targetVel{0.0f};
    glm::vec3 toTarget{0.0f};
    glm::vec3 predictedTarget{0.0f};
    float targetDistance = 0.0f;
    glm::vec3 selfVel{0.0f};
    bool grounded = false;
};

class Npc {
public:
    std::uint32_t id = 0;
    float difficulty = 1.0f;
    NpcDifficultyTuning tuning;
    Player body;
    NpcSensorContext sensors;
    NpcStateMachine stateMachine;

    float dashCooldown = 0.0f;
    bool dashCommandConsumed = false;
    float attackCooldown = 0.0f;
    float lastTargetLogDistance = -1.0f;
    glm::vec3 previousPosition{0.0f};
    glm::vec2 lastMoveInput{0.0f};
    glm::vec3 lastAcceleration{0.0f};
    float lastGravityDelta = 0.0f;
    float lastFrictionDelta = 0.0f;
    float lastFinalSpeed = 0.0f;
    float hitReactionTimer = 0.0f;
    unsigned int rngState = 1;

    Npc(std::uint32_t id, float difficulty, glm::vec3 spawn);
};

class NpcSystem {
public:
    void spawnPrototypeScene();
    void clear();
    void destroySelected(const std::vector<std::uint32_t>& ids);
    void destroyAll();
    void update(const World& world, Player& player, float dt);
    void render(const Camera& camera) const;
    void drawDebug(const Camera& camera) const;
    std::vector<DebugVis::NpcDebugInfo> debugInfo() const;
    const std::vector<Npc>& all() const { return npcs; }
    std::vector<Npc>& all() { return npcs; }

    void spawnNpc(float difficulty, glm::vec3 spawnPos);
    uint32_t nextNpcId() { return nextId++; }

    void setGlobalDifficulty(float d);
    float globalDifficulty() const { return globalDifficulty_; }

private:
    std::vector<Npc> npcs;
    uint32_t nextId = 100;
    float globalDifficulty_ = -1.0f;

    void updateOneNpc(Npc& npc, const World& world, Player& player, float dt);
};
