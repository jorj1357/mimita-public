#include "perf/perf-frame.h"
#include "perf/perf-spike.h"
#include "perf/perf.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "config.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

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
    const PerfState& ps = Perf::state();
    frame.npcCount = ps.npcCount;
    frame.playerCount = ps.playerCount > 0 ? ps.playerCount : 1;
    frame.particleCount = ps.current.particleCount;
    frame.effectCount = ps.current.effectsAlive;
    frame.audioCount = ps.audioSourceCount;
    frame.corpseCount = ps.corpseCount;
    frame.ragdollCount = 0;
    frame.bloodDecalCount = ps.current.damageNumbersAlive;
    frame.projectileCount = ps.projectileCount;
    frame.allocCount = ps.allocationsThisFrame;
    frame.allocBytes = (size_t)gPerfAllocBytes;
    frame.collisionQueryCount = ps.queryRecordCount;

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
    fprintf(f, "    allocs:     %d\n", frame.allocCount);
    fprintf(f, "    alloc_bytes: %zu\n", frame.allocBytes);
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
            "FRAME_%06d total=%.2fms budget=%.2fms npcs=%d effects=%d allocs=%d\n",
            frame.frameNumber, frame.totalMs, frame.budgetMs,
            frame.npcCount, frame.effectCount, frame.allocCount);
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

        delta("npcs",       before.npcCount, spike.npcCount);
        delta("particles",  before.particleCount, spike.particleCount);
        delta("effects",    before.effectCount, spike.effectCount);
        delta("audio",      before.audioCount, spike.audioCount);
        delta("corpses",    before.corpseCount, spike.corpseCount);
        delta("ragdolls",   before.ragdollCount, spike.ragdollCount);
        delta("decals",     before.bloodDecalCount, spike.bloodDecalCount);
        delta("projectiles", before.projectileCount, spike.projectileCount);
        delta("allocs",     before.allocCount, spike.allocCount);
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
