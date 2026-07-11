#include "perf/perf-spike.h"
#include "perf/perf-frame.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "config.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

// ── Global state ────────────────────────────────────────────

PerfScopeCapture gPerfScopes[MAX_SCOPES_PER_FRAME];
int gPerfScopeCount = 0;
PerfBudgetConfig gPerfBudget;
int gPerfScopeStack[MAX_SCOPES_PER_FRAME];
int gPerfScopeStackDepth = 0;

int gPerfAllocCount = 0;
size_t gPerfAllocBytes = 0;
size_t gPerfLargestAlloc = 0;
const char* gPerfLargestAllocSite = nullptr;

char gPerfCorrelationStack[8][32] = {};
int gPerfCorrelationDepth = 0;

// ── Cycle counter ──────────────────────────────────────────

uint64_t PerfScopeGuard::readCycles()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ── Constructor / Destructor ───────────────────────────────

PerfScopeGuard::PerfScopeGuard(
    const char* file, int line, const char* func, const char* label)
{
    if (gPerfScopeCount >= MAX_SCOPES_PER_FRAME)
        return;

    mScopeIndex = gPerfScopeCount++;
    PerfScopeCapture& cap = gPerfScopes[mScopeIndex];
    cap.file = file;
    cap.line = line;
    cap.func = func;
    cap.label = label;
    cap.active = true;

    // Copy correlation ID from stack
    if (gPerfCorrelationDepth > 0) {
        std::strncpy(cap.correlationId, gPerfCorrelationStack[gPerfCorrelationDepth - 1], sizeof(cap.correlationId) - 1);
    }

    // Determine parent from stack
    if (gPerfScopeStackDepth > 0) {
        cap.parentIndex = gPerfScopeStack[gPerfScopeStackDepth - 1];
    } else {
        cap.parentIndex = -1;
    }

    // Push to stack
    if (gPerfScopeStackDepth < MAX_SCOPES_PER_FRAME) {
        gPerfScopeStack[gPerfScopeStackDepth++] = mScopeIndex;
    }

    // Capture pre-scope counters
    mAllocBefore = (uint32_t)gPerfAllocCount;
    mBytesBefore = (uint32_t)gPerfAllocBytes;
    mAssetLoadBefore = 0;
    mBloodBefore = 0;
    mDebrisBefore = 0;
    mDecalBefore = 0;
    mReplayEventsBefore = 0;
    mReplayBytesBefore = 0;
    mStartCycles = readCycles();
}

PerfScopeGuard::~PerfScopeGuard()
{
    if (mScopeIndex < 0 || mScopeIndex >= MAX_SCOPES_PER_FRAME)
        return;

    uint64_t endCycles = readCycles();
    uint64_t elapsed = endCycles - mStartCycles;

    PerfScopeCapture& cap = gPerfScopes[mScopeIndex];
    cap.cyclesInclusive += elapsed;
    if (elapsed < cap.minCycles) cap.minCycles = elapsed;
    if (elapsed > cap.maxCycles) cap.maxCycles = elapsed;
    cap.callCount++;

    // Record counters
    cap.allocCount += (uint32_t)(gPerfAllocCount - mAllocBefore);
    cap.allocBytes += (uint32_t)(gPerfAllocBytes - mBytesBefore);

    // Pop from stack
    if (gPerfScopeStackDepth > 0) {
        gPerfScopeStackDepth--;
    }
}

// ── Correlation ID ─────────────────────────────────────────

void perfSetCorrelation(const char* id)
{
    if (gPerfCorrelationDepth < 8 && id) {
        std::strncpy(gPerfCorrelationStack[gPerfCorrelationDepth], id, 31);
        gPerfCorrelationStack[gPerfCorrelationDepth][31] = '\0';
        gPerfCorrelationDepth++;
    }
}

void perfClearCorrelation()
{
    if (gPerfCorrelationDepth > 0)
        gPerfCorrelationDepth--;
}

// ── Frame aggregation ───────────────────────────────────────

