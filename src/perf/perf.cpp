#include "perf/perf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

#include "video/frame-pacer.h"
#include "gui/ui-system.h"
#include "debug/debug-log.h"

extern FramePacer gFramePacer;

PerfState gState;

static uint64_t nowUs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()).count();
}

PerfState& Perf::state() { return gState; }

void Perf::beginFrame()
{
    gState.allocationsThisFrame = 0;
    gState.current = PerfTimes{};
}

void Perf::endFrame()
{
    PerfState& s = gState;

    s.frameNumber++;

    // Track elapsed game time
    s.gameTime += gFramePacer.dt();

    // Rolling average for spike detection
    float currentMs = gFramePacer.frameTimeMs();
    s.avgFrameCount += 1.0;
    s.avgFrameTimeMs += (currentMs - s.avgFrameTimeMs) / std::min(s.avgFrameCount, 100.0);

    // Spike detection
    if (s.showSpikes && currentMs > s.avgFrameTimeMs * 2.0 && s.avgFrameCount > 10.0)
        detectSpike(currentMs);

    // Copy frame history from FramePacer
    s.frameHistoryCount = gFramePacer.historyCount();
    for (int i = 0; i < s.frameHistoryCount && i < 300; ++i)
        s.frameHistory[i] = gFramePacer.historyMs(i);

    // Aggregate frametime for benchmarks
    if (s.benchmarkRunning && s.benchmarkFrameCount < 3600)
    {
        s.benchmarkFrameTimes[s.benchmarkFrameCount++] = gFramePacer.frameTimeMs();
        double elapsed = (double)(nowUs() - (uint64_t)s.benchmarkStartWall) / 1000000.0;
        if (elapsed >= s.benchmarkDuration)
        {
            s.benchmarkRunning = false;
            char path[256];
            snprintf(path, sizeof(path), "benchmark_report.txt");
            exportReport(path);
            Debug::log(Debug::Category::General, "[PERF] Benchmark completed after %.1f seconds, report: %s",
                       elapsed, path);
        }
    }

    // Stress test phase advancement
    if (s.stressRunning)
    {
        s.stressTimer += gFramePacer.dt();
    }
}

void Perf::addTime(const char* name, double ms)
{
    PerfTimes& t = gState.current;
    if (std::strcmp(name, "Input") == 0)           t.input += ms;
    else if (std::strcmp(name, "Physics") == 0 ||
             std::strcmp(name, "Simulation") == 0) t.physics += ms;
    else if (std::strcmp(name, "Collision") == 0)  t.collision += ms;
    else if (std::strcmp(name, "Movement") == 0)   t.movement += ms;
    else if (std::strcmp(name, "AI") == 0)         t.npcAi += ms;
    else if (std::strcmp(name, "NpcUpdate") == 0)  t.npcUpdate += ms;
    else if (std::strcmp(name, "NpcSpawn") == 0)   t.npcSpawn += ms;
    else if (std::strcmp(name, "NpcCombat") == 0)  t.npcCombat += ms;
    else if (std::strcmp(name, "NpcPathfinding") == 0) t.npcPathfinding += ms;
    else if (std::strcmp(name, "NpcCollision") == 0)   t.npcCollision += ms;
    else if (std::strcmp(name, "NpcRender") == 0)      t.npcRender += ms;
    else if (std::strcmp(name, "Weapons") == 0)    t.weapons += ms;
    else if (std::strcmp(name, "Combat") == 0)     t.combat += ms;
    else if (std::strcmp(name, "Particles") == 0)  t.particles += ms;
    else if (std::strcmp(name, "Blood") == 0)      t.blood += ms;
    else if (std::strcmp(name, "Audio") == 0)      t.audio += ms;
    else if (std::strcmp(name, "Replay") == 0)     t.replay += ms;
    else if (std::strcmp(name, "Networking") == 0) t.networking += ms;
    else if (std::strcmp(name, "UI") == 0)         t.ui += ms;
    else if (std::strcmp(name, "Rendering") == 0)  t.rendering += ms;
    t.total += ms;
}

