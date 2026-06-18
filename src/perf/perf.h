#pragma once

#include <cstdint>
#include <cstdio>
#include <chrono>

struct PerfTimes {
    double input = 0.0;
    double physics = 0.0;
    double collision = 0.0;
    double movement = 0.0;
    double npcAi = 0.0;
    double npcUpdate = 0.0;
    double npcSpawn = 0.0;
    double npcCombat = 0.0;
    double npcPathfinding = 0.0;
    double npcCollision = 0.0;
    double npcRender = 0.0;
    double weapons = 0.0;
    double combat = 0.0;
    double particles = 0.0;
    double blood = 0.0;
    double audio = 0.0;
    double replay = 0.0;
    double networking = 0.0;
    double ui = 0.0;
    double rendering = 0.0;
    double total = 0.0;
};

struct SpikeInfo {
    double frameTimeMs = 0.0;
    double worstSubsystemMs = 0.0;
    char subsystemName[32] = {};
    int npcCount = 0;
    int frameNumber = 0;
    double replayMemoryMb = 0.0;
};

struct PerfState {
    bool showPerfReport = false;
    bool showGraph = false;
    bool showNpcPerf = false;
    bool showMemory = false;
    bool showSpikes = false;
    bool renderStats = false;
    bool allocAudit = false;
    bool audioAudit = false;

    int preset = 0; // 0=default, 1=low, 2=medium, 3=high

    // Subsystem times for current frame
    PerfTimes current;

    // Rolling average frame time for spike detection
    double avgFrameTimeMs = 0.0;
    double avgFrameCount = 0.0;

    // Spike detection
    SpikeInfo lastSpike;

    // Frame history for graph (mirrored from FramePacer)
    float frameHistory[300] = {};
    int frameHistoryCount = 0;

    // Benchmark
    bool benchmarkRunning = false;
    double benchmarkStartWall = 0.0;
    double benchmarkDuration = 60.0;
    double benchmarkFrameTimes[3600] = {};
    int benchmarkFrameCount = 0;

    // Stress test
    bool stressRunning = false;
    bool combatTestRunning = false;
    int stressTotalNpcs = 0;
    double stressTimer = 0.0;

    // External counters (set by main loop)
    int drawCalls = 0;
    int triangles = 0;
    int playerCount = 0;
    int npcCount = 0;
    int bloodCount = 0;
    int particleCount = 0;
    int projectileCount = 0;

    // Network
    double netBytesIn = 0.0;
    double netBytesOut = 0.0;
    double snapshotBuildMs = 0.0;
    double serializeMs = 0.0;
    double receiveMs = 0.0;

    // Memory
    double replayMemoryMb = 0.0;

    // Allocations
    int allocationsThisFrame = 0;
    int totalAllocations = 0;

    // Audio audit
    bool soundLoadsDetected = false;
    int soundLoadCount = 0;

    // Frame counter
    int frameNumber = 0;

    // Game elapsed time (seconds)
    double gameTime = 0.0;

    // Memory tracking totals
    int totalNpcsSpawned = 0;
    int totalNpcsDestroyed = 0;
    float peakNpcCount = 0.0f;
    int effectCount = 0;
    int audioSourceCount = 0;
    int corpseCount = 0;
};

namespace Perf {

PerfState& state();

void beginFrame();
void endFrame();

// Render all active overlays (call inside uiBeginFrame/uiEndFrame)
void renderOverlay();

// Scoped timer for manual subsystem measurement
struct ScopedTimer {
    const char* name;
    uint64_t startUs;
    ScopedTimer(const char* n);
    ~ScopedTimer();
};

// Add a timed subsystem measurement in ms
void addTime(const char* name, double ms);

// Spike detection
void detectSpike(double currentFrameMs);

// --- Commands ---
void togglePerfReport();
void toggleGraph();
void toggleNpcPerf();
void toggleMemory();
void toggleSpikes();
void toggleRenderStats();
void toggleAllocAudit();
void toggleAudioAudit();
void setPreset(int p);
void startBenchmark(double seconds);
void startStress(int npcTarget);
void startCombatTest();
void exportReport(const char* path);
void printSuggestions();

// Preset application
void applyPreset(int p);

// Suggester
void generateSuggestions(char* buf, int bufSize);

} // namespace Perf

void registerPerfCommands();
