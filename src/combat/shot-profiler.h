#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>

struct ShotProfiler {
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;

    // Top-level stages
    double aimMs = 0.0;
    double pelletGenMs = 0.0;
    double pelletLoopMs = 0.0;
    double damageMs = 0.0;
    double audioMs = 0.0;
    double animationMs = 0.0;

    // Collision
    double worldCollisionMs = 0.0;
    double npcCollisionMs = 0.0;
    double remotePlayerCollisionMs = 0.0;
    int collisionCalls = 0;
    int worldTriangleTests = 0;
    int npcBodyTests = 0;

    // HitFX sub-stages (within onHit)
    double impactSphereMs = 0.0;
    double bloodMs = 0.0;
    double debrisMs = 0.0;
    double damageNumberMs = 0.0;
    double hitBurstMs = 0.0;
    int hitFxCalls = 0;
    int objectsSpawnedByHitFx = 0;

    // EffectPart spawn
    double effectSpawnMs = 0.0;
    int effectsSpawned = 0;
    int poolLinearScans = 0;
    int poolHits = 0;

    // Replay
    double replayRecordMs = 0.0;
    int replayEventsCreated = 0;
    int replayVectorGrows = 0;

    // Tracers
    double tracerMs = 0.0;
    int tracersSpawned = 0;

    // Memory
    int totalMallocs = 0;
    int totalFrees = 0;
    int totalBytes = 0;
    int largestAllocBytes = 0;
    int vectorReallocs = 0;
    int stringAllocs = 0;

    // Pellets
    int totalPellets = 0;
    int npcHits = 0;
    int worldHits = 0;
    int misses = 0;

    // Weapon context
    std::string weaponName;
    int frameTick = 0;

    void reset(const std::string& weapon, int tick) {
        *this = ShotProfiler{};
        weaponName = weapon;
        frameTick = tick;
    }

    struct Scope {
        double* target;
        Time start;
        bool active;
        Scope(double* t) : target(t), active(t != nullptr) { if (active) start = Clock::now(); }
        ~Scope() { if (active) *target += std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
    };

    void print() const;
};

extern ShotProfiler* gShotProfiler;