void perfAggregateScopes(double totalFrameMs, double budgetMs)
{
    if (gPerfScopeCount == 0)
        return;

    // Compute child sums
    std::vector<double> childInclusiveSum(gPerfScopeCount, 0.0);
    for (int i = 0; i < gPerfScopeCount; ++i) {
        const PerfScopeCapture& cap = gPerfScopes[i];
        if (cap.parentIndex >= 0 && cap.parentIndex < gPerfScopeCount) {
            double inclMs = (double)cap.cyclesInclusive / 1000000.0;
            childInclusiveSum[cap.parentIndex] += inclMs;
        }
    }

    // Build sorted list
    struct Entry { int index; double inclMs; double selfMs; double pctOfTotal; };
    std::vector<Entry> entries;
    entries.reserve(gPerfScopeCount);

    for (int i = 0; i < gPerfScopeCount; ++i) {
        const PerfScopeCapture& cap = gPerfScopes[i];
        double inclMs = (double)cap.cyclesInclusive / 1000000.0;
        double selfMs = inclMs - childInclusiveSum[i];
        if (selfMs < 0.0) selfMs = 0.0;
        double pct = totalFrameMs > 0.0 ? (inclMs / totalFrameMs) * 100.0 : 0.0;
        entries.push_back({i, inclMs, selfMs, pct});
    }

    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.selfMs > b.selfMs; });

    bool isSpike = totalFrameMs >= gPerfBudget.spikeThresholdMs;

    // Write spike report for frames exceeding threshold
    if (isSpike) {
        perfWriteSpikeReport(totalFrameMs, budgetMs);
    }

    // Always write frame breakdown to rolling log (throttled)
    static int sFrameLogCounter = 0;
    sFrameLogCounter++;
    if (sFrameLogCounter % 60 == 0 || isSpike || DebugConfig::DEBUG_DEATH_PERF) {
        // Capture frame into ring buffer
        // Frame number is pulled from the spike report context
        // perfCaptureFrame is called from Perf::endFrame which has the frame number
    }
}

// ── Spike report ────────────────────────────────────────────

