#pragma once

#include <atomic>
#include <cstddef>
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
    char correlationId[32] = {};

    // Frame and tick at scope begin
    uint32_t beginFrameId = 0;
    uint64_t beginTickId = 0;

    // Accumulated nanoseconds for this scope across all calls this frame
    uint64_t cyclesInclusive = 0;
    uint64_t cyclesSelf = 0;
    uint64_t minCycles = UINT64_MAX;
    uint64_t maxCycles = 0;
    uint32_t callCount = 0;

    // Counters
    uint64_t allocCount = 0;
    uint64_t allocBytes = 0;
    uint32_t assetLoadCount = 0;
    uint32_t collisionQueryCount = 0;

    // Per-death counters
    uint32_t bloodParticlesSpawned = 0;
    uint32_t debrisChunksSpawned = 0;
    uint32_t bloodDecalsSpawned = 0;

    // Per-replay counters
    uint32_t replayEventsCreated = 0;
    uint32_t replayJsonBytes = 0;

    int parentIndex = -1;
    bool active = false;
    bool crossedBoundary = false;
};

struct PerfProgressSnapshot {
    const char* file = nullptr;
    const char* function = nullptr;
    const char* label = nullptr;
    int line = 0;
    uint32_t frameId = 0;
    uint32_t threadId = 0;
    uint32_t depth = 0;
    uint64_t startNanoseconds = 0;
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

// Allocation tracking globals
extern std::atomic<uint64_t> gPerfAllocCount;
extern std::atomic<uint64_t> gPerfAllocBytes;
extern std::atomic<uint64_t> gPerfLargestAlloc;
extern const char* gPerfLargestAllocSite;

constexpr std::size_t PERF_THREAD_ALLOCATION_SLOT_COUNT = 128;
struct PerfThreadAllocationCounter {
    std::atomic<uint32_t> threadId{0};
    std::atomic<uint64_t> allocationCount{0};
    std::atomic<uint64_t> allocationBytes{0};
};
extern PerfThreadAllocationCounter gPerfThreadAllocations[PERF_THREAD_ALLOCATION_SLOT_COUNT];
extern std::atomic<uint64_t> gPerfThreadAllocationUntracked;
void perfRecordThreadAllocation(std::size_t size);

// Correlation ID stack
extern char gPerfCorrelationStack[8][32];
extern int gPerfCorrelationDepth;

// ── RAII scope guard ────────────────────────────────────────

class PerfScopeGuard {
public:
    PerfScopeGuard(const char* file, int line, const char* func, const char* label);
    ~PerfScopeGuard();
    PerfScopeGuard(const PerfScopeGuard&) = delete;
    PerfScopeGuard& operator=(const PerfScopeGuard&) = delete;

private:
    int mScopeIndex = -1;
    uint64_t mStartCycles = 0;
    uint64_t mAllocBefore = 0;
    uint64_t mBytesBefore = 0;
    uint32_t mAssetLoadBefore = 0;
    uint32_t mBloodBefore = 0;
    uint32_t mDebrisBefore = 0;
    uint32_t mDecalBefore = 0;
    uint32_t mReplayEventsBefore = 0;
    uint32_t mReplayBytesBefore = 0;

    static uint64_t readCycles();
};

class PerfProgressGuard {
public:
    PerfProgressGuard(const char* file, int line, const char* function, const char* label);
    ~PerfProgressGuard();
    PerfProgressGuard(const PerfProgressGuard&) = delete;
    PerfProgressGuard& operator=(const PerfProgressGuard&) = delete;

private:
    PerfProgressSnapshot mPrevious;
    bool mHadPrevious = false;
    bool mTracked = false;
};

// ── Convenience macro ───────────────────────────────────────

#define MIMITA_PERF_SCOPE_IMPL(label, uniqueId) \
    PerfScopeGuard MIMITA_PERF_GUARD_##uniqueId( \
        __FILE__, __LINE__, __FUNCTION__, label)
#define MIMITA_PERF_SCOPE_EXPAND(label, uniqueId) \
    MIMITA_PERF_SCOPE_IMPL(label, uniqueId)
#define MIMITA_PERF_SCOPE(label) \
    MIMITA_PERF_SCOPE_EXPAND(label, __COUNTER__)

#define MIMITA_PERF_PROGRESS_IMPL(label, uniqueId) \
    PerfProgressGuard MIMITA_PERF_PROGRESS_GUARD_##uniqueId( \
        __FILE__, __LINE__, __FUNCTION__, label)
#define MIMITA_PERF_PROGRESS_EXPAND(label, uniqueId) \
    MIMITA_PERF_PROGRESS_IMPL(label, uniqueId)
#define MIMITA_PERF_PROGRESS(label) \
    MIMITA_PERF_PROGRESS_EXPAND(label, __COUNTER__)

// ── API ─────────────────────────────────────────────────────

void perfAggregateScopes(double totalFrameMs, double budgetMs, int frameNumber);
void perfWriteSpikeReport(double totalFrameMs, double budgetMs, int frameNumber);
void perfSetProgressMonitorThread(uint32_t threadId);
bool perfReadProgressSnapshot(PerfProgressSnapshot& snapshot);

// Correlation ID management
void perfSetCorrelation(const char* id);
void perfClearCorrelation();

// Flush any pending spike contexts
void perfFlushAllSpikeContexts();

// Frame boundary timestamp (set by Perf::beginFrame) for scope cross-frame detection
extern uint64_t gPerfFrameStartCycles;

// Current frame ID (set by Perf::beginFrame from PerfState)
extern uint32_t gPerfCurrentFrameId;
