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

extern PerfState gState;


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

    fprintf(f, "--- Network ---\n");
    fprintf(f, "Bytes In/s: %.0f\n", s.netBytesIn);
    fprintf(f, "Bytes Out/s: %.0f\n", s.netBytesOut);
    fprintf(f, "Snapshot Build: %.2fms\n", s.snapshotBuildMs);
    fprintf(f, "Serialize: %.2fms\n", s.serializeMs);
    fprintf(f, "Receive: %.2fms\n", s.receiveMs);
    fprintf(f, "\n");

    fprintf(f, "--- Steam Deck Compatibility Estimate ---\n");
    double estLow = avgFps * 0.45;
    double estMed = avgFps * 0.65;
    double estHigh = avgFps * 0.80;
    fprintf(f, "1080p Low: ~%.0f FPS\n", estLow);
    fprintf(f, "1080p Medium: ~%.0f FPS\n", estMed);
    fprintf(f, "1080p High: ~%.0f FPS\n", estHigh);
    fprintf(f, "\n");

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
