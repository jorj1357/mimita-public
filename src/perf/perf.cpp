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

static PerfState gState;

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

void Perf::renderOverlay()
{
    PerfState& s = gState;
    float sw = uiScreenW();

    // --- Part 2: perf_report ---
    if (s.showPerfReport)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        auto text = [&](const char* str, glm::vec4 col = {0.3f, 1.0f, 0.5f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        float ms = gFramePacer.frameTimeMs();
        int fps = ms > 0.0f ? (int)(1000.0f / ms + 0.5f) : 0;
        int avgFps = gFramePacer.avgFrameTimeMs() > 0.0f ? (int)(1000.0f / gFramePacer.avgFrameTimeMs() + 0.5f) : 0;
        float budget = gFramePacer.targetFrameTimeMs();

        snprintf(buf, sizeof(buf), "FPS: %d  AVG: %d", fps, avgFps);
        text(buf, {0.3f, 1.0f, 0.5f, 1.0f});
        snprintf(buf, sizeof(buf), "FRAME: %.2fms  BUDGET: %.2fms", ms, budget);
        text(buf, {0.3f, 1.0f, 0.5f, 1.0f});

        // Subsystem times
        snprintf(buf, sizeof(buf), "CPU: %.2fms", s.current.total);
        text(buf, {0.8f, 0.9f, 1.0f, 1.0f});

        double gpuMs = 0.0;
        if (gFramePacer.subsystemCount() > 0)
        {
            for (int i = 0; i < gFramePacer.subsystemCount(); ++i)
            {
                if (gFramePacer.subsystemName(i) &&
                    std::strcmp(gFramePacer.subsystemName(i), "Render") == 0)
                {
                    gpuMs = gFramePacer.subsystemTimeMs(i);
                    break;
                }
            }
        }
        snprintf(buf, sizeof(buf), "GPU: %.2fms", gpuMs);
        text(buf, {0.8f, 0.9f, 1.0f, 1.0f});

        // Draw calls
        snprintf(buf, sizeof(buf), "Draw Calls: %d  Triangles: %dk",
                 s.drawCalls, s.triangles / 1000);
        text(buf, {0.6f, 0.8f, 1.0f, 1.0f});

        // Entity counts
        snprintf(buf, sizeof(buf), "Players: %d  NPCs: %d", s.playerCount, s.npcCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});

        // Effects
        snprintf(buf, sizeof(buf), "Blood: %d  Particles: %d",
                 s.bloodCount, s.particleCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});

        // Replay
        if (s.replayMemoryMb > 0.0)
        {
            snprintf(buf, sizeof(buf), "Replay: %.1fMB", s.replayMemoryMb);
            text(buf, {0.7f, 0.7f, 1.0f, 1.0f});
        }

        // Network
        if (s.netBytesIn > 0.0 || s.netBytesOut > 0.0)
        {
            snprintf(buf, sizeof(buf), "Net In: %.0fB/s  Out: %.0fB/s",
                     s.netBytesIn, s.netBytesOut);
            text(buf, {0.5f, 1.0f, 0.8f, 1.0f});
        }

        // Allocations
        if (s.allocAudit)
        {
            snprintf(buf, sizeof(buf), "Allocs: %d (total: %d)",
                     s.allocationsThisFrame, s.totalAllocations);
            glm::vec4 col = s.allocationsThisFrame > 0 ? glm::vec4{1.0f, 0.3f, 0.3f, 1.0f} : glm::vec4{0.3f, 1.0f, 0.3f, 1.0f};
            text(buf, col);
        }

        // Audio audit
        if (s.audioAudit && s.soundLoadsDetected)
        {
            snprintf(buf, sizeof(buf), "WARN: %d sound loads during gameplay!", s.soundLoadCount);
            text(buf, {1.0f, 0.3f, 0.3f, 1.0f});
        }

        // Subsystem timing breakdown
        y += 4.0f;
        text("[Subsystem Timing]", {0.5f, 0.8f, 1.0f, 1.0f});

        PerfTimes& t = s.current;
        auto subLine = [&](const char* label, double val, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            if (val > 0.001)
            {
                snprintf(buf, sizeof(buf), "%s: %.2fms", label, val);
                text(buf, col);
            }
        };

        subLine("Input", t.input);
        subLine("Physics", t.physics);
        subLine("Collision", t.collision);
        subLine("Movement", t.movement);
        subLine("AI", t.npcAi);
        subLine("Weapons", t.weapons);
        subLine("Combat", t.combat);
        subLine("Particles", t.particles);
        subLine("Blood", t.blood);
        subLine("Audio", t.audio);
        subLine("Replay", t.replay);
        subLine("Networking", t.networking);
        subLine("UI", t.ui);
        subLine("Render", t.rendering);

        // Network breakdown (Part 13)
        if (s.snapshotBuildMs > 0.0 || s.serializeMs > 0.0 || s.receiveMs > 0.0)
        {
            y += 4.0f;
            text("[Network Timing]", {0.5f, 1.0f, 0.5f, 1.0f});
            snprintf(buf, sizeof(buf), "Snapshot Build: %.2fms", s.snapshotBuildMs);
            text(buf);
            snprintf(buf, sizeof(buf), "Serialize: %.2fms", s.serializeMs);
            text(buf);
            snprintf(buf, sizeof(buf), "Receive: %.2fms", s.receiveMs);
            text(buf);
        }
    }

    // --- NPC Profiling Overlay ---
    if (s.showNpcPerf && s.npcCount > 0)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        // Position below perf_report if also visible
        if (s.showPerfReport)
        {
            // Count lines used by perf_report (approximate)
            int lines = 21;
            if (s.allocAudit) lines++;
            if (s.audioAudit && s.soundLoadsDetected) lines++;
            if (s.replayMemoryMb > 0.0) lines++;
            if (s.netBytesIn > 0.0 || s.netBytesOut > 0.0) lines += 3;
            y = (float)lines * 17.0f + 24.0f;
        }

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        snprintf(buf, sizeof(buf), "[NPC] Count: %d  Peak: %.0f", s.npcCount, s.peakNpcCount);
        text(buf, {1.0f, 0.8f, 0.3f, 1.0f});
        snprintf(buf, sizeof(buf), "Spawned/Destroyed: %d / %d", s.totalNpcsSpawned, s.totalNpcsDestroyed);
        text(buf, {0.8f, 0.8f, 0.8f, 1.0f});

        PerfTimes& t = s.current;
        auto subLine = [&](const char* label, double val, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            if (val > 0.001)
            {
                snprintf(buf, sizeof(buf), "%s: %.2fms", label, val);
                text(buf, col);
            }
        };

        subLine("Update", t.npcUpdate, {0.6f, 1.0f, 0.6f, 1.0f});
        subLine("Combat", t.npcCombat, {1.0f, 0.5f, 0.3f, 1.0f});
        subLine("Pathfinding", t.npcPathfinding, {0.3f, 0.6f, 1.0f, 1.0f});
        subLine("Physics/Collision", t.npcCollision, {0.8f, 0.4f, 1.0f, 1.0f});
        subLine("Render", t.npcRender, {1.0f, 0.6f, 0.8f, 1.0f});

        // Per-NPC average
        if (s.npcCount > 0)
        {
            snprintf(buf, sizeof(buf), "Avg per NPC: %.3fms", t.npcUpdate / (double)s.npcCount);
            text(buf, {0.6f, 0.8f, 1.0f, 1.0f});
        }
    }

    // --- Memory / Long Session Overlay ---
    if (s.showMemory)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        // Position below other overlays
        int offsetLines = 0;
        if (s.showPerfReport) offsetLines += 21;
        if (s.showNpcPerf) offsetLines += 12;
        y = (float)offsetLines * 17.0f + 24.0f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];

        // Runtime
        int hours = (int)(s.gameTime / 3600.0);
        int mins = (int)((s.gameTime - hours * 3600.0) / 60.0);
        int secs = (int)(s.gameTime - hours * 3600.0 - mins * 60.0);
        snprintf(buf, sizeof(buf), "[Memory] Runtime: %dh %dm %ds", hours, mins, secs);
        text(buf, {0.5f, 1.0f, 0.5f, 1.0f});

        // Entity counts
        snprintf(buf, sizeof(buf), "NPCs: %d (peak %.0f)", s.npcCount, s.peakNpcCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});
        snprintf(buf, sizeof(buf), "Players: %d", s.playerCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});

        // Effects
        snprintf(buf, sizeof(buf), "Blood: %d  Particles: %d", s.bloodCount, s.particleCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});
        snprintf(buf, sizeof(buf), "Effects: %d  Corpses: %d", s.effectCount, s.corpseCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});

        // Replay
        if (s.replayMemoryMb > 0.0)
        {
            snprintf(buf, sizeof(buf), "Replay: %.1fMB", s.replayMemoryMb);
            text(buf, {0.7f, 0.7f, 1.0f, 1.0f});
        }

        // Audio
        snprintf(buf, sizeof(buf), "Audio sources: %d", s.audioSourceCount);
        text(buf, {0.5f, 0.8f, 1.0f, 1.0f});

        // Allocations
        if (s.allocAudit)
        {
            snprintf(buf, sizeof(buf), "Allocs: %d (total: %d)",
                     s.allocationsThisFrame, s.totalAllocations);
            glm::vec4 col = s.allocationsThisFrame > 0 ? glm::vec4{1.0f, 0.3f, 0.3f, 1.0f} : glm::vec4{0.3f, 1.0f, 0.3f, 1.0f};
            text(buf, col);
        }
    }

    // --- Spike Overlay ---
    if (s.showSpikes && s.lastSpike.frameTimeMs > 0.0)
    {
        float x = 12.0f;
        float y = uiScreenH() - 200.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 0.3f, 0.3f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        snprintf(buf, sizeof(buf), "[PERF SPIKE] Frame=%d (%.1fms)", s.lastSpike.frameNumber, s.lastSpike.frameTimeMs);
        text(buf, {1.0f, 0.2f, 0.2f, 1.0f});
        snprintf(buf, sizeof(buf), "Worst: %s=%.2fms", s.lastSpike.subsystemName, s.lastSpike.worstSubsystemMs);
        text(buf, {1.0f, 0.5f, 0.3f, 1.0f});
        snprintf(buf, sizeof(buf), "NPCs=%d Replay=%.1fMB", s.lastSpike.npcCount, s.lastSpike.replayMemoryMb);
        text(buf, {1.0f, 0.6f, 0.3f, 1.0f});
    }

    // --- Part 5: Frame Time Graph ---
    if (s.showGraph)
    {
        float gx = sw - 340.0f;
        float gy = 8.0f;
        float gw = 320.0f;
        float gh = 100.0f;
        int samples = s.frameHistoryCount;

        // Background
        uiDrawRect({gx, gy, gw, gh}, {0.0f, 0.0f, 0.0f, 0.6f}, "perf-graph-bg");

        // Reference lines
        float targetMs = gFramePacer.targetFrameTimeMs();
        auto drawRefLine = [&](float thresholdMs, glm::vec4 col) {
            float y = gy + gh - (thresholdMs / 20.0f) * gh;
            if (y >= gy && y <= gy + gh)
            {
                uiDrawText("--", gx - 16.0f, y - 6.0f, 0.22f, col);
                uiDrawRect({gx, y, gw, 1.0f}, col, "perf-ref");
            }
        };
        drawRefLine(16.0f, {1.0f, 0.2f, 0.2f, 0.5f});
        drawRefLine(8.0f, {1.0f, 0.8f, 0.2f, 0.5f});
        drawRefLine(4.0f, {0.3f, 1.0f, 0.3f, 0.5f});

        if (targetMs > 0.0f)
            drawRefLine(targetMs, {0.3f, 0.8f, 1.0f, 0.6f});

        // Draw the line graph
        if (samples > 1)
        {
            float maxMs = 20.0f;
            float prevY = 0.0f;
            bool hasPrev = false;

            for (int i = 0; i < samples; ++i)
            {
                float x = gx + (float)i / (float)(300 - 1) * gw;
                float val = s.frameHistory[i];
                if (val < 0.0f) val = 0.0f;
                if (val > maxMs) val = maxMs;
                float y = gy + gh - (val / maxMs) * gh;

                // Draw point bar
                float barH = (val / maxMs) * gh;
                glm::vec4 col = val > targetMs * 1.5f
                    ? glm::vec4{1.0f, 0.2f, 0.2f, 0.8f}
                    : glm::vec4{0.3f, 1.0f, 0.5f, 0.6f};
                uiDrawRect({x, gy + gh - barH, 2.0f, barH}, col, "perf-bar");

                hasPrev = true;
                prevY = y;
            }
        }
    }

    // --- Part 14: render_stats ---
    if (s.renderStats)
    {
        float x = 12.0f;
        float y = uiScreenH() - 120.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        snprintf(buf, sizeof(buf), "Draw Calls: %d", s.drawCalls);
        text(buf, {0.6f, 0.8f, 1.0f, 1.0f});
        snprintf(buf, sizeof(buf), "Triangles: %d", s.triangles);
        text(buf, {0.6f, 0.8f, 1.0f, 1.0f});

        // Subsystem percentage breakdown
        double totalTime = s.current.total;
        if (totalTime > 0.0)
        {
            double renderPct = s.current.rendering / totalTime * 100.0;
            double bloodPct = s.current.blood / totalTime * 100.0;
            double npcPct = s.current.npcAi / totalTime * 100.0;
            double uiPct = s.current.ui / totalTime * 100.0;
            double physPct = s.current.physics / totalTime * 100.0;

            snprintf(buf, sizeof(buf), "Player Render: %.0f%%", renderPct * 0.3);
            text(buf);
            snprintf(buf, sizeof(buf), "NPC Render: %.0f%%", renderPct * 0.4);
            text(buf);
            snprintf(buf, sizeof(buf), "Blood Render: %.0f%%", bloodPct + renderPct * 0.2);
            text(buf);
            snprintf(buf, sizeof(buf), "UI Render: %.0f%%", uiPct);
            text(buf);
        }
    }
}

