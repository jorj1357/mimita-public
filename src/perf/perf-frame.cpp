#include "perf/perf-frame.h"
#include "perf/perf-spike.h"
#include "perf/perf.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "config.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <psapi.h>
#include <nlohmann/json.hpp>

namespace {

struct WindowScopeStats {
    char name[64] = {};
    uint64_t calls = 0;
    double totalMs = 0.0;
    double maxMs = 0.0;
};

struct ResourceWindow {
    int frames = 0;
    int firstFrame = 0;
    int lastFrame = 0;
    double totalFrameMs = 0.0;
    double maxFrameMs = 0.0;
    int maxFrame = 0;
    uint64_t allocCount = 0;
    uint64_t allocBytes = 0;
    int maxNpcCount = 0;
    int maxEffectCount = 0;
    int maxProjectileCount = 0;
    WindowScopeStats scopes[32];
    int scopeCount = 0;
};

ResourceWindow gResourceWindow;

void resetResourceWindow()
{
    gResourceWindow = ResourceWindow{};
}

WindowScopeStats* scopeStatsFor(const char* name)
{
    for (int i = 0; i < gResourceWindow.scopeCount; ++i)
        if (std::strncmp(gResourceWindow.scopes[i].name, name, sizeof(gResourceWindow.scopes[i].name)) == 0)
            return &gResourceWindow.scopes[i];
    if (gResourceWindow.scopeCount >= 32) return nullptr;
    WindowScopeStats& out = gResourceWindow.scopes[gResourceWindow.scopeCount++];
    std::snprintf(out.name, sizeof(out.name), "%s", name ? name : "?");
    return &out;
}

void accumulateResourceFrame(const PerfFrame& frame)
{
    if (gResourceWindow.frames == 0) gResourceWindow.firstFrame = frame.frameNumber;
    gResourceWindow.lastFrame = frame.frameNumber;
    ++gResourceWindow.frames;
    gResourceWindow.totalFrameMs += frame.totalMs;
    if (frame.totalMs > gResourceWindow.maxFrameMs) {
        gResourceWindow.maxFrameMs = frame.totalMs;
        gResourceWindow.maxFrame = frame.frameNumber;
    }
    gResourceWindow.allocCount += frame.allocCount;
    gResourceWindow.allocBytes += frame.allocBytes;
    gResourceWindow.maxNpcCount = std::max(gResourceWindow.maxNpcCount, frame.npcCount);
    gResourceWindow.maxEffectCount = std::max(gResourceWindow.maxEffectCount, frame.effectCount);
    gResourceWindow.maxProjectileCount = std::max(gResourceWindow.maxProjectileCount, frame.projectileCount);
    for (int i = 0; i < frame.entryCount; ++i) {
        const PerfBreakdownEntry& in = frame.entries[i];
        WindowScopeStats* out = scopeStatsFor(in.label);
        if (!out) continue;
        out->calls += in.callCount;
        out->totalMs += in.inclMs;
        out->maxMs = std::max(out->maxMs, in.inclMs);
    }
}

void emitResourceWindowSummary()
{
    if (gResourceWindow.frames == 0) return;

    PROCESS_MEMORY_COUNTERS_EX memory{};
    const bool memoryOk = GetProcessMemoryInfo(
        GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
        sizeof(memory)) != 0;
    const StructuredLogger::IoStats io = StructuredLogger::instance().ioStats(true);

    nlohmann::json scopes = nlohmann::json::array();
    for (int i = 0; i < gResourceWindow.scopeCount; ++i) {
        const WindowScopeStats& s = gResourceWindow.scopes[i];
        scopes.push_back({
            {"name", s.name}, {"calls", s.calls},
            {"total_ms", s.totalMs}, {"max_ms", s.maxMs}});
    }

    nlohmann::json fields = {
        {"window_frames", gResourceWindow.frames},
        {"frame_start", gResourceWindow.firstFrame},
        {"frame_end", gResourceWindow.lastFrame},
        {"frame_total_ms", gResourceWindow.totalFrameMs},
        {"frame_average_ms", gResourceWindow.totalFrameMs / std::max(1, gResourceWindow.frames)},
        {"frame_max_ms", gResourceWindow.maxFrameMs},
        {"frame_max_number", gResourceWindow.maxFrame},
        {"alloc_count", gResourceWindow.allocCount},
        {"alloc_bytes", gResourceWindow.allocBytes},
        {"max_npc_count", gResourceWindow.maxNpcCount},
        {"max_effect_count", gResourceWindow.maxEffectCount},
        {"max_projectile_count", gResourceWindow.maxProjectileCount},
        {"logger_events_written", io.eventsWritten},
        {"logger_bytes_written", io.bytesWritten},
        {"logger_flush_count", io.flushCount},
        {"logger_flush_ms", static_cast<double>(io.flushMicroseconds) / 1000.0},
        {"logger_mutex_wait_ms", static_cast<double>(io.mutexWaitMicroseconds) / 1000.0},
        {"memory_available", memoryOk},
        {"working_set_bytes", memoryOk ? memory.WorkingSetSize : 0},
        {"private_bytes", memoryOk ? memory.PrivateUsage : 0},
        {"pagefile_usage_bytes", memoryOk ? memory.PagefileUsage : 0},
        {"scopes", scopes}
    };
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Performance, StructuredLevel::Important,
        "performance.resource-window", "performance", "60-frame-summary",
        static_cast<uint32_t>(gResourceWindow.lastFrame), fields,
        __FILE__, __LINE__, __FUNCTION__);
    resetResourceWindow();
}

}

