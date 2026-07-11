#include "perf/perf-spike.h"
#include "perf/perf-frame.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "config.h"
#include <algorithm>

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

uint64_t gPerfFrameStartCycles = 0;

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

    // Frame-boundary detection: if this scope started before the current frame,
    // it spans across frames. Cap the measurement to avoid impossible times.
    if (gPerfFrameStartCycles > 0 && mStartCycles < gPerfFrameStartCycles) {
        // Started in a previous frame. Use only the within-frame portion.
        elapsed = endCycles - gPerfFrameStartCycles;
        // If still impossibly large, clamp to 16ms (one budget frame)
        if (elapsed > 16000000ULL) elapsed = 16000000ULL;
    }

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

void perfAggregateScopes(double totalFrameMs, double budgetMs, int frameNumber)
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
        perfWriteSpikeReport(totalFrameMs, budgetMs, frameNumber);
    }

    // Always write frame breakdown to rolling log (throttled)
    static int sFrameLogCounter = 0;
    sFrameLogCounter++;
    if (sFrameLogCounter % 60 == 0 || isSpike || DebugConfig::DEBUG_DEATH_PERF) {
        // perfCaptureFrame is called from Perf::endFrame which handles the ring buffer
    }
}

// ── Spike report (routed through StructuredLogger) ─────────