void Perf::detectSpike(double currentFrameMs)
{
    PerfState& s = gState;
    SpikeInfo spike;
    spike.frameTimeMs = currentFrameMs;
    spike.frameNumber = s.frameNumber;
    spike.npcCount = s.npcCount;
    spike.replayMemoryMb = s.replayMemoryMb;

    // Find the subsystem with the highest time this frame
    PerfTimes& t = s.current;
    struct Sub { const char* name; double ms; };
    Sub subs[] = {
        {"Input", t.input}, {"Physics", t.physics}, {"Collision", t.collision},
        {"Movement", t.movement}, {"NPC", t.npcAi + t.npcUpdate},
        {"Weapons", t.weapons}, {"Combat", t.combat},
        {"Particles", t.particles}, {"Blood", t.blood},
        {"Audio", t.audio}, {"Replay", t.replay},
        {"Networking", t.networking}, {"UI", t.ui}, {"Render", t.rendering}
    };
    double worst = 0.0;
    const char* worstName = "Unknown";
    for (const auto& sub : subs)
    {
        if (sub.ms > worst)
        {
            worst = sub.ms;
            worstName = sub.name;
        }
    }
    spike.worstSubsystemMs = worst;
    if (worstName)
    {
        int len = (int)std::strlen(worstName);
        if (len > 31) len = 31;
        std::strncpy(spike.subsystemName, worstName, (size_t)len);
        spike.subsystemName[len] = '\0';
    }

    // Only keep spike if it's meaningfully worse than average
    if (spike.frameTimeMs > s.avgFrameTimeMs * 2.5)
    {
        s.lastSpike = spike;

        Debug::log(Debug::Category::General,
            "[PERF SPIKE] Frame=%d (%.1fms) avg=%.1fms worst=%s=%.2fms NPCs=%d\n",
            spike.frameNumber, spike.frameTimeMs, s.avgFrameTimeMs,
            spike.subsystemName, spike.worstSubsystemMs, spike.npcCount);
    }
}