// ── Global state ────────────────────────────────────────────

PerfFrame gFrameHistory[FRAME_HISTORY_CAPACITY];
int gFrameHistoryIndex = 0;
int gFrameHistoryCount = 0;

// ── Capture current frame ───────────────────────────────────

void perfCaptureFrame(double totalMs, double budgetMs, int frameNumber)
{
    PerfFrame& frame = gFrameHistory[gFrameHistoryIndex % FRAME_HISTORY_CAPACITY];
    frame.frameNumber = frameNumber;
    frame.totalMs = totalMs;
    frame.budgetMs = budgetMs;
    frame.entryCount = 0;

    // Aggregate scopes into a flat tree
    // First compute child sums to get self time
    double childSum[MAX_SCOPES_PER_FRAME] = {0.0};
    for (int i = 0; i < gPerfScopeCount; ++i) {
        if (gPerfScopes[i].parentIndex >= 0 && gPerfScopes[i].parentIndex < gPerfScopeCount) {
            childSum[gPerfScopes[i].parentIndex] += (double)gPerfScopes[i].cyclesInclusive / 1000000.0;
        }
    }

    for (int i = 0; i < gPerfScopeCount && frame.entryCount < MAX_BREAKDOWN_ENTRIES; ++i) {
        const PerfScopeCapture& cap = gPerfScopes[i];
        double inclMs = (double)cap.cyclesInclusive / 1000000.0;
        double selfMs = inclMs - childSum[i];
        if (selfMs < 0.0) selfMs = 0.0;

        PerfBreakdownEntry& e = frame.entries[frame.entryCount++];
        std::snprintf(e.label, sizeof(e.label), "%s", cap.label ? cap.label : "?");
        e.selfMs = selfMs;
        e.inclMs = inclMs;
        e.callCount = cap.callCount;
        e.depth = 0;

        // Compute depth from parent chain
        int depth = 0;
        int p = cap.parentIndex;
        while (p >= 0) { depth++; p = gPerfScopes[p].parentIndex; }
        e.depth = depth;
    }

    // Fill entity snapshot from PerfState (already populated by engine systems)
    PerfState& ps = Perf::state();
    frame.npcCount = ps.npcCount;
    frame.playerCount = ps.playerCount > 0 ? ps.playerCount : 1;
    frame.particleCount = ps.current.particleCount;
    frame.effectCount = ps.current.effectsAlive;
    frame.audioCount = ps.audioSourceCount;
    frame.corpseCount = ps.corpseCount;
    frame.ragdollCount = 0;
    frame.bloodDecalCount = ps.current.damageNumbersAlive;
    frame.projectileCount = ps.projectileCount;
    frame.collisionQueryCount = ps.queryRecordCount;

    // Allocation frame deltas from monotonically increasing global counters.
    // These globals (std::atomic<uint64_t>) are incremented by operator new
    // and never reset. Frame deltas are current minus previous snapshot.
    static bool sAllocSnapshotsInit = false;
    static uint64_t sPrevAllocCount = 0;
    static uint64_t sPrevAllocBytes = 0;

    const uint64_t curAllocCount = gPerfAllocCount.load(std::memory_order_relaxed);
    const uint64_t curAllocBytes = gPerfAllocBytes.load(std::memory_order_relaxed);

    uint64_t frameAllocCount = 0;
    uint64_t frameAllocBytes = 0;
    if (sAllocSnapshotsInit) {
        frameAllocCount = curAllocCount - sPrevAllocCount;
        frameAllocBytes = curAllocBytes - sPrevAllocBytes;
    } else {
        sAllocSnapshotsInit = true;
    }
    sPrevAllocCount = curAllocCount;
    sPrevAllocBytes = curAllocBytes;

    frame.allocCount = frameAllocCount;
    frame.allocBytes = frameAllocBytes;

    // Derived display values — allocationsThisFrame is NOT the source of truth.
    ps.allocationsThisFrame = frameAllocCount > static_cast<uint64_t>(INT_MAX)
        ? INT_MAX : static_cast<int>(frameAllocCount);
    ps.totalAllocations = curAllocCount;

    if (frameAllocBytes > 0 && frameAllocCount == 0) {
        Debug::warn(Debug::Category::General,
            "[PERF][ALLOC][WARN] frame bytes increased without frame count increase; "
            "investigate allocation hook mismatch");
    }

    accumulateResourceFrame(frame);
    if (gResourceWindow.frames >= 60)
        emitResourceWindowSummary();

    // 60-second heartbeat: log lifetime allocation totals
    {
        static auto sLastHeartbeat = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - sLastHeartbeat).count();
        if (elapsed >= 60.0) {
            Debug::log(Debug::Category::General,
                "[PERF][ALLOC] totals count=%llu bytes=%llu\n",
                (unsigned long long)curAllocCount, (unsigned long long)curAllocBytes);
            sLastHeartbeat = now;
        }
    }

    gFrameHistoryIndex++;
    if (gFrameHistoryCount < FRAME_HISTORY_CAPACITY)
        gFrameHistoryCount++;
}

