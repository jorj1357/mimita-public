#include "perf/perf.h"
#include "perf/perf-accounting.h"
#include "perf/perf-report-state.h"
#include "perf/perf-spike.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "video/frame-pacer.h"
#include "gui/ui-system.h"
#include "debug/debug-log.h"

extern FramePacer gFramePacer;
extern PerfState gState;

namespace {

struct OverlayLine {
    std::string text;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct TimerValue {
    const char* label;
    double ms;
};

uint64_t overlayNowUs()
{
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()).count();
}

void addTimer(std::vector<TimerValue>& out, const char* label, double ms)
{
    if (ms > 0.001)
        out.push_back({label, ms});
}

float renderSampledPerfReport(int mode, float sw)
{
    static PerfFrameAccounting cached;
    static std::vector<OverlayLine> lines;
    static uint64_t lastBuildUs = 0;
    static int cachedMode = -1;

    const uint64_t now = overlayNowUs();
    const uint64_t intervalUs =
        (uint64_t)Perf::reportSampleMs() * 1000ull;
    if (lines.empty() || cachedMode != mode ||
        now - lastBuildUs >= intervalUs) {
        cached = gPerfFrameAccounting;
        cachedMode = mode;
        lastBuildUs = now;
        lines.clear();

        const float ms = gFramePacer.frameTimeMs();
        const int fps = ms > 0.0f ? (int)(1000.0f / ms + 0.5f) : 0;
        const int avgFps = gFramePacer.avgFrameTimeMs() > 0.0f
            ? (int)(1000.0f / gFramePacer.avgFrameTimeMs() + 0.5f) : 0;
        const float budget = gFramePacer.targetFrameTimeMs();
        const double cycleMs = cached.cycleMs > 0.0 ? cached.cycleMs : (double)ms;
        auto pct = [&](double value) {
            return cycleMs > 0.0 ? 100.0 * value / cycleMs : 0.0;
        };
        char buf[192];

        std::snprintf(buf, sizeof(buf), "FPS: %d  AVG: %d", fps, avgFps);
        lines.push_back({buf, {0.3f, 1.0f, 0.5f, 1.0f}});
        std::snprintf(buf, sizeof(buf), "FRAME: %.2fms  BUDGET: %.2fms",
                      ms, budget);
        lines.push_back({buf, {0.3f, 1.0f, 0.5f, 1.0f}});
        std::snprintf(buf, sizeof(buf), "MAX: %.2fms", gState.maxFrameTimeMs);
        lines.push_back({buf, {1.0f, 0.7f, 0.4f, 1.0f}});

        if (cached.valid) {
            std::snprintf(buf, sizeof(buf),
                          "HOST GAP: %.2fms (%.1f%%)  GAME: %.2fms (%.1f%%)",
                          cached.hostGapMs, pct(cached.hostGapMs),
                          cached.rootAccountedMs, pct(cached.rootAccountedMs));
            lines.push_back({buf, {1.0f, 0.85f, 0.4f, 1.0f}});
            std::snprintf(buf, sizeof(buf),
                          "SWAP: %.2fms  SLEEP: %.2fms  UNACCOUNTED: %.2fms",
                          cached.swapMs, cached.sleepMs, cached.unaccountedMs);
            lines.push_back({buf, {1.0f, 0.5f, 0.5f, 1.0f}});
        } else {
            lines.push_back({"warming up", {1.0f, 0.5f, 0.5f, 1.0f}});
        }

        std::snprintf(buf, sizeof(buf), "OVERLAY: %.2fms", cached.overlayMs);
        lines.push_back({buf, {0.7f, 0.7f, 1.0f, 1.0f}});

        if (cached.serverValid) {
            char actual[32];
            if (cached.serverActualValid)
                std::snprintf(actual, sizeof(actual), "%.1f Hz",
                              cached.serverActualHz);
            else
                std::snprintf(actual, sizeof(actual), "n/a");
            std::snprintf(buf, sizeof(buf),
                          "SERVER RX: %.1f Hz  ACTUAL: %s  TICK: %u%s",
                          cached.serverRxHz, actual, cached.serverTick,
                          cached.serverStale ? " (stale)" : "");
            lines.push_back({buf, {0.5f, 1.0f, 0.9f, 1.0f}});
        } else {
            lines.push_back({"SERVER: off", {0.6f, 0.8f, 0.9f, 1.0f}});
        }

        if (cached.publishedGeneration[0])
        {
            std::snprintf(buf, sizeof(buf), "GEN: %s",
                          cached.publishedGeneration);
            lines.push_back({buf, {0.7f, 0.7f, 0.8f, 1.0f}});
        }

        const int scopeLimit = mode == 3
            ? cached.scopeCount
            : std::min(5, cached.scopeCount);
        if (scopeLimit > 0) {
            lines.push_back({"[Frame Scopes]", {0.5f, 0.8f, 1.0f, 1.0f}});
            for (int i = 0; i < scopeLimit; ++i) {
                const PerfAccountedScope& scope = cached.scopes[i];
                std::snprintf(buf, sizeof(buf), "%s: %.2fms (%.1f%%)",
                              scope.label, scope.selfMs, pct(scope.selfMs));
                lines.push_back({buf, {0.8f, 0.95f, 1.0f, 1.0f}});
            }
        }

        std::vector<TimerValue> timers;
        const PerfTimes& t = cached.times;
        addTimer(timers, "Input", t.input);
        addTimer(timers, "Setup", t.setup);
        addTimer(timers, "Audio", t.audio);
        addTimer(timers, "State", t.state);
        addTimer(timers, "Replay", t.replay);
        addTimer(timers, "Networking", t.networking);
        addTimer(timers, "Camera", t.camera);
        addTimer(timers, "Combat", t.combat);
        addTimer(timers, "UI", t.ui);
        addTimer(timers, "Physics", t.physics);
        addTimer(timers, "SweepSlide", t.sweepSlide);
        addTimer(timers, "Depenetration", t.depenetration);
        addTimer(timers, "GroundDetection", t.groundDetection);
        addTimer(timers, "ChunkQuery", t.chunkQuery);
        addTimer(timers, "BodyWeaponColl.", t.weaponCollisions);
        addTimer(timers, "Movement", t.movement);
        addTimer(timers, "NPC Update", t.npcUpdate);
        addTimer(timers, "NPC Think", t.npcThink);
        addTimer(timers, "NPC Pathfinding", t.npcPathfinding);
        addTimer(timers, "NPC Combat", t.npcCombat);
        addTimer(timers, "NPC Collision", t.npcCollision);
        addTimer(timers, "NPC Render", t.npcRender);
        addTimer(timers, "Weapons", t.weapons);
        addTimer(timers, "Projectiles", t.projectiles);
        addTimer(timers, "Particles", t.particles);
        addTimer(timers, "Blood", t.blood);
        addTimer(timers, "Damage Numbers", t.damageNumbers);
        addTimer(timers, "Rendering", t.rendering);
        addTimer(timers, "ShadowRender", t.shadowRender);
        addTimer(timers, "PostFX", t.postFX);
        addTimer(timers, "WorldRender", t.worldRender);
        addTimer(timers, "PlayerRender", t.playerRender);
        addTimer(timers, "WeaponRender", t.weaponRender);
        addTimer(timers, "EffectRender", t.effectRender);
        addTimer(timers, "WorldCulling", t.worldCulling);
        addTimer(timers, "MapTraversal", t.mapTraversal);
        std::sort(timers.begin(), timers.end(),
                  [](const TimerValue& a, const TimerValue& b) {
                      return a.ms > b.ms;
                  });
        const int timerLimit = mode == 3
            ? (int)timers.size()
            : std::min<int>(5, (int)timers.size());
        if (timerLimit > 0) {
            lines.push_back({"[Timers]", {0.5f, 0.8f, 1.0f, 1.0f}});
            for (int i = 0; i < timerLimit; ++i) {
                std::snprintf(buf, sizeof(buf), "%s: %.2fms (%.1f%%)",
                              timers[(size_t)i].label, timers[(size_t)i].ms,
                              pct(timers[(size_t)i].ms));
                lines.push_back({buf, {0.9f, 0.95f, 1.0f, 1.0f}});
            }
        }

        if (mode == 3 &&
            (gState.snapshotBuildMs > 0.0 || gState.serializeMs > 0.0 ||
             gState.receiveMs > 0.0)) {
            lines.push_back({"[Network Timing]", {0.5f, 1.0f, 0.5f, 1.0f}});
            std::snprintf(buf, sizeof(buf), "Snapshot Build: %.2fms",
                          gState.snapshotBuildMs);
            lines.push_back({buf, {0.8f, 1.0f, 0.8f, 1.0f}});
            std::snprintf(buf, sizeof(buf), "Serialize: %.2fms",
                          gState.serializeMs);
            lines.push_back({buf, {0.8f, 1.0f, 0.8f, 1.0f}});
            std::snprintf(buf, sizeof(buf), "Receive: %.2fms",
                          gState.receiveMs);
            lines.push_back({buf, {0.8f, 1.0f, 0.8f, 1.0f}});
        }
    }

    const float x = sw - 310.0f;
    float y = 8.0f;
    const float lineH = 17.0f;
    const float small = mode == 3 ? 0.26f : 0.28f;
    for (const OverlayLine& line : lines) {
        uiDrawText(line.text.c_str(), x, y, small, line.color);
        y += lineH;
    }
    return y;
}

}  // namespace