void perfWriteSpikeReport(double totalFrameMs, double budgetMs, int frameNumber)
{
    // Build entries
    struct ScopeEntry { int index; double inclMs; double selfMs; };
    std::vector<ScopeEntry> entries;
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
        [](const ScopeEntry& a, const ScopeEntry& b) { return a.selfMs > b.selfMs; });

    double overBy = totalFrameMs - budgetMs;
    double slowdown = budgetMs > 0.0 ? totalFrameMs / budgetMs : 0.0;

    // Compute measured top-level work (root scopes only, no parent)
    double measuredTopLevel = 0.0;
    for (const auto& e : entries) {
        const PerfScopeCapture& cap = gPerfScopes[e.index];
        if (cap.parentIndex < 0) {
            measuredTopLevel += e.selfMs;
        }
    }
    double unaccounted = totalFrameMs - measuredTopLevel;
    double accountedPct = totalFrameMs > 0.0 ? (measuredTopLevel / totalFrameMs) * 100.0 : 0.0;

    // Build a plain-text report string to send to StructuredLogger
    char msg[8192];
    int pos = 0;
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "SPIKE DETECTED\n");
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Frame: %d\n", frameNumber);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Frame total: %.3f ms\n", totalFrameMs);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Budget: %.3f ms\n", budgetMs);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Over budget: %.3f ms\n", overBy);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Slowdown: %.2fx\n", slowdown);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "FPS equivalent: %.1f\n", totalFrameMs > 0.0 ? 1000.0 / totalFrameMs : 0.0);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Measured top-level work: %.3f ms\n", measuredTopLevel);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Unaccounted: %.3f ms\n", unaccounted);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Accounted: %.1f%%\n\n", accountedPct);

    // Top functions with full detail
    int topN = std::min(gPerfBudget.topFunctionsPerFrame, (int)entries.size());
    for (int i = 0; i < topN; ++i) {
        const ScopeEntry& e = entries[i];
        const PerfScopeCapture& cap = gPerfScopes[e.index];
        double inclPct = totalFrameMs > 0.0 ? (e.inclMs / totalFrameMs) * 100.0 : 0.0;
        double selfPct = totalFrameMs > 0.0 ? (e.selfMs / totalFrameMs) * 100.0 : 0.0;

        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "Function: %s\n", cap.label ? cap.label : "?");
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "  File: %s\n", cap.file ? cap.file : "?");
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "  Line: %d\n", cap.line);
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "  Function: %s\n", cap.func ? cap.func : "?");
        if (cap.correlationId[0])
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "  Correlation: %s\n", cap.correlationId);
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "  Inclusive: %.3f ms (%.1f%%)\n", e.inclMs, inclPct);
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "  Self: %.3f ms (%.1f%%)\n", e.selfMs, selfPct);
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "  Calls: %u\n", cap.callCount);

        // Children
        bool hasChildren = false;
        for (int j = 0; j < gPerfScopeCount; ++j) {
            if (gPerfScopes[j].parentIndex == e.index && gPerfScopes[j].active) {
                if (!hasChildren) {
                    pos += std::snprintf(msg + pos, sizeof(msg) - pos, "  Children:\n");
                    hasChildren = true;
                }
                double cInclMs = (double)gPerfScopes[j].cyclesInclusive / 1000000.0;
                if (cInclMs > 0.01) {
                    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                        "    %s: %.3f ms",
                        gPerfScopes[j].label ? gPerfScopes[j].label : "?",
                        cInclMs);
                    if (gPerfScopes[j].correlationId[0])
                        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                            " [%s]", gPerfScopes[j].correlationId);
                    pos += std::snprintf(msg + pos, sizeof(msg) - pos, "\n");
                }
            }
        }

        // Measured work
        pos += std::snprintf(msg + pos, sizeof(msg) - pos, "  Measured work:\n");
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "    Allocations: %u (%u bytes)\n", cap.allocCount, cap.allocBytes);
        if (cap.assetLoadCount > 0)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "    Asset loads: %u\n", cap.assetLoadCount);
        if (cap.collisionQueryCount > 0)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "    Collision queries: %u\n", cap.collisionQueryCount);
        if (cap.bloodParticlesSpawned > 0)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "    Blood particles: %u\n", cap.bloodParticlesSpawned);
        if (cap.debrisChunksSpawned > 0)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "    Debris chunks: %u\n", cap.debrisChunksSpawned);
        if (cap.bloodDecalsSpawned > 0)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "    Blood decals: %u\n", cap.bloodDecalsSpawned);
        if (cap.replayEventsCreated > 0)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "    Replay events: %u\n", cap.replayEventsCreated);
        pos += std::snprintf(msg + pos, sizeof(msg) - pos, "\n");
    }

    // Compute allocation status
    bool allocInstrumented = (gPerfAllocCount > 0);

    // Send to StructuredLogger
    StructuredLogger::Entry e;
    e.category = StructuredCategory::Performance;
    e.level = StructuredLevel::Errors;
    e.eventId = "PERFORMANCE_SPIKE";
    e.correlationId = "";
    if (gPerfCorrelationDepth > 0)
        e.correlationId = gPerfCorrelationStack[gPerfCorrelationDepth - 1];
    e.reason = "Frame exceeded budget";
    e.sourceFile = __FILE__;
    e.sourceLine = __LINE__;
    e.functionName = "perfWriteSpikeReport";
    e.frame = (uint32_t)(frameNumber > 0 ? frameNumber : 0);
    e.message = std::string(msg) + std::string("Allocation instrumentation: ") +
        (allocInstrumented ? "MEASURED" : "NOT INSTRUMENTED") + "\n" +
        "Measurement source: MIMITA_PERF_SCOPE + operator new override\n" +
        "Status: " + (accountedPct >= 95.0 ? "PASS\n" : "PROFILER COVERAGE FAILURE\n");

    StructuredLogger::instance().write(e);

    // Also log to console
    if (DebugConfig::DEBUG_DEATH_PERF) {
        Debug::log(Debug::Category::General,
            "[PERF SPIKE] frame=%d total=%.1fms accounted=%.1f%% top=%s=%.1fms\n",
            frameNumber, totalFrameMs, accountedPct,
            entries.empty() ? "?" : (gPerfScopes[entries[0].index].label ? gPerfScopes[entries[0].index].label : "?"),
            entries.empty() ? 0.0 : entries[0].selfMs);
    }
}

// ── Flush all spike contexts ───────────────────────────────

void perfFlushAllSpikeContexts()
{
    // All spike contexts are written inline during capture - no deferred flush needed
}