// ── Write frame breakdown ───────────────────────────────────

void perfWriteFrameBreakdown(FILE* f, const PerfFrame& frame, bool showChildren)
{
    if (!f) return;

    fprintf(f, "FRAME_%06d\n", frame.frameNumber);

    // Sort entries by depth then self time descending
    struct SortEntry { int idx; double selfMs; };
    SortEntry sorted[MAX_BREAKDOWN_ENTRIES];
    int sortCount = 0;
    for (int i = 0; i < frame.entryCount; ++i) {
        if (frame.entries[i].selfMs > 0.005 || showChildren) {
            sorted[sortCount++] = {i, frame.entries[i].selfMs};
        }
    }
    std::sort(sorted, sorted + sortCount,
        [](const SortEntry& a, const SortEntry& b) { return a.selfMs > b.selfMs; });

    double budgetMs = frame.budgetMs > 0.0 ? frame.budgetMs : 16.667;

    for (int si = 0; si < sortCount; ++si) {
        const PerfBreakdownEntry& e = frame.entries[sorted[si].idx];
        double ratio = budgetMs > 0.0 ? e.selfMs / budgetMs : 0.0;
        const char* flag = ratio > 1.0 ? "  ← OVER BUDGET" : "";

        // Indent by depth
        for (int d = 0; d < e.depth; ++d) fprintf(f, "  ");
        fprintf(f, "  %s: %.2fms  (%u calls)%s\n",
                e.label, e.selfMs, e.callCount, flag);
    }

    // Summary
    double overBy = frame.totalMs - budgetMs;
    double slowdown = budgetMs > 0.0 ? frame.totalMs / budgetMs : 0.0;
    fprintf(f, "  total:         %.2fms\n", frame.totalMs);
    fprintf(f, "  budget:        %.2fms\n", budgetMs);
    fprintf(f, "  over_by:       %.2fms\n", overBy);
    fprintf(f, "  slowdown:      %.2fx\n", slowdown);
    fprintf(f, "  fps_eq:        %.1f\n", frame.totalMs > 0.0 ? 1000.0 / frame.totalMs : 0.0);
    fprintf(f, "  status:        %s\n", frame.totalMs <= budgetMs ? "PASS" : "FAIL");
    fprintf(f, "\n");

    // Entity counts
    fprintf(f, "  entities:\n");
    fprintf(f, "    npcs:       %d\n", frame.npcCount);
    fprintf(f, "    players:    %d\n", frame.playerCount);
    fprintf(f, "    particles:  %d\n", frame.particleCount);
    fprintf(f, "    effects:    %d\n", frame.effectCount);
    fprintf(f, "    audio:      %d\n", frame.audioCount);
    fprintf(f, "    corpses:    %d\n", frame.corpseCount);
    fprintf(f, "    ragdolls:   %d\n", frame.ragdollCount);
    fprintf(f, "    decals:     %d\n", frame.bloodDecalCount);
    fprintf(f, "    projectiles: %d\n", frame.projectileCount);
    fprintf(f, "    allocs:     %llu\n", (unsigned long long)frame.allocCount);
    fprintf(f, "    alloc_bytes: %llu\n", (unsigned long long)frame.allocBytes);
    fprintf(f, "    coll_queries: %d\n", frame.collisionQueryCount);
}

