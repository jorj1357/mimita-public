#pragma once

#include <cstdint>
#include <cstdio>
#include <chrono>

// ── Per-scope capture record ─────────────────────────────────
// One record per unique scope per frame. Cleared at beginFrame.

struct PerfScopeCapture {
    const char* file = nullptr;
    int line = 0;
    const char* func = nullptr;
    const char* label = nullptr;

    // Accumulated cycles for this scope across all calls this frame
    uint64_t cyclesInclusive = 0;
    uint64_t cyclesSelf = 0;
    // Min/max over individual calls this frame
    uint64_t minCycles = UINT64_MAX;
    uint64_t maxCycles = 0;
    uint32_t callCount = 0;

    // Counters collected during scope
    uint32_t allocCount = 0;
    uint32_t assetLoadCount = 0;
    uint32_t collisionQueryCount = 0;

    int parentIndex = -1;   // index of parent scope, -1 for root
    bool active = false;    // slot in use
};

// ── Budget config (loaded from debuglogger.json) ─────────────

struct PerfBudgetConfig {
    int targetFps = 60;
    double spikeThresholdMs = 20.0;
    double severeThresholdMs = 100.0;
    double catastrophicThresholdMs = 1000.0;
    bool captureCallTreeOnSpike = true;
    int topFunctionsPerFrame = 30;
    int historyFramesBeforeSpike = 120;
    int historyFramesAfterSpike = 180;
    int aggregateWindowFrames = 600;
    bool logAllocations = true;
    bool logAssetIO = true;
    bool logCollisionQueries = true;
    bool logEntityCounts = true;
    bool logEffectCounts = true;
    bool logRenderCounts = true;
    bool enabled = false;
};

// ── Global state ────────────────────────────────────────────

static constexpr int MAX_SCOPES_PER_FRAME = 1024;

extern PerfScopeCapture gPerfScopes[MAX_SCOPES_PER_FRAME];
extern int gPerfScopeCount;
extern PerfBudgetConfig gPerfBudget;
extern int gPerfScopeStack[MAX_SCOPES_PER_FRAME];
extern int gPerfScopeStackDepth;

// ── RAII scope guard ────────────────────────────────────────

class PerfScopeGuard {
public:
    PerfScopeGuard(const char* file, int line, const char* func, const char* label);
    ~PerfScopeGuard();

    // Not copyable, not movable
    PerfScopeGuard(const PerfScopeGuard&) = delete;
    PerfScopeGuard& operator=(const PerfScopeGuard&) = delete;

private:
    int mScopeIndex = -1;
    int mPrevParentIndex = -1;
    uint64_t mStartCycles = 0;
    uint32_t mAllocBefore = 0;
    uint32_t mAssetLoadBefore = 0;

    static uint64_t readCycles();
};

// ── Convenience macro ───────────────────────────────────────

#define MIMITA_PERF_SCOPE(label) \
    PerfScopeGuard MIMITA_PERF_GUARD_##__LINE__( \
        __FILE__, __LINE__, __FUNCTION__, label)

// ── API ─────────────────────────────────────────────────────

// Call from Perf::endFrame() to aggregate scopes and generate spike report
void perfAggregateScopes(double totalFrameMs, double budgetMs);

// Write enhanced spike report to file
void perfWriteSpikeReport(double totalFrameMs, double budgetMs);