Perf::ScopedTimer::ScopedTimer(const char* n)
    : name(n), startUs(nowUs())
{
}

Perf::ScopedTimer::~ScopedTimer()
{
    double ms = (double)(nowUs() - startUs) / 1000.0;
    addTime(name, ms);
}

void Perf::togglePerfReport()
{
    gState.showPerfReport = !gState.showPerfReport;
    Debug::log(Debug::Category::General, "[PERF] perf_report=%s",
               gState.showPerfReport ? "ON" : "OFF");
}

void Perf::toggleGraph()
{
    gState.showGraph = !gState.showGraph;
    Debug::log(Debug::Category::General, "[PERF] perf_graph=%s",
               gState.showGraph ? "ON" : "OFF");
}

void Perf::toggleNpcPerf()
{
    gState.showNpcPerf = !gState.showNpcPerf;
    Debug::log(Debug::Category::General, "[PERF] perf_npc=%s",
               gState.showNpcPerf ? "ON" : "OFF");
}

void Perf::toggleMemory()
{
    gState.showMemory = !gState.showMemory;
    Debug::log(Debug::Category::General, "[PERF] perf_memory=%s",
               gState.showMemory ? "ON" : "OFF");
}

void Perf::toggleSpikes()
{
    gState.showSpikes = !gState.showSpikes;
    Debug::log(Debug::Category::General, "[PERF] perf_spikes=%s",
               gState.showSpikes ? "ON" : "OFF");
}

void Perf::toggleRenderStats()
{
    gState.renderStats = !gState.renderStats;
    Debug::log(Debug::Category::General, "[PERF] render_stats=%s",
               gState.renderStats ? "ON" : "OFF");
}

void Perf::toggleAllocAudit()
{
    gState.allocAudit = !gState.allocAudit;
    gState.totalAllocations = 0;
    Debug::log(Debug::Category::General, "[PERF] allocation_audit=%s",
               gState.allocAudit ? "ON" : "OFF");
}

void Perf::toggleAudioAudit()
{
    gState.audioAudit = !gState.audioAudit;
    Debug::log(Debug::Category::General, "[PERF] audio_audit=%s",
               gState.audioAudit ? "ON" : "OFF");
}

void Perf::setPreset(int p)
{
    if (p < 0 || p > 3) p = 0;
    gState.preset = p;
    applyPreset(p);
    static const char* names[] = {"Default", "Low", "Medium", "High"};
    Debug::log(Debug::Category::General, "[PERF] preset=%s", names[p]);
}

void Perf::applyPreset(int p)
{
    (void)p;
    // Presets modify external config; implemented via config variables.
    // Low: reduce blood, particles, effects, disable debug/post effects.
    // Medium: moderate reduction.
    // High: full quality.
    // These are applied by the subsystems reading PerfState::preset.
}

void Perf::startBenchmark(double seconds)
{
    gState.benchmarkRunning = true;
    gState.benchmarkDuration = seconds;
    gState.benchmarkFrameCount = 0;
    gState.benchmarkStartWall = (double)nowUs();
    Debug::log(Debug::Category::General, "[PERF] Benchmark started for %.1f seconds", seconds);
}

void Perf::startStress(int npcTarget)
{
    gState.stressRunning = true;
    gState.stressTotalNpcs = npcTarget;
    gState.stressTimer = 0.0;
    Debug::log(Debug::Category::General, "[PERF] Stress test started, target NPCs: %d", npcTarget);
}

void Perf::startCombatTest()
{
    gState.combatTestRunning = true;
    gState.stressRunning = true;
    gState.stressTotalNpcs = 10;
    gState.stressTimer = 0.0;
    Debug::log(Debug::Category::General, "[PERF] Combat stress test started");
}