void perfWriteFrameSummary(FILE* f, const PerfFrame& frame)
{
    if (!f) return;
    double budgetMs = frame.budgetMs > 0.0 ? frame.budgetMs : 16.667;
    double overBy = frame.totalMs - budgetMs;
    double slowdown = budgetMs > 0.0 ? frame.totalMs / budgetMs : 0.0;

    fprintf(f, "%06d | %.2fms | %.2fms | %+.2fms | %.2fx | %s",
            frame.frameNumber, frame.totalMs, budgetMs, overBy, slowdown,
            frame.totalMs <= budgetMs ? "PASS\n" : "FAIL\n");
}

// ── Spike context (routed through StructuredLogger) ────────

void perfDumpSpikeContext(int spikeFrameIndex, int framesBefore, int framesAfter)
{
    if (gFrameHistoryCount == 0) return;

    int historyLen = std::min(gFrameHistoryCount, FRAME_HISTORY_CAPACITY);

    int spikeIdx = -1;
    for (int i = 0; i < historyLen; ++i) {
        int idx = (gFrameHistoryIndex - historyLen + i) % FRAME_HISTORY_CAPACITY;
        if (gFrameHistory[idx].frameNumber == spikeFrameIndex) {
            spikeIdx = idx;
            break;
        }
        if (spikeIdx < 0)
            spikeIdx = (gFrameHistoryIndex - 1 + FRAME_HISTORY_CAPACITY) % FRAME_HISTORY_CAPACITY;
    }
    if (spikeIdx < 0)
        spikeIdx = (gFrameHistoryIndex - 1 + FRAME_HISTORY_CAPACITY) % FRAME_HISTORY_CAPACITY;

    int spikePos = -1;
    for (int i = 0; i < historyLen; ++i) {
        int idx = (gFrameHistoryIndex - historyLen + i) % FRAME_HISTORY_CAPACITY;
        if (idx == spikeIdx) { spikePos = i; break; }
    }
    if (spikePos < 0) spikePos = historyLen - 1;

    char msg[16384];
    int pos = 0;
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "=== SPIKE CONTEXT BEGIN ===\n");
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Spike frame: %d\n", spikeFrameIndex);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Frames before: %d  Frames after: %d\n\n", framesBefore, framesAfter);

    int startPos = std::max(0, spikePos - framesBefore);
    int endPos = std::min(historyLen - 1, spikePos + framesAfter);

    for (int i = startPos; i <= endPos; ++i) {
        int idx = (gFrameHistoryIndex - historyLen + i) % FRAME_HISTORY_CAPACITY;
        const PerfFrame& frame = gFrameHistory[idx];

        if (i == spikePos)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos, "<<< SPIKE >>> ");
        else
            pos += std::snprintf(msg + pos, sizeof(msg) - pos, "           ");

        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "FRAME_%06d total=%.2fms budget=%.2fms npcs=%d effects=%d allocs=%llu\n",
            frame.frameNumber, frame.totalMs, frame.budgetMs,
            frame.npcCount, frame.effectCount, (unsigned long long)frame.allocCount);
    }

    // Entity delta
    if (spikePos > startPos) {
        int beforeIdx = (gFrameHistoryIndex - historyLen + spikePos - 1) % FRAME_HISTORY_CAPACITY;
        const PerfFrame& before = gFrameHistory[beforeIdx];
        const PerfFrame& spike = gFrameHistory[spikeIdx];

        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "\n=== ENTITY DELTA (frame %d → %d) ===\n",
            before.frameNumber, spike.frameNumber);

        auto delta = [&](const char* name, int b, int a) {
            int diff = a - b;
            if (diff != 0)
                pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                    "  %s: %d → %d (%+d)\n", name, b, a, diff);
        };

        auto deltaU64 = [&](const char* name, uint64_t b, uint64_t a) {
            if (a != b) {
                if (a > b) {
                    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                        "  %s: %llu → %llu (+%llu)\n", name,
                        (unsigned long long)b, (unsigned long long)a,
                        (unsigned long long)(a - b));
                } else {
                    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                        "  %s: %llu → %llu (-%llu)\n", name,
                        (unsigned long long)b, (unsigned long long)a,
                        (unsigned long long)(b - a));
                }
            }
        };

        delta("npcs",       before.npcCount, spike.npcCount);
        delta("particles",  before.particleCount, spike.particleCount);
        delta("effects",    before.effectCount, spike.effectCount);
        delta("audio",      before.audioCount, spike.audioCount);
        delta("corpses",    before.corpseCount, spike.corpseCount);
        delta("ragdolls",   before.ragdollCount, spike.ragdollCount);
        delta("decals",     before.bloodDecalCount, spike.bloodDecalCount);
        delta("projectiles", before.projectileCount, spike.projectileCount);
        deltaU64("allocs",  before.allocCount, spike.allocCount);
    }

    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "\n=== SPIKE CONTEXT END ===\n");

    // Route through StructuredLogger
    StructuredLogger::Entry e;
    e.category = StructuredCategory::Performance;
    e.level = StructuredLevel::Important;
    e.eventId = "PERFORMANCE_CONTEXT";
    e.reason = "Spike context for frame " + std::to_string(spikeFrameIndex);
    e.sourceFile = __FILE__;
    e.sourceLine = __LINE__;
    e.functionName = "perfDumpSpikeContext";
    e.frame = (uint32_t)spikeFrameIndex;
    e.message = msg;
    StructuredLogger::instance().write(e);

    Debug::log(Debug::Category::General,
        "[PERF] Spike context written for frame %d\n", spikeFrameIndex);
}
