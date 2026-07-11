#include "perf/perf-spike.h"
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

    // Determine parent from stack
    if (gPerfScopeStackDepth > 0) {
        mPrevParentIndex = gPerfScopeStack[gPerfScopeStackDepth - 1];
        cap.parentIndex = mPrevParentIndex;
    } else {
        cap.parentIndex = -1;
    }

    // Push to stack
    if (gPerfScopeStackDepth < MAX_SCOPES_PER_FRAME) {
        gPerfScopeStack[gPerfScopeStackDepth++] = mScopeIndex;
    }

    // Capture pre-scope counters
    mAllocBefore = 0;
    mAssetLoadBefore = 0;
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

    // Record child time: subtract from parent's self time
    if (cap.parentIndex >= 0 && cap.parentIndex < MAX_SCOPES_PER_FRAME) {
        PerfScopeCapture& parent = gPerfScopes[cap.parentIndex];
        // Add elapsed to parent's child time (tracked via cyclesSelf subtraction later)
    }

    // Pop from stack
    if (gPerfScopeStackDepth > 0) {
        gPerfScopeStackDepth--;
    }
}

// ── Frame aggregation ───────────────────────────────────────

struct SortedScope {
    int index;
    const PerfScopeCapture* cap;
    double inclMs;
    double selfMs;
    double pctOfTotal;
};

void perfAggregateScopes(double totalFrameMs, double budgetMs)
{
    if (gPerfScopeCount == 0)
        return;

    // Build child-time map: for each scope, sum inclusive time of direct children
    // We compute self time = inclusive - sum of children's inclusive
    std::vector<double> childTime(gPerfScopeCount, 0.0);
    for (int i = 0; i < gPerfScopeCount; ++i) {
        const PerfScopeCapture& cap = gPerfScopes[i];
        if (cap.parentIndex >= 0 && cap.parentIndex < gPerfScopeCount) {
            // parent's self time does not include this child's inclusive time
        }
    }

    // Compute self: for each scope, sum child inclusive times and subtract
    std::vector<double> childInclusiveSum(gPerfScopeCount, 0.0);
    for (int i = 0; i < gPerfScopeCount; ++i) {
        const PerfScopeCapture& cap = gPerfScopes[i];
        if (cap.parentIndex >= 0 && cap.parentIndex < gPerfScopeCount) {
            double inclMs = (double)cap.cyclesInclusive / 1000000.0;
            childInclusiveSum[cap.parentIndex] += inclMs;
        }
    }

    // Convert cycles to ms (1 cycle = 1 nanosecond for steady_clock)
    // and build sorted list
    std::vector<SortedScope> sorted;
    sorted.reserve(gPerfScopeCount);

    for (int i = 0; i < gPerfScopeCount; ++i) {
        const PerfScopeCapture& cap = gPerfScopes[i];
        double inclMs = (double)cap.cyclesInclusive / 1000000.0;
        double selfMs = inclMs - childInclusiveSum[i];
        if (selfMs < 0.0) selfMs = 0.0;
        double pct = totalFrameMs > 0.0 ? (inclMs / totalFrameMs) * 100.0 : 0.0;
        sorted.push_back({i, &cap, inclMs, selfMs, pct});
    }

    // Sort by self time descending
    std::sort(sorted.begin(), sorted.end(),
        [](const SortedScope& a, const SortedScope& b) {
            return a.selfMs > b.selfMs;
        });

    // Check if any scope exceeded spike threshold
    double maxSelfMs = sorted.empty() ? 0.0 : sorted[0].selfMs;
    bool isSpike = totalFrameMs >= gPerfBudget.spikeThresholdMs;

    if (!isSpike && !DebugConfig::DEBUG_DEATH_PERF)
        return;

    // ── Write spike report ────────────────────────────────
    perfWriteSpikeReport(totalFrameMs, budgetMs);
}

