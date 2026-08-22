// 08 09 2026, 14 30
/* purpose
* Declares the Npc type, its difficulty tuning, sensor context, and the
* NpcSystem container that updates all NPCs. Holds per-NPC facing-mode state
* (aim-at-target vs face-movement) and the smoothed gun facing used by combat.
* Does NOT implement AI decisions, state machine, movement, or firing logic.
* Does NOT own server-side NPC simulation (see src/network/server-npcs.cpp).
*/

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
    float turnSpeed = 720.0f;  // degrees per second (0:180, 10:1080)
};

struct NpcSensorContext {
    bool hasTarget = false;
    glm::vec3 targetPos{0.0f};
    glm::vec3 targetVel{0.0f};
    glm::vec3 toTarget{0.0f};
    glm::vec3 predictedTarget{0.0f};
    float targetDistance = 0.0f;
    glm::vec3 selfVel{0.0f};
    bool touchFloor = false;
    float time = 0.0f;
};

class Npc {
public:
    std::uint32_t id = 0;
    // Lifecycle counter: increments once per respawn so clients can detect a
    // new life (same pattern as player transformEpoch). Starts at 1 so the
    // first snapshot is a nonzero epoch; wrapped in respawnServerNpc so it
    // never hits 0 (clients ignore epoch 0).
    uint16_t transformEpoch = 1;
    std::string avatarName;
    float difficulty = 1.0f;
    NpcDifficultyTuning tuning;
    Player body;
    NpcSensorContext sensors;
    NpcStateMachine stateMachine;

    float dashCooldown = 0.0f;
    float downDashCooldown = 0.0f;
    bool dashCommandConsumed = false;
    float attackCooldown = 0.0f;
    float weaponSwitchCooldown = 0.0f;
    float lastTargetLogDistance = -1.0f;
    glm::vec3 previousPosition{0.0f};
    glm::vec2 lastMoveInput{0.0f};
    glm::vec3 lastAcceleration{0.0f};
    float lastGravityDelta = 0.0f;
    float lastFrictionDelta = 0.0f;
    float lastFinalSpeed = 0.0f;
    float hitReactionTimer = 0.0f;
    float wakeupTimer = 0.0f;  // seconds of spawn delay before NPC becomes active

    // Bomb tag behavior flags
    bool bombTagActive = false;
    bool bombTagHasBomb = false;
    glm::vec3 bombTagChaseTarget{0.0f};
    glm::vec3 bombTagFleeFrom{0.0f};

    // Position history for difficulty-based reaction delay
    struct PositionSample { glm::vec3 pos{0.0f}; glm::vec3 vel{0.0f}; float time = 0.0f; };
    static constexpr int MAX_HISTORY_SAMPLES = 120;
    PositionSample posRing[MAX_HISTORY_SAMPLES]{};
    int posRingHead = 0;
    int posRingCount = 0;

    unsigned int rngState = 1;

    // Aim timing
    float aimTimer = 0.0f;
    float reactionTimer = 0.0f;

    // Micro-movement noise
    float moveNoiseTimer = 0.0f;
    glm::vec2 moveOffset{0.0f};

    // Firing behavior (human-like variability)
    float fireRhythmOffset = 0.0f;
    float fireAggressionBias = 0.0f;
    float timeSinceLastShot = 0.0f;
    static constexpr float MAX_FIRE_DELAY = 0.5f;

    // Training mode: 0=idle, 1=flee, 2=attack (normal AI)
    int trainingMode = 2;

    // Cached line-of-sight result updated each frame in updateOneNpc
    bool cachedLoSBlocked = false;
    int losTickCounter = 0;

    // Smoothed facing direction for turn speed limiting
    glm::vec3 currentFacing{1.0f, 0.0f, 0.0f};

    // Facing mode: which way the gun/model points. Aim mode (default, dominant)
    // locks onto the target; move mode briefly faces travel direction. The mode
    // timer decides when to switch, independent of grounded/airborne state.
    bool facingTargetMode = true;
    float facingModeTimer = 0.0f;

