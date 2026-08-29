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

std::atomic<uint64_t> gPerfAllocCount{0};
std::atomic<uint64_t> gPerfAllocBytes{0};
std::atomic<uint64_t> gPerfLargestAlloc{0};
const char* gPerfLargestAllocSite = nullptr;

char gPerfCorrelationStack[8][32] = {};
int gPerfCorrelationDepth = 0;

uint64_t gPerfFrameStartCycles = 0;
uint32_t gPerfCurrentFrameId = 0;

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

    // Record frame/tick at scope creation
    cap.beginFrameId = gPerfCurrentFrameId;
    cap.beginTickId = 0;

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
    mAllocBefore = gPerfAllocCount.load(std::memory_order_relaxed);
    mBytesBefore = gPerfAllocBytes.load(std::memory_order_relaxed);
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

    // Frame-boundary detection: if this scope started in a different frame,
    // mark it as cross-frame and exclude from synchronous frame accounting.
    cap.crossedBoundary = (gPerfCurrentFrameId > 0 && cap.beginFrameId > 0 &&
                           cap.beginFrameId != gPerfCurrentFrameId);
    if (cap.crossedBoundary && DebugConfig::DEBUG_DEATH_PERF) {
        Debug::log(Debug::Category::General,
            "[PERF CROSS-FRAME] scope=%s frame=%d→%d\n",
            cap.label ? cap.label : "?",
            cap.beginFrameId, gPerfCurrentFrameId);
    }
    cap.cyclesInclusive += elapsed;
    if (elapsed < cap.minCycles) cap.minCycles = elapsed;
    if (elapsed > cap.maxCycles) cap.maxCycles = elapsed;
    cap.callCount++;

    // Record counters — uint64_t snapshot delta, no clamp needed
    const uint64_t currentCount = gPerfAllocCount.load(std::memory_order_relaxed);
    const uint64_t currentBytes = gPerfAllocBytes.load(std::memory_order_relaxed);
    const uint64_t allocDelta = currentCount - mAllocBefore;
    const uint64_t bytesDelta = currentBytes - mBytesBefore;
    cap.allocCount += allocDelta;
    cap.allocBytes += bytesDelta;
    if (bytesDelta > 0 && allocDelta == 0) {
        Debug::warn(Debug::Category::General,
            "[PERF][ALLOC][WARN] bytes increased without count increase; "
            "investigate allocation hook mismatch");
    }

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

    // Compute measured top-level work: find the root scope (parentIndex < 0)
    // that is NOT cross-frame, and use its inclusive time.
    double measuredTopLevel = 0.0;
    double measuredTopLevelCrossed = 0.0;
    for (const auto& e : entries) {
        const PerfScopeCapture& cap = gPerfScopes[e.index];
        if (cap.parentIndex < 0) {
            if (cap.crossedBoundary) {
                measuredTopLevelCrossed += e.inclMs;
            } else {
                measuredTopLevel += e.inclMs;
            }
        }
    }
    // If there are multiple non-crossed roots, take the largest.
    // Single root (EngineTick) is the normal case.
    int rootCount = 0;
    int crossedRootCount = 0;
    double maxRoot = 0.0;
    for (const auto& e : entries) {
        const PerfScopeCapture& cap = gPerfScopes[e.index];
        if (cap.parentIndex < 0 && !cap.crossedBoundary) {
            rootCount++;
            if (e.inclMs > maxRoot) maxRoot = e.inclMs;
        }
        if (cap.parentIndex < 0 && cap.crossedBoundary)
            crossedRootCount++;
    }
    if (rootCount > 1)
        measuredTopLevel = maxRoot;
    else if (rootCount == 0)
        measuredTopLevel = 0.0;
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
        "Accounted: %.1f%%\n", accountedPct);
    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
        "Cross-frame roots: %.3f ms\n\n", measuredTopLevelCrossed);

    // Chronological timeline: scopes in order of start time (by entry index order,
    // which follows creation order within the frame)
    {
        pos += std::snprintf(msg + pos, sizeof(msg) - pos,
            "--- TIMELINE ---\n");
        double currentTime = 0.0;
        for (int ti = 0; ti < (int)entries.size() && ti < 30; ++ti) {
            const PerfScopeCapture& cap = gPerfScopes[entries[ti].index];
            if (cap.parentIndex >= 0) continue; // skip children (shown under parent)
            double inclMs = entries[ti].inclMs;
            if (inclMs < 0.01) continue;
            double startTime = currentTime;
            double endTime = startTime + inclMs;
            currentTime = endTime;
            const char* flag = cap.crossedBoundary ? " [CROSS-FRAME]" : "";
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "  %.2f–%.2fms: %s (%.2fms)%s\n",
                startTime, endTime, cap.label ? cap.label : "?", inclMs, flag);
        }
        // Unaccounted gap
        double timelineTotal = currentTime;
        double gap = totalFrameMs - timelineTotal;
        if (gap > 0.25) {
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "  %.2f–%.2fms: UNACCOUNTED (%.2fms)\n",
                timelineTotal, totalFrameMs, gap);
        }
        pos += std::snprintf(msg + pos, sizeof(msg) - pos, "\n");
    }

    // Cross-frame scope summary
    {
        bool hasCrossed = false;
        for (const auto& e : entries) {
            if (gPerfScopes[e.index].crossedBoundary) {
                if (!hasCrossed) {
                    pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                        "--- CROSS-FRAME SCOPES ---\n");
                    hasCrossed = true;
                }
                const PerfScopeCapture& cap = gPerfScopes[e.index];
                pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                    "  %s: start=frame %d %.2fms\n",
                    cap.label ? cap.label : "?",
                    cap.beginFrameId, e.inclMs);
            }
        }
        if (hasCrossed)
            pos += std::snprintf(msg + pos, sizeof(msg) - pos, "\n");
    }

    // Allocation consistency check
    {
        bool allocInconsistent = false;
        for (const auto& e : entries) {
            const PerfScopeCapture& cap = gPerfScopes[e.index];
            if (cap.allocCount == 0 && cap.allocBytes > 0) {
                allocInconsistent = true;
                break;
            }
        }
        if (allocInconsistent) {
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "--- ALLOCATION WARNING ---\n");
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "Scope(s) with allocCount=0 but allocBytes>0 detected.\n");
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "Allocation deltas may be invalid (counters not monotonic).\n\n");
        } else {
            pos += std::snprintf(msg + pos, sizeof(msg) - pos,
                "Allocation counters: CONSISTENT\n\n");
        }
    }

    // Top functions with full detail. Always include the root scope first.
    int topN = std::min(gPerfBudget.topFunctionsPerFrame, (int)entries.size());

    // Build ordered list: root first (highest inclusive, zero parent), then top by self time
    struct OrderedEntry { int srcIdx; double selfMs; };
    std::vector<OrderedEntry> ordered;
    ordered.reserve(topN + 1);

    // Find root
    int rootIdx = -1;
    for (int ei = 0; ei < (int)entries.size(); ++ei) {
        if (gPerfScopes[entries[ei].index].parentIndex < 0) {
            if (rootIdx < 0 || entries[ei].inclMs > entries[rootIdx].inclMs)
                rootIdx = ei;
        }
    }
    if (rootIdx >= 0)
        ordered.push_back({rootIdx, entries[rootIdx].selfMs});

    // Add remaining top scopes by self time, excluding root
    for (int i = 0; i < topN && (int)ordered.size() < topN; ++i) {
        if (i != rootIdx)
            ordered.push_back({i, entries[i].selfMs});
    }

    for (int oi = 0; oi < (int)ordered.size(); ++oi) {
        const ScopeEntry& e = entries[ordered[oi].srcIdx];
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
            "    Allocations: %llu (%llu bytes)\n",
            (unsigned long long)cap.allocCount, (unsigned long long)cap.allocBytes);
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
    bool allocInstrumented = (gPerfAllocCount.load(std::memory_order_relaxed) > 0);

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