void Perf::renderOverlay()
{
    MIMITA_PERF_SCOPE("Perf::Overlay");
    PerfState& s = gState;
    PerfFrameAccounting& acct = gPerfFrameAccounting;
    const int reportMode = Perf::reportMode();
    float sw = uiScreenW();
    float reportBottomY = 8.0f;
    float npcBottomY = 8.0f;

    if (reportMode == 2 || reportMode == 3)
    {
        reportBottomY = renderSampledPerfReport(reportMode, sw);
    }
    else if (s.showPerfReport && reportMode == 1)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        // Word-wrap the overlay text to the right edge so long lines (MAX /
        // UNACCOUNTED) do not run off-screen.
        auto text = [&](const char* str, glm::vec4 col = {0.3f, 1.0f, 0.5f, 1.0f}) {
            const std::string s(str);
            const float maxW = sw - x - 8.0f;
            std::string line;
            std::string word;
            auto flushWord = [&]() {
                if (word.empty())
                    return;
                const std::string candidate =
                    line.empty() ? word : line + " " + word;
                if (!line.empty() &&
                    uiMeasureText(candidate.c_str(), small) > maxW)
                {
                    uiDrawText(line.c_str(), x, y, small, col);
                    y += lineH;
                    line = word;
                }
                else
                {
                    line = candidate;
                }
                word.clear();
            };
            for (char ch : s)
            {
                if (ch == ' ')
                    flushWord();
                else
                    word += ch;
            }
            flushWord();
            if (!line.empty())
            {
                uiDrawText(line.c_str(), x, y, small, col);
                y += lineH;
            }
        };

        char buf[160];
        float ms = gFramePacer.frameTimeMs();
        int fps = ms > 0.0f ? (int)(1000.0f / ms + 0.5f) : 0;
        int avgFps = gFramePacer.avgFrameTimeMs() > 0.0f ? (int)(1000.0f / gFramePacer.avgFrameTimeMs() + 0.5f) : 0;
        float budget = gFramePacer.targetFrameTimeMs();
        const double cycleMs = acct.cycleMs > 0.0 ? acct.cycleMs : (double)ms;
        auto pctOf = [&](double value) {
            return cycleMs > 0.0 ? 100.0 * value / cycleMs : 0.0;
        };

        snprintf(buf, sizeof(buf), "FPS: %d  AVG: %d", fps, avgFps);
        text(buf, {0.3f, 1.0f, 0.5f, 1.0f});
        snprintf(buf, sizeof(buf), "FRAME: %.2fms  BUDGET: %.2fms", ms, budget);
        text(buf, {0.3f, 1.0f, 0.5f, 1.0f});

        snprintf(buf, sizeof(buf),
                 "MAX: %.2fms  PHYS %.2f  ENT %.2f  REND %.2f",
                 s.maxFrameTimeMs, s.maxPhysicsMs, s.maxEntityPhysicsMs,
                 s.maxRenderingMs);
        text(buf, {1.0f, 0.7f, 0.4f, 1.0f});

        if (acct.valid)
        {
            snprintf(buf, sizeof(buf),
                     "HOST GAP: %.2fms (%.1f%%)  GAME: %.2fms (%.1f%%)",
                     acct.hostGapMs, pctOf(acct.hostGapMs),
                     acct.rootAccountedMs, pctOf(acct.rootAccountedMs));
            text(buf, {1.0f, 0.85f, 0.4f, 1.0f});
            snprintf(buf, sizeof(buf),
                     "SWAP: %.2fms  SLEEP: %.2fms  UNACCOUNTED: %.2fms",
                     acct.swapMs, acct.sleepMs, acct.unaccountedMs);
            text(buf, {1.0f, 0.5f, 0.5f, 1.0f});
        }
        else
        {
            text("HOST GAP: warming up  GAME: --  UNACCOUNTED: --",
                 {1.0f, 0.5f, 0.5f, 1.0f});
        }

        snprintf(buf, sizeof(buf), "OVERLAY: %.2fms", acct.overlayMs);
        text(buf, {0.7f, 0.7f, 1.0f, 1.0f});

        if (acct.serverValid)
        {
            char actual[32];
            if (acct.serverActualValid)
                snprintf(actual, sizeof(actual), "%.1f Hz", acct.serverActualHz);
            else
                snprintf(actual, sizeof(actual), "n/a");
            snprintf(buf, sizeof(buf),
                     "SERVER RX: %.1f Hz  ACTUAL: %s  TICK: %u%s",
                     acct.serverRxHz, actual, acct.serverTick,
                     acct.serverStale ? " (stale)" : "");
            text(buf, {0.5f, 1.0f, 0.9f, 1.0f});
        }
        else
        {
            text("SERVER: off", {0.6f, 0.8f, 0.9f, 1.0f});
        }

        if (acct.publishedGeneration[0])
        {
            snprintf(buf, sizeof(buf), "GEN: %s", acct.publishedGeneration);
            text(buf, {0.7f, 0.7f, 0.8f, 1.0f});
        }

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

        snprintf(buf, sizeof(buf), "Draw Calls: %d  Triangles: %dk",
                 s.drawCalls, s.triangles / 1000);
        text(buf, {0.6f, 0.8f, 1.0f, 1.0f});

        snprintf(buf, sizeof(buf), "Players: %d  NPCs: %d", s.playerCount, s.npcCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});

        snprintf(buf, sizeof(buf), "Blood: %d  Particles: %d",
                 s.bloodCount, s.particleCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});

        if (s.replayMemoryMb > 0.0)
        {
            snprintf(buf, sizeof(buf), "Replay: %.1fMB", s.replayMemoryMb);
            text(buf, {0.7f, 0.7f, 1.0f, 1.0f});
        }

        if (s.netBytesIn > 0.0 || s.netBytesOut > 0.0)
        {
            snprintf(buf, sizeof(buf), "Net In: %.0fB/s  Out: %.0fB/s",
                     s.netBytesIn, s.netBytesOut);
            text(buf, {0.5f, 1.0f, 0.8f, 1.0f});
        }

        if (s.allocAudit)
        {
            snprintf(buf, sizeof(buf), "Allocs: %d (total: %llu)",
                     s.allocationsThisFrame, (unsigned long long)s.totalAllocations);
            glm::vec4 col = s.allocationsThisFrame > 0 ? glm::vec4{1.0f, 0.3f, 0.3f, 1.0f} : glm::vec4{0.3f, 1.0f, 0.3f, 1.0f};
            text(buf, col);
        }

        if (s.audioAudit && s.soundLoadsDetected)
        {
            snprintf(buf, sizeof(buf), "WARN: %d sound loads during gameplay!", s.soundLoadCount);
            text(buf, {1.0f, 0.3f, 0.3f, 1.0f});
        }

        y += 4.0f;
        text("[Frame Scopes]", {0.5f, 0.8f, 1.0f, 1.0f});
        if (acct.valid)
        {
            for (int i = 0; i < acct.scopeCount; ++i)
            {
                const PerfAccountedScope& scope = acct.scopes[i];
                int indent = scope.depth * 2;
                if (indent > 12)
                    indent = 12;
                snprintf(buf, sizeof(buf), "%*s%s: %.2fms (%.1f%%)",
                         indent, "", scope.label, scope.selfMs,
                         pctOf(scope.selfMs));
                text(buf, {0.8f, 0.95f, 1.0f, 1.0f});
            }
        }
        else
        {
            text("warming up", {0.8f, 0.95f, 1.0f, 1.0f});
        }

        y += 4.0f;
        text("[Timers]", {0.5f, 0.8f, 1.0f, 1.0f});

        const PerfTimes& t = acct.valid ? acct.times : s.current;
        auto subLine = [&](const char* label, double val, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            if (val > 0.001)
            {
                snprintf(buf, sizeof(buf), "%s: %.2fms (%.1f%%)", label, val,
                         pctOf(val));
                text(buf, col);
            }
        };

        subLine("Input", t.input);
        subLine("Setup", t.setup);
        subLine("Audio", t.audio);
        subLine("State", t.state);
        subLine("Replay", t.replay);
        subLine("Networking", t.networking);
        subLine("Camera", t.camera);
        subLine("Combat", t.combat);
        subLine("UI", t.ui);
        subLine("Physics", t.physics);
        subLine("SweepSlide", t.sweepSlide);
        subLine("Depenetration", t.depenetration);
        subLine("GroundDetection", t.groundDetection);
        subLine("ChunkQuery", t.chunkQuery);
        subLine("BodyWeaponColl.", t.weaponCollisions);
        subLine("Movement", t.movement);
        subLine("NPC Update", t.npcUpdate);
        subLine("NPC Think", t.npcThink);
        subLine("NPC Pathfinding", t.npcPathfinding);
        subLine("NPC Combat", t.npcCombat);
        subLine("NPC Collision", t.npcCollision);
        subLine("NPC Render", t.npcRender);
        subLine("Weapons", t.weapons);
        subLine("Projectiles", t.projectiles);
        subLine("Particles", t.particles);
        subLine("Blood", t.blood);
        subLine("Damage Numbers", t.damageNumbers);
        subLine("Rendering", t.rendering);
        subLine("ShadowRender", t.shadowRender);
        subLine("PostFX", t.postFX);
        subLine("WorldRender", t.worldRender);
        subLine("PlayerRender", t.playerRender);
        subLine("WeaponRender", t.weaponRender);
        subLine("EffectRender", t.effectRender);
        subLine("WorldCulling", t.worldCulling);
        subLine("MapTraversal", t.mapTraversal);

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
        reportBottomY = y;
    }

    if (s.showNpcPerf && s.npcCount > 0)
    {
        float x = sw - 310.0f;
        float y = s.showPerfReport ? reportBottomY + 8.0f : 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];
        snprintf(buf, sizeof(buf), "[NPC] Count: %d  Peak: %.0f", s.npcCount, s.peakNpcCount);
        text(buf, {1.0f, 0.8f, 0.3f, 1.0f});
        snprintf(buf, sizeof(buf), "Spawned/Destroyed: %d / %d", s.totalNpcsSpawned, s.totalNpcsDestroyed);
        text(buf, {0.8f, 0.8f, 0.8f, 1.0f});

        const PerfTimes& t = acct.valid ? acct.times : s.current;
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

        if (s.npcCount > 0)
        {
            snprintf(buf, sizeof(buf), "Avg per NPC: %.3fms", t.npcUpdate / (double)s.npcCount);
            text(buf, {0.6f, 0.8f, 1.0f, 1.0f});
        }
        npcBottomY = y;
    }

    if (s.showMemory)
    {
        float x = sw - 310.0f;
        float y = 8.0f;
        float lineH = 17.0f;
        float small = 0.28f;

        if (s.showNpcPerf && s.npcCount > 0)
            y = npcBottomY + 8.0f;
        else if (s.showPerfReport)
            y = reportBottomY + 8.0f;

        auto text = [&](const char* str, glm::vec4 col = {1.0f, 1.0f, 1.0f, 1.0f}) {
            uiDrawText(str, x, y, small, col);
            y += lineH;
        };

        char buf[128];

        int hours = (int)(s.gameTime / 3600.0);
        int mins = (int)((s.gameTime - hours * 3600.0) / 60.0);
        int secs = (int)(s.gameTime - hours * 3600.0 - mins * 60.0);
        snprintf(buf, sizeof(buf), "[Memory] Runtime: %dh %dm %ds", hours, mins, secs);
        text(buf, {0.5f, 1.0f, 0.5f, 1.0f});

        snprintf(buf, sizeof(buf), "NPCs: %d (peak %.0f)", s.npcCount, s.peakNpcCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});
        snprintf(buf, sizeof(buf), "Players: %d", s.playerCount);
        text(buf, {1.0f, 1.0f, 0.7f, 1.0f});

        snprintf(buf, sizeof(buf), "Blood: %d  Particles: %d", s.bloodCount, s.particleCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});
        snprintf(buf, sizeof(buf), "Effects: %d  Corpses: %d", s.effectCount, s.corpseCount);
        text(buf, {1.0f, 0.7f, 0.5f, 1.0f});

        if (s.replayMemoryMb > 0.0)
        {
            snprintf(buf, sizeof(buf), "Replay: %.1fMB", s.replayMemoryMb);
            text(buf, {0.7f, 0.7f, 1.0f, 1.0f});
        }

        snprintf(buf, sizeof(buf), "Audio sources: %d", s.audioSourceCount);
        text(buf, {0.5f, 0.8f, 1.0f, 1.0f});

        if (s.allocAudit)
        {
            snprintf(buf, sizeof(buf), "Allocs: %d (total: %llu)",
                     s.allocationsThisFrame, (unsigned long long)s.totalAllocations);
            glm::vec4 col = s.allocationsThisFrame > 0 ? glm::vec4{1.0f, 0.3f, 0.3f, 1.0f} : glm::vec4{0.3f, 1.0f, 0.3f, 1.0f};
            text(buf, col);
        }
    }

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

    if (s.showGraph)
    {
        float gx = sw - 340.0f;
        float gy = 8.0f;
        float gw = 320.0f;
        float gh = 100.0f;
        int samples = s.frameHistoryCount;

        uiDrawRect({gx, gy, gw, gh}, {0.0f, 0.0f, 0.0f, 0.6f}, "perf-graph-bg");

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

        if (samples > 1)
        {
            float maxMs = 20.0f;

            for (int i = 0; i < samples; ++i)
            {
                float x = gx + (float)i / (float)(300 - 1) * gw;
                float val = s.frameHistory[i];
                if (val < 0.0f) val = 0.0f;
                if (val > maxMs) val = maxMs;

                float barH = (val / maxMs) * gh;
                glm::vec4 col = val > targetMs * 1.5f
                    ? glm::vec4{1.0f, 0.2f, 0.2f, 0.8f}
                    : glm::vec4{0.3f, 1.0f, 0.5f, 0.6f};
                uiDrawRect({x, gy + gh - barH, 2.0f, barH}, col, "perf-bar");
            }
        }
    }

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

    PerfTimes& t = s.current;
    struct Sub { const char* name; double ms; };
    Sub subs[] = {
        {"Input", t.input}, {"Setup", t.setup}, {"Audio", t.audio}, {"State", t.state},
        {"Replay", t.replay}, {"Networking", t.networking}, {"Camera", t.camera},
        {"Combat", t.combat}, {"UI", t.ui},
        {"Physics", t.physics}, {"SweepSlide", t.sweepSlide},
        {"Depenetration", t.depenetration}, {"ChunkQuery", t.chunkQuery},
        {"GroundDetection", t.groundDetection}, {"WeaponCollisions", t.weaponCollisions},
        {"Movement", t.movement},
        {"NPC Update", t.npcUpdate}, {"NPC Think", t.npcThink},
        {"NPC Pathfinding", t.npcPathfinding}, {"NPC Combat", t.npcCombat},
        {"NPC Collision", t.npcCollision}, {"NPC Render", t.npcRender},
        {"Weapons", t.weapons}, {"Projectiles", t.projectiles},
        {"Particles", t.particles}, {"Blood", t.blood},
        {"Rendering", t.rendering}, {"ShadowRender", t.shadowRender},
        {"PostFX", t.postFX}, {"WorldRender", t.worldRender},
        {"PlayerRender", t.playerRender}, {"EffectRender", t.effectRender},
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

    if (spike.frameTimeMs > s.avgFrameTimeMs * 2.5)
    {
        s.lastSpike = spike;

        Debug::log(Debug::Category::General,
            "[PERF SPIKE] Frame=%d (%.1fms) avg=%.1fms worst=%s=%.2fms NPCs=%d\n",
            spike.frameNumber, spike.frameTimeMs, s.avgFrameTimeMs,
            spike.subsystemName, spike.worstSubsystemMs, spike.npcCount);
    }
}