    // The actual fired shot from the last successful tryFire. The server
    // broadcast reads these so the remote tracer goes exactly where the damage
    // ray went (look direction == shoot direction == bullet endpoint).
    glm::vec3 lastShotOrigin{0.0f};
    glm::vec3 lastShotEnd{0.0f};
    glm::vec3 lastShotNormal{0.0f, 0.0f, 1.0f};
    bool hasLastShot = false;
    bool lastShotHitWorld = false;

    // Set true on the tick this NPC actually fired, so the server broadcasts
    // the shot exactly once (no per-frame shot/sound spam).
    bool justFired = false;

    // Per-pellet data for multi-pellet weapons (shotgun).
    // Stored after tryFire so broadcastNpcFiring can send PelletBlastEventPacket.
    struct PelletData {
        glm::vec3 hitPos{0.0f};
        glm::vec3 hitNormal{0.0f, 0.0f, 1.0f};
        bool hitEntity = false;
        bool hitWorld = false;
    };
    static constexpr int MAX_NPC_PELLETS = 16;
    PelletData pelletResults[MAX_NPC_PELLETS]{};
    int pelletResultCount = 0;
    uint32_t pelletSpreadSeed = 0;
    uint32_t shotSerialCounter = 1;  // unique per-shot serial for pellet blast dedup

    Npc(std::uint32_t id, float difficulty, glm::vec3 spawn,
        const std::string& weaponId = "revolver");
};

struct CombatSoundEvent {
    glm::vec3 position{0.0f};
    float time = 0.0f;
    float intensity = 1.0f;
};

class NpcSystem {
public:
    void spawnPrototypeScene();
    void clear();
    void destroySelected(const std::vector<std::uint32_t>& ids);
    void destroyAll();
    void update(const World& world, Player& player, float dt);
    // Server hook: simulate a single NPC against a specific player target so
    // each online NPC can target its own nearest live player (instead of a
    // single shared mirror). Advances that NPC's AI, movement, and firing.
    void updateOneWithTarget(uint32_t npcId, const World& world, Player& player, float dt);
    void render(const Camera& camera) const;
    void drawDebug(const Camera& camera) const;
    std::vector<DebugVis::NpcDebugInfo> debugInfo() const;
    const std::vector<Npc>& all() const { return npcs; }
    std::vector<Npc>& all() { return npcs; }

    void spawnNpc(float difficulty);
    void spawnNpc(uint32_t id, float difficulty, glm::vec3 spawnPos);
    uint32_t nextNpcId() { return nextId++; }

    void setGlobalDifficulty(float d);
    float globalDifficulty() const { return globalDifficulty_; }

    // Register a combat sound for NPC hearing system
    void notifyCombatSound(glm::vec3 position, float intensity = 1.0f);

    // Query most recent combat sound near a position
    bool recentCombatSoundNear(glm::vec3 pos, float maxAge, float maxDist, glm::vec3& outSource) const;

    // NPC communication: check if another NPC is near a position (avoids clustering)
    bool isNpcNear(glm::vec3 pos, float radius, uint32_t excludeId) const;

    // NPC communication: find nearest other NPC to a position
    float nearestOtherNpc(glm::vec3 pos, uint32_t excludeId, glm::vec3& outPos) const;

    // NPC communication: count how many NPCs are targeting the same position
    int npcCountNear(glm::vec3 pos, float radius) const;

private:
    std::vector<Npc> npcs;
    uint32_t nextId = 100;
    float globalDifficulty_ = -1.0f;
    float currentTime = 0.0f;

    // Ring buffer of recent combat sounds for NPC hearing
    static constexpr int MAX_HEARD_SOUNDS = 16;
    CombatSoundEvent heardSounds[MAX_HEARD_SOUNDS]{};
    int heardSoundHead = 0;
    int heardSoundCount = 0;

    void updateOneNpc(Npc& npc, const World& world, Player& player, float dt);
};