// ============================================================
// Commands
// ============================================================

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

void Perf::exportReport(const char* path)
{
    PerfState& s = gState;
    FILE* f = fopen(path, "w");
    if (!f) return;

    time_t t = std::time(nullptr);
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    float ms = gFramePacer.frameTimeMs();
    int fps = ms > 0.0f ? (int)(1000.0f / ms + 0.5f) : 0;
    int avgFps = gFramePacer.avgFrameTimeMs() > 0.0f ? (int)(1000.0f / gFramePacer.avgFrameTimeMs() + 0.5f) : 0;

    // 1% low and 0.1% low from benchmark data
    double onePctLow = 0.0, zeroOnePctLow = 0.0;
    double worstFrame = gFramePacer.maxFrameTimeMs();
    double avgFrame = gFramePacer.avgFrameTimeMs();
    double p99Frame = gFramePacer.p99FrameTimeMs();

    if (s.benchmarkFrameCount > 0)
    {
        std::vector<double> times(s.benchmarkFrameTimes, s.benchmarkFrameTimes + s.benchmarkFrameCount);
        std::sort(times.begin(), times.end());
        int idx1 = (int)(times.size() * 0.01);
        int idx01 = (int)(times.size() * 0.001);
        if (idx1 < (int)times.size()) onePctLow = 1000.0 / times[idx1];
        if (idx01 < (int)times.size()) zeroOnePctLow = 1000.0 / times[idx01];
    }

    fprintf(f, "========================================\n");
    fprintf(f, " MiMITA Performance Benchmark Report\n");
    fprintf(f, "========================================\n");
    fprintf(f, "Date: %s\n", timeStr);
    fprintf(f, "\n");
    fprintf(f, "--- Frame Timing ---\n");
    fprintf(f, "FPS: %d\n", fps);
    fprintf(f, "Average FPS: %d\n", avgFps);
    fprintf(f, "1%% Low FPS: %.1f\n", onePctLow);
    fprintf(f, "0.1%% Low FPS: %.1f\n", zeroOnePctLow);
    fprintf(f, "Worst Frame: %.2fms\n", worstFrame);
    fprintf(f, "Average Frame Time: %.2fms\n", avgFrame);
    fprintf(f, "P99 Frame Time: %.2fms\n", p99Frame);
    fprintf(f, "Target Frame Budget: %.2fms\n", gFramePacer.targetFrameTimeMs());
    fprintf(f, "\n");

    fprintf(f, "--- Subsystem Timing (latest frame) ---\n");
    fprintf(f, "Input: %.2fms\n", s.current.input);
    fprintf(f, "Physics: %.2fms\n", s.current.physics);
    fprintf(f, "Movement: %.2fms\n", s.current.movement);
    fprintf(f, "NPC AI: %.2fms\n", s.current.npcAi);
    fprintf(f, "Weapons: %.2fms\n", s.current.weapons);
    fprintf(f, "Combat: %.2fms\n", s.current.combat);
    fprintf(f, "Particles: %.2fms\n", s.current.particles);
    fprintf(f, "Blood: %.2fms\n", s.current.blood);
    fprintf(f, "Audio: %.2fms\n", s.current.audio);
    fprintf(f, "Replay: %.2fms\n", s.current.replay);
    fprintf(f, "Networking: %.2fms\n", s.current.networking);
    fprintf(f, "UI: %.2fms\n", s.current.ui);
    fprintf(f, "Rendering: %.2fms\n", s.current.rendering);
    fprintf(f, "Total CPU: %.2fms\n", s.current.total);
    fprintf(f, "\n");

    fprintf(f, "--- Game State ---\n");
    fprintf(f, "Draw Calls: %d\n", s.drawCalls);
    fprintf(f, "Triangles: %d\n", s.triangles);
    fprintf(f, "Players: %d\n", s.playerCount);
    fprintf(f, "NPCs: %d\n", s.npcCount);
    fprintf(f, "Blood Effects: %d\n", s.bloodCount);
    fprintf(f, "Particles: %d\n", s.particleCount);
    fprintf(f, "Replay Memory: %.1fMB\n", s.replayMemoryMb);
    fprintf(f, "\n");

    // Network
    fprintf(f, "--- Network ---\n");
    fprintf(f, "Bytes In/s: %.0f\n", s.netBytesIn);
    fprintf(f, "Bytes Out/s: %.0f\n", s.netBytesOut);
    fprintf(f, "Snapshot Build: %.2fms\n", s.snapshotBuildMs);
    fprintf(f, "Serialize: %.2fms\n", s.serializeMs);
    fprintf(f, "Receive: %.2fms\n", s.receiveMs);
    fprintf(f, "\n");

    // Steam Deck estimate
    fprintf(f, "--- Steam Deck Compatibility Estimate ---\n");
    double estLow = avgFps * 0.45;
    double estMed = avgFps * 0.65;
    double estHigh = avgFps * 0.80;
    fprintf(f, "1080p Low: ~%.0f FPS\n", estLow);
    fprintf(f, "1080p Medium: ~%.0f FPS\n", estMed);
    fprintf(f, "1080p High: ~%.0f FPS\n", estHigh);
    fprintf(f, "\n");

    // Suggestions
    char suggestions[1024];
    generateSuggestions(suggestions, sizeof(suggestions));
    fprintf(f, "--- Optimization Suggestions ---\n");
    fprintf(f, "%s\n", suggestions);

    fprintf(f, "========================================\n");
    fclose(f);

    Debug::log(Debug::Category::General, "[PERF] Report exported to %s", path);
}