void perfWriteSpikeReport(double totalFrameMs, double budgetMs)
{
    // Build log path
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);

    // Open frame spikes log
    static int sFrameSpikeLogOpened = 0;
    static FILE* sSpikeFile = nullptr;
    if (!sSpikeFile) {
        sSpikeFile = fopen("logs/FrameSpikes_log.txt", "a");
        if (sSpikeFile) {
            fprintf(sSpikeFile, "=== Frame Spike Log ===\n");
            fprintf(sSpikeFile, "Format: frame | total_ms | budget_ms | over_by | slowdown_x | label | self_ms | incl_ms | file:line\n");
            fprintf(sSpikeFile, "---\n");
            sFrameSpikeLogOpened = 1;
        }
    }

    // Build sorted list
    struct Entry {
        int index;
        double inclMs;
        double selfMs;
    };
    std::vector<Entry> entries;
    entries.reserve(gPerfScopeCount);

    struct ChildSum { double incl; };
    std::vector<ChildSum> childSums(gPerfScopeCount, {0.0});

    for (int i = 0; i < gPerfScopeCount; ++i) {
        double inclMs = (double)gPerfScopes[i].cyclesInclusive / 1000000.0;
        if (gPerfScopes[i].parentIndex >= 0 && gPerfScopes[i].parentIndex < gPerfScopeCount) {
            childSums[gPerfScopes[i].parentIndex].incl += inclMs;
        }
    }

    for (int i = 0; i < gPerfScopeCount; ++i) {
        double inclMs = (double)gPerfScopes[i].cyclesInclusive / 1000000.0;
        double selfMs = inclMs - childSums[i].incl;
        if (selfMs < 0.0) selfMs = 0.0;
        entries.push_back({i, inclMs, selfMs});
    }

    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.selfMs > b.selfMs; });

    double overBy = totalFrameMs - budgetMs;
    double slowdown = budgetMs > 0.0 ? totalFrameMs / budgetMs : 0.0;

    // Write to spike file
    if (sSpikeFile) {
        // Header
        fprintf(sSpikeFile, "\n=== Frame Spike ===\n");
        fprintf(sSpikeFile, "Frame: %d\n", 0);
        fprintf(sSpikeFile, "Total frame time: %.3f ms\n", totalFrameMs);
        fprintf(sSpikeFile, "Target frame time: %.3f ms\n", budgetMs);
        fprintf(sSpikeFile, "Over budget: %.3f ms\n", overBy);
        fprintf(sSpikeFile, "Slowdown: %.2fx\n", slowdown);
        fprintf(sSpikeFile, "FPS equivalent: %.1f\n", totalFrameMs > 0.0 ? 1000.0 / totalFrameMs : 0.0);
        fprintf(sSpikeFile, "\n");

        // Top functions
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
                        fprintf(sSpikeFile, "    %s: %.3f ms\n",
                            gPerfScopes[j].label ? gPerfScopes[j].label : "?",
                            cInclMs);
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
            if (cap.allocCount > 0 || cap.assetLoadCount > 0 || cap.collisionQueryCount > 0) {
                fprintf(sSpikeFile, "  Measured work:\n");
                if (cap.allocCount > 0)
                    fprintf(sSpikeFile, "    Heap allocations: %u\n", cap.allocCount);
                if (cap.assetLoadCount > 0)
                    fprintf(sSpikeFile, "    Asset files loaded from disk: %u\n", cap.assetLoadCount);
                if (cap.collisionQueryCount > 0)
                    fprintf(sSpikeFile, "    Collision queries: %u\n", cap.collisionQueryCount);
                fprintf(sSpikeFile, "\n");
            }
        }

        // Summary line for quick grepping
        fprintf(sSpikeFile, "--- SUMMARY ---\n");
        fprintf(sSpikeFile, "Frame=0 total=%.3f budget=%.3f over=%.3f slowdown=%.2fx top=",
                totalFrameMs, budgetMs, overBy, slowdown);
        for (int i = 0; i < std::min(5, (int)entries.size()); ++i) {
            const PerfScopeCapture& cap = gPerfScopes[entries[i].index];
            fprintf(sSpikeFile, "%s=%.1fms ",
                    cap.label ? cap.label : "?",
                    entries[i].selfMs);
        }
        fprintf(sSpikeFile, "\n");
        fflush(sSpikeFile);
    }

    // Also log to structured logger if available
    if (DebugConfig::DEBUG_DEATH_PERF) {
        for (int i = 0; i < std::min(10, (int)entries.size()); ++i) {
            const Entry& e = entries[i];
            const PerfScopeCapture& cap = gPerfScopes[e.index];
            Debug::log(Debug::Category::General,
                "[PERF SPIKE] frame=%d total=%.1fms scope=%s self=%.1fms incl=%.1fms calls=%u file=%s:%d\n",
                0, totalFrameMs,
                cap.label ? cap.label : "?",
                e.selfMs, e.inclMs, cap.callCount,
                cap.file ? cap.file : "?", cap.line);
        }
    }
}