void perfWriteSpikeReport(double totalFrameMs, double budgetMs)
{
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);

    static FILE* sSpikeFile = nullptr;
    if (!sSpikeFile) {
        sSpikeFile = fopen("logs/FrameSpikes_log.txt", "a");
        if (sSpikeFile) {
            fprintf(sSpikeFile, "=== Frame Spike Log ===\n");
            fprintf(sSpikeFile, "Format: frame | total_ms | budget_ms | over_by | slowdown_x | label | self_ms | incl_ms | file:line\n");
            fprintf(sSpikeFile, "---\n");
        }
    }

    // Build entries
    struct Entry { int index; double inclMs; double selfMs; };
    std::vector<Entry> entries;
    entries.reserve(gPerfScopeCount);

    std::vector<double> childSums(gPerfScopeCount, 0.0);
    for (int i = 0; i < gPerfScopeCount; ++i) {
        double inclMs = (double)gPerfScopes[i].cyclesInclusive / 1000000.0;
        if (gPerfScopes[i].parentIndex >= 0 && gPerfScopes[i].parentIndex < gPerfScopeCount) {
            childSums[gPerfScopes[i].parentIndex] += inclMs;
        }
    }
    for (int i = 0; i < gPerfScopeCount; ++i) {
        double inclMs = (double)gPerfScopes[i].cyclesInclusive / 1000000.0;
        double selfMs = inclMs - childSums[i];
        if (selfMs < 0.0) selfMs = 0.0;
        entries.push_back({i, inclMs, selfMs});
    }

    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.selfMs > b.selfMs; });

    double overBy = totalFrameMs - budgetMs;
    double slowdown = budgetMs > 0.0 ? totalFrameMs / budgetMs : 0.0;

    if (sSpikeFile) {
        fprintf(sSpikeFile, "\n=== Frame Spike ===\n");
        fprintf(sSpikeFile, "Frame: unknown\n");
        fprintf(sSpikeFile, "Total frame time: %.3f ms\n", totalFrameMs);
        fprintf(sSpikeFile, "Target frame time: %.3f ms\n", budgetMs);
        fprintf(sSpikeFile, "Over budget: %.3f ms\n", overBy);
        fprintf(sSpikeFile, "Slowdown: %.2fx\n", slowdown);
        fprintf(sSpikeFile, "FPS equivalent: %.1f\n", totalFrameMs > 0.0 ? 1000.0 / totalFrameMs : 0.0);
        fprintf(sSpikeFile, "\n");

        // Top functions with full detail
        int topN = std::min(gPerfBudget.topFunctionsPerFrame, (int)entries.size());
        for (int i = 0; i < topN; ++i) {
            const Entry& e = entries[i];
            const PerfScopeCapture& cap = gPerfScopes[e.index];
            double inclPct = totalFrameMs > 0.0 ? (e.inclMs / totalFrameMs) * 100.0 : 0.0;
            double selfPct = totalFrameMs > 0.0 ? (e.selfMs / totalFrameMs) * 100.0 : 0.0;

            fprintf(sSpikeFile, "Function:\n");
            fprintf(sSpikeFile, "  %s\n", cap.file ? cap.file : "?");
            fprintf(sSpikeFile, "  %s\n", cap.func ? cap.func : "?");
            fprintf(sSpikeFile, "  Label: %s\n", cap.label ? cap.label : "?");
            fprintf(sSpikeFile, "  Line: %d\n", cap.line);
            if (cap.correlationId[0])
                fprintf(sSpikeFile, "  Correlation: %s\n", cap.correlationId);
            fprintf(sSpikeFile, "\n");

            fprintf(sSpikeFile, "  Inclusive time:\n");
            fprintf(sSpikeFile, "    %.3f ms\n", e.inclMs);
            fprintf(sSpikeFile, "    %.1f%% of total frame\n", inclPct);
            fprintf(sSpikeFile, "\n");

            fprintf(sSpikeFile, "  Self time:\n");
            fprintf(sSpikeFile, "    %.3f ms\n", e.selfMs);
            fprintf(sSpikeFile, "    %.1f%% of total frame\n", selfPct);
            fprintf(sSpikeFile, "\n");

            fprintf(sSpikeFile, "  Call count this frame:\n");
            fprintf(sSpikeFile, "    %u\n", cap.callCount);
            fprintf(sSpikeFile, "\n");

            // Children
            fprintf(sSpikeFile, "  Children:\n");
            int childCount = 0;
            double otherChildMs = 0.0;
            for (int j = 0; j < gPerfScopeCount; ++j) {
                if (gPerfScopes[j].parentIndex == e.index && gPerfScopes[j].active) {
                    double cInclMs = (double)gPerfScopes[j].cyclesInclusive / 1000000.0;
                    if (cInclMs > 0.01 && childCount < 20) {
                        fprintf(sSpikeFile, "    %s: %.3f ms",
                            gPerfScopes[j].label ? gPerfScopes[j].label : "?",
                            cInclMs);
                        if (gPerfScopes[j].correlationId[0])
                            fprintf(sSpikeFile, " [%s]", gPerfScopes[j].correlationId);
                        fprintf(sSpikeFile, "\n");
                        childCount++;
                    } else if (cInclMs > 0.01) {
                        otherChildMs += cInclMs;
                    }
                }
            }
            if (otherChildMs > 0.0)
                fprintf(sSpikeFile, "    Other children: %.3f ms\n", otherChildMs);
            fprintf(sSpikeFile, "\n");

            // Measured work
            fprintf(sSpikeFile, "  Measured work:\n");
            fprintf(sSpikeFile, "    Heap allocations: %u\n", cap.allocCount);
            fprintf(sSpikeFile, "    Alloc bytes: %u\n", cap.allocBytes);
            if (cap.assetLoadCount > 0)
                fprintf(sSpikeFile, "    Asset files loaded from disk: %u\n", cap.assetLoadCount);
            if (cap.collisionQueryCount > 0)
                fprintf(sSpikeFile, "    Collision queries: %u\n", cap.collisionQueryCount);
            if (cap.bloodParticlesSpawned > 0)
                fprintf(sSpikeFile, "    Blood particles: %u\n", cap.bloodParticlesSpawned);
            if (cap.debrisChunksSpawned > 0)
                fprintf(sSpikeFile, "    Debris chunks: %u\n", cap.debrisChunksSpawned);
            if (cap.bloodDecalsSpawned > 0)
                fprintf(sSpikeFile, "    Blood decals: %u\n", cap.bloodDecalsSpawned);
            if (cap.replayEventsCreated > 0)
                fprintf(sSpikeFile, "    Replay events created: %u\n", cap.replayEventsCreated);
            if (cap.replayJsonBytes > 0)
                fprintf(sSpikeFile, "    Replay JSON bytes: %u\n", cap.replayJsonBytes);
            fprintf(sSpikeFile, "\n");
        }

        // Summary
        fprintf(sSpikeFile, "--- SUMMARY ---\n");
        fprintf(sSpikeFile, "Frame=unknown total=%.3f budget=%.3f over=%.3f slowdown=%.2fx top=",
                totalFrameMs, budgetMs, overBy, slowdown);
        for (int i = 0; i < std::min(5, (int)entries.size()); ++i) {
            const PerfScopeCapture& cap = gPerfScopes[entries[i].index];
            fprintf(sSpikeFile, "%s=%.1fms ",
                    cap.label ? cap.label : "?", entries[i].selfMs);
        }
        fprintf(sSpikeFile, "\n");
        fflush(sSpikeFile);
    }

    // Log to console
    if (DebugConfig::DEBUG_DEATH_PERF) {
        for (int i = 0; i < std::min(10, (int)entries.size()); ++i) {
            const Entry& e = entries[i];
            const PerfScopeCapture& cap = gPerfScopes[e.index];
            Debug::log(Debug::Category::General,
                "[PERF SPIKE] total=%.1fms scope=%s self=%.1fms incl=%.1fms calls=%u allocs=%u bytes=%u file=%s:%d\n",
                totalFrameMs,
                cap.label ? cap.label : "?",
                e.selfMs, e.inclMs, cap.callCount,
                cap.allocCount, cap.allocBytes,
                cap.file ? cap.file : "?", cap.line);
        }
    }
}

// ── Flush all spike contexts ───────────────────────────────

void perfFlushAllSpikeContexts()
{
    // Placeholder — spike contexts are written inline during capture
}