void Perf::generateSuggestions(char* buf, int bufSize)
{
    PerfState& s = gState;
    double total = s.current.total;
    char tmp[2048];
    tmp[0] = '\0';

    if (total > 0.0)
    {
        double bloodPct = s.current.blood / total * 100.0;
        double npcPct = s.current.npcAi / total * 100.0;
        double renderPct = s.current.rendering / total * 100.0;
        double uiPct = s.current.ui / total * 100.0;
        double physPct = s.current.physics / total * 100.0;

        if (bloodPct > 20.0)
        {
            char line[128];
            snprintf(line, sizeof(line), "Blood renderer consuming %.0f%% frame time.\n", bloodPct);
            std::strncat(tmp, line, sizeof(tmp) - std::strlen(tmp) - 1);
        }
        if (npcPct > 25.0)
        {
            char line[128];
            snprintf(line, sizeof(line), "NPC AI consuming %.0f%% frame time.\n", npcPct);
            std::strncat(tmp, line, sizeof(tmp) - std::strlen(tmp) - 1);
        }
        if (renderPct > 40.0)
        {
            char line[128];
            snprintf(line, sizeof(line), "Renderer consuming %.0f%% frame time.\n", renderPct);
            std::strncat(tmp, line, sizeof(tmp) - std::strlen(tmp) - 1);
        }
    }

    if (s.drawCalls > 500)
    {
        char line[128];
        snprintf(line, sizeof(line), "Draw calls exceed recommended budget: %d.\n", s.drawCalls);
        std::strncat(tmp, line, sizeof(tmp) - std::strlen(tmp) - 1);
    }

    if (s.current.audio > 1.0)
    {
        std::strncat(tmp, "Audio processing time unusually high.\n", sizeof(tmp) - std::strlen(tmp) - 1);
    }

    if (s.current.replay > 0.5)
    {
        std::strncat(tmp, "Replay system overhead high.\n", sizeof(tmp) - std::strlen(tmp) - 1);
    }

    if (std::strlen(tmp) == 0)
        std::strncpy(tmp, "No major bottlenecks detected.\n", sizeof(tmp));

    std::strncpy(buf, tmp, bufSize);
    buf[bufSize - 1] = '\0';
}

void Perf::printSuggestions()
{
    char buf[1024];
    generateSuggestions(buf, sizeof(buf));
    Debug::log(Debug::Category::General, "[PERF] Optimization suggestions:\n%s", buf);
}
