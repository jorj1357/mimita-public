#include "perf/perf-frame.h"
#include "perf/perf-spike.h"
#include "perf/perf.h"
#include "perf/perf-gpu.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "debug/crash-handler.h"
#include "config.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <windows.h>
#include <psapi.h>
#include <nlohmann/json.hpp>

namespace {

struct WindowScopeStats {
    char name[64] = {};
    uint64_t calls = 0;
    double totalMs = 0.0;
    double maxMs = 0.0;
    double selfMs = 0.0;
    uint64_t allocCount = 0;
    uint64_t allocBytes = 0;
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
    uint64_t broadphaseQueries = 0;
    uint64_t repeatedQueries = 0;
    uint64_t chunkCellsVisited = 0;
    uint64_t uniqueTriangles = 0;
    uint64_t triangleTests = 0;
    uint64_t visibleMeshes = 0;
    uint64_t visibleTriangles = 0;
    uint64_t totalDrawCalls = 0;
    int maxBroadphaseQueries = 0;
    int maxUniqueTriangles = 0;
    int maxVisibleMeshes = 0;
    int maxVisibleTriangles = 0;
    std::array<double, 60> frameTimesMs{};
    int framesOverBudget = 0;
    double frameBudgetMs = 0.0;
    double gpuFrameMsTotal = 0.0;
    double gpuFrameMsMax = 0.0;
    uint64_t gpuFrameSamples = 0;
    uint64_t scopeEntriesDropped = 0;
    uint64_t scopeCaptureCount = 0;
    int maxScopeCaptures = 0;
    int framesScopeCaptureSaturated = 0;
    uint64_t mainThreadCpuStart100ns = 0;
    bool mainThreadCpuAvailable = false;
    WindowScopeStats scopes[128];
    int scopeCount = 0;
};

ResourceWindow gResourceWindow;

std::atomic<bool> gResourceWatchdogStop{false};
std::atomic<uint32_t> gResourceWatchdogMainThreadId{0};
std::atomic<uint32_t> gLastCompletedFrameNumber{0};
std::atomic<uint64_t> gLastCompletedFrameNanoseconds{0};
std::thread gResourceWatchdogThread;

uint64_t steadyNowNanoseconds()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

double threadCpuMilliseconds(uint32_t threadId)
{
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId);
    if (!thread)
        return -1.0;
    FILETIME creation{}, exit{}, kernel{}, user{};
    const bool ok = GetThreadTimes(thread, &creation, &exit, &kernel, &user) != 0;
    CloseHandle(thread);
    if (!ok)
        return -1.0;
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return static_cast<double>(k.QuadPart + u.QuadPart) / 10000.0;
}

void resourceWatchdogMain()
{
    bool stallReported = false;
    bool stackAttempted = false;
    uint64_t nextReportNs = 0;
    while (!gResourceWatchdogStop.load(std::memory_order_relaxed)) {
        Sleep(250);
        const uint32_t threadId = gResourceWatchdogMainThreadId.load(
            std::memory_order_relaxed);
        const uint64_t lastFrameNs = gLastCompletedFrameNanoseconds.load(
            std::memory_order_relaxed);
        if (threadId == 0 || lastFrameNs == 0)
            continue;
        const uint64_t nowNs = steadyNowNanoseconds();
        const uint64_t ageMs = nowNs >= lastFrameNs ? (nowNs - lastFrameNs) / 1000000 : 0;
        if (ageMs < 1500) {
            stallReported = false;
            stackAttempted = false;
            continue;
        }
        if (nowNs < nextReportNs)
            continue;
        nextReportNs = nowNs + 1000000000ULL;
        PerfProgressSnapshot progress;
        const bool hasProgress = perfReadProgressSnapshot(progress) &&
            progress.threadId == threadId;
        const double cpuMs = threadCpuMilliseconds(threadId);
        const uint32_t frame = gLastCompletedFrameNumber.load(std::memory_order_relaxed);
        StructuredLogger::instance().tryWriteEvent(
            StructuredCategory::Performance, StructuredLevel::Important,
            "performance.stall-watchdog", "performance",
            stallReported ? "main thread still has not completed a frame" :
                            "main thread frame completion stalled",
            frame,
            nlohmann::json{
                {"main_thread_id", threadId}, {"last_completed_frame", frame},
                {"frame_heartbeat_age_ms", ageMs}, {"main_thread_cpu_ms", cpuMs},
                {"progress_available", hasProgress},
                {"progress_label", hasProgress && progress.label ? progress.label : ""},
                {"progress_file", hasProgress && progress.file ? progress.file : ""},
                {"progress_line", hasProgress ? progress.line : 0},
                {"progress_function", hasProgress && progress.function ? progress.function : ""},
                {"progress_frame_id", hasProgress ? progress.frameId : 0},
                {"progress_depth", hasProgress ? progress.depth : 0},
                {"progress_age_ms", hasProgress && nowNs >= progress.startNanoseconds
                    ? (nowNs - progress.startNanoseconds) / 1000000 : 0}},
            __FILE__, __LINE__, __FUNCTION__);
        stallReported = true;
        if (!stackAttempted) {
            stackAttempted = true;
            char stackTrace[4096] = {};
            StructuredLogger::instance().tryWriteEvent(
                StructuredCategory::Performance, StructuredLevel::Important,
                "performance.stall-stack-capture", "performance",
                "begin main-thread stack capture", frame,
                nlohmann::json{{"phase", "begin"}, {"main_thread_id", threadId},
                               {"frame_heartbeat_age_ms", ageMs},
                               {"last_completed_frame", frame},
                               {"main_thread_cpu_ms", cpuMs}},
                __FILE__, __LINE__, __FUNCTION__);
            const bool captured = captureThreadStackTrace(threadId, stackTrace,
                                                           sizeof(stackTrace));
            StructuredLogger::instance().tryWriteEvent(
                StructuredCategory::Performance, StructuredLevel::Important,
                "performance.stall-stack-capture", "performance",
                "finished main-thread stack capture", frame,
                nlohmann::json{{"phase", "end"}, {"main_thread_id", threadId},
                               {"captured", captured}, {"stack", stackTrace}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
}

void startResourceWatchdog(uint32_t mainThreadId)
{
    if (gResourceWatchdogThread.joinable())
        return;
    gResourceWatchdogMainThreadId.store(mainThreadId, std::memory_order_relaxed);
    gLastCompletedFrameNanoseconds.store(steadyNowNanoseconds(),
                                         std::memory_order_relaxed);
    gResourceWatchdogStop.store(false, std::memory_order_relaxed);
    gResourceWatchdogThread = std::thread(resourceWatchdogMain);
}

uint64_t currentThreadCpu100ns(bool& available)
{
    FILETIME creation{}, exit{}, kernel{}, user{};
    available = GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) != 0;
    if (!available)
        return 0;
    ULARGE_INTEGER kernelTime{};
    ULARGE_INTEGER userTime{};
    kernelTime.LowPart = kernel.dwLowDateTime;
    kernelTime.HighPart = kernel.dwHighDateTime;
    userTime.LowPart = user.dwLowDateTime;
    userTime.HighPart = user.dwHighDateTime;
    return kernelTime.QuadPart + userTime.QuadPart;
}

void resetResourceWindow()
{
    gResourceWindow = ResourceWindow{};
}

WindowScopeStats* scopeStatsFor(const char* name)
{
    for (int i = 0; i < gResourceWindow.scopeCount; ++i)
        if (std::strncmp(gResourceWindow.scopes[i].name, name, sizeof(gResourceWindow.scopes[i].name)) == 0)
            return &gResourceWindow.scopes[i];
    if (gResourceWindow.scopeCount >=
        static_cast<int>(sizeof(gResourceWindow.scopes) / sizeof(gResourceWindow.scopes[0])))
        return nullptr;
    WindowScopeStats& out = gResourceWindow.scopes[gResourceWindow.scopeCount++];
    std::snprintf(out.name, sizeof(out.name), "%s", name ? name : "?");
    return &out;
}

void accumulateResourceFrame(const PerfFrame& frame)
{
    if (gResourceWindow.frames == 0) {
        gResourceWindow.firstFrame = frame.frameNumber;
        gResourceWindow.mainThreadCpuStart100ns = currentThreadCpu100ns(
            gResourceWindow.mainThreadCpuAvailable);
    }
    gResourceWindow.lastFrame = frame.frameNumber;
    gResourceWindow.scopeCaptureCount += static_cast<uint64_t>(gPerfScopeCount);
    gResourceWindow.maxScopeCaptures = std::max(
        gResourceWindow.maxScopeCaptures, gPerfScopeCount);
    if (gPerfScopeCount >= MAX_SCOPES_PER_FRAME)
        ++gResourceWindow.framesScopeCaptureSaturated;
    if (gResourceWindow.frames < static_cast<int>(gResourceWindow.frameTimesMs.size()))
        gResourceWindow.frameTimesMs[gResourceWindow.frames] = frame.totalMs;
    if (frame.totalMs > frame.budgetMs)
        ++gResourceWindow.framesOverBudget;
    gResourceWindow.frameBudgetMs += frame.budgetMs;
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
    gResourceWindow.broadphaseQueries += frame.broadphaseQueries;
    gResourceWindow.repeatedQueries += frame.repeatedQueries;
    gResourceWindow.chunkCellsVisited += frame.chunkCellsVisited;
    gResourceWindow.uniqueTriangles += frame.uniqueTriangles;
    gResourceWindow.triangleTests += frame.triangleTests;
    gResourceWindow.visibleMeshes += frame.visibleMeshes;
    gResourceWindow.visibleTriangles += frame.visibleTriangles;
    gResourceWindow.totalDrawCalls += frame.totalDrawCalls;
    gResourceWindow.maxBroadphaseQueries = std::max(gResourceWindow.maxBroadphaseQueries, frame.broadphaseQueries);
    gResourceWindow.maxUniqueTriangles = std::max(gResourceWindow.maxUniqueTriangles, frame.uniqueTriangles);
    gResourceWindow.maxVisibleMeshes = std::max(gResourceWindow.maxVisibleMeshes, frame.visibleMeshes);
    gResourceWindow.maxVisibleTriangles = std::max(gResourceWindow.maxVisibleTriangles, frame.visibleTriangles);
    if (PerfGpu::enabled()) {
        const double gpuFrameMs = PerfGpu::lastCompletedFrameMs();
        if (gpuFrameMs > 0.0) {
            gResourceWindow.gpuFrameMsTotal += gpuFrameMs;
            gResourceWindow.gpuFrameMsMax = std::max(
                gResourceWindow.gpuFrameMsMax, gpuFrameMs);
            ++gResourceWindow.gpuFrameSamples;
        }
    }
    double childMs[MAX_SCOPES_PER_FRAME] = {};
    for (int i = 0; i < gPerfScopeCount; ++i) {
        const PerfScopeCapture& scope = gPerfScopes[i];
        if (scope.parentIndex >= 0 && scope.parentIndex < gPerfScopeCount)
            childMs[scope.parentIndex] +=
                static_cast<double>(scope.cyclesInclusive) / 1000000.0;
    }
    for (int i = 0; i < gPerfScopeCount; ++i) {
        const PerfScopeCapture& in = gPerfScopes[i];
        const double inclusiveMs = static_cast<double>(in.cyclesInclusive) / 1000000.0;
        const double selfMs = std::max(0.0, inclusiveMs - childMs[i]);
        WindowScopeStats* out = scopeStatsFor(in.label);
        if (!out) {
            ++gResourceWindow.scopeEntriesDropped;
            continue;
        }
        out->calls += in.callCount;
        out->totalMs += inclusiveMs;
        out->maxMs = std::max(out->maxMs, inclusiveMs);
        out->selfMs += selfMs;
        out->allocCount += in.allocCount;
        out->allocBytes += in.allocBytes;
    }
}

// Compact time-based slice for periodic hitch evidence. The full 60-frame
// resource window is too coarse on a slow frame and can cover more than a
// second; this slice is bounded by wall-clock time so a sustained session can
// be compared at roughly 200 ms resolution without dumping every frame.
struct FastSliceScope {
    char name[64] = {};
    uint32_t calls = 0;
    double totalMs = 0.0;
    double maxMs = 0.0;
    uint64_t allocCount = 0;
    uint64_t allocBytes = 0;
};

struct FastSlice {
    uint64_t startNs = 0;
    uint32_t frames = 0;
    double sumMs = 0.0;
    double maxMs = 0.0;
    uint32_t overBudget = 0;
    std::array<double, 240> samples{};
    uint32_t sampleCount = 0;
    uint64_t allocCount = 0;
    uint64_t allocBytes = 0;
    int maxNpcs = 0;
    int maxEffects = 0;
    int maxBlood = 0;
    int maxCorpses = 0;
    int maxAudio = 0;
    int maxProjectiles = 0;
    FastSliceScope scopes[48];
    int scopeCount = 0;
};

FastSlice gFastSlice;

FastSliceScope* fastScopeFor(const char* name)
{
    for (int i = 0; i < gFastSlice.scopeCount; ++i)
        if (std::strncmp(gFastSlice.scopes[i].name, name,
                         sizeof(gFastSlice.scopes[i].name)) == 0)
            return &gFastSlice.scopes[i];
    if (gFastSlice.scopeCount >=
        static_cast<int>(sizeof(gFastSlice.scopes) / sizeof(gFastSlice.scopes[0])))
        return nullptr;
    FastSliceScope& out = gFastSlice.scopes[gFastSlice.scopeCount++];
    std::snprintf(out.name, sizeof(out.name), "%s", name ? name : "?");
    return &out;
}

void accumulateFastSlice(const PerfFrame& frame, const PerfState& state)
{
    const uint64_t nowNs = steadyNowNanoseconds();
    if (gFastSlice.startNs == 0)
        gFastSlice.startNs = nowNs;
    ++gFastSlice.frames;
    gFastSlice.sumMs += frame.totalMs;
    gFastSlice.maxMs = std::max(gFastSlice.maxMs, frame.totalMs);
    if (frame.totalMs > frame.budgetMs)
        ++gFastSlice.overBudget;
    if (gFastSlice.sampleCount < gFastSlice.samples.size())
        gFastSlice.samples[gFastSlice.sampleCount++] = frame.totalMs;
    gFastSlice.allocCount += frame.allocCount;
    gFastSlice.allocBytes += frame.allocBytes;
    gFastSlice.maxNpcs = std::max(gFastSlice.maxNpcs, frame.npcCount);
    gFastSlice.maxEffects = std::max(gFastSlice.maxEffects, frame.effectCount);
    gFastSlice.maxBlood = std::max(gFastSlice.maxBlood, state.bloodCount);
    gFastSlice.maxCorpses = std::max(gFastSlice.maxCorpses, frame.corpseCount);
    gFastSlice.maxAudio = std::max(gFastSlice.maxAudio, frame.audioCount);
    gFastSlice.maxProjectiles = std::max(gFastSlice.maxProjectiles, frame.projectileCount);

    for (int i = 0; i < frame.entryCount; ++i) {
        const PerfBreakdownEntry& entry = frame.entries[i];
        FastSliceScope* scope = fastScopeFor(entry.label);
        if (!scope) continue;
        scope->calls += entry.callCount;
        scope->totalMs += entry.inclMs;
        scope->maxMs = std::max(scope->maxMs, entry.inclMs);
        scope->allocCount += entry.allocCount;
        scope->allocBytes += entry.allocBytes;
    }
}

void emitFastSliceIfDue(const PerfFrame& lastFrame, const PerfState& state)
{
    if (gFastSlice.startNs == 0)
        return;
    const uint64_t nowNs = steadyNowNanoseconds();
    const double elapsedMs = static_cast<double>(nowNs - gFastSlice.startNs) / 1e6;
    if (elapsedMs < 200.0)
        return;

    PROCESS_MEMORY_COUNTERS_EX memory{};
    const bool memoryOk = GetProcessMemoryInfo(
        GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
        sizeof(memory)) != 0;
    const StructuredLogger::IoStats io = StructuredLogger::instance().ioStats(false);

    std::array<double, 240> sorted = gFastSlice.samples;
    const int count = std::min(gFastSlice.sampleCount,
        static_cast<uint32_t>(sorted.size()));
    std::sort(sorted.begin(), sorted.begin() + count);
    auto pct = [&](double p) {
        if (count <= 0) return 0.0;
        return sorted[std::min(count - 1,
            static_cast<int>(std::ceil(count * p)) - 1)];
    };

    std::vector<const FastSliceScope*> slowScopes;
    std::vector<const FastSliceScope*> allocatingScopes;
    slowScopes.reserve(gFastSlice.scopeCount);
    allocatingScopes.reserve(gFastSlice.scopeCount);
    for (int i = 0; i < gFastSlice.scopeCount; ++i) {
        slowScopes.push_back(&gFastSlice.scopes[i]);
        if (gFastSlice.scopes[i].allocBytes > 0)
            allocatingScopes.push_back(&gFastSlice.scopes[i]);
    }
    std::sort(slowScopes.begin(), slowScopes.end(),
        [](const FastSliceScope* a, const FastSliceScope* b) {
            return a->maxMs > b->maxMs;
        });
    std::sort(allocatingScopes.begin(), allocatingScopes.end(),
        [](const FastSliceScope* a, const FastSliceScope* b) {
            return a->allocBytes > b->allocBytes;
        });
    auto scopeJson = [](const FastSliceScope& scope) {
        return nlohmann::json{{"name", scope.name}, {"calls", scope.calls},
                              {"total_ms", scope.totalMs}, {"max_ms", scope.maxMs},
                              {"alloc_count", scope.allocCount},
                              {"alloc_bytes", scope.allocBytes}};
    };
    nlohmann::json scopes = nlohmann::json::array();
    for (size_t i = 0; i < slowScopes.size() && i < 10; ++i)
        scopes.push_back(scopeJson(*slowScopes[i]));
    nlohmann::json allocScopes = nlohmann::json::array();
    for (size_t i = 0; i < allocatingScopes.size() && i < 10; ++i)
        allocScopes.push_back(scopeJson(*allocatingScopes[i]));

    static uint64_t sSessionStartNs = 0;
    static uint64_t sSessionFirstWorkingSet = 0;
    static uint64_t sSessionFirstPrivate = 0;
    static uint64_t sSessionFirstAlloc = 0;
    static uint64_t sLastSessionGrowthNs = 0;
    if (sSessionStartNs == 0) {
        sSessionStartNs = nowNs;
        sLastSessionGrowthNs = nowNs;
    }
    if (memoryOk && sSessionFirstWorkingSet == 0) {
        sSessionFirstWorkingSet = memory.WorkingSetSize;
        sSessionFirstPrivate = memory.PrivateUsage;
        sSessionFirstAlloc = state.totalAllocations;
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Performance, StructuredLevel::Important,
        "performance.frame-slice", "performance", "200ms hitch evidence slice",
        static_cast<uint32_t>(lastFrame.frameNumber),
        nlohmann::json{
            {"window_ms", elapsedMs},
            {"frames", gFastSlice.frames},
            {"frame_avg_ms", gFastSlice.sumMs / std::max(1u, gFastSlice.frames)},
            {"frame_max_ms", gFastSlice.maxMs},
            {"frame_p50_ms", pct(0.50)},
            {"frame_p95_ms", pct(0.95)},
            {"frame_p99_ms", pct(0.99)},
            {"frames_over_budget", gFastSlice.overBudget},
            {"alloc_count", gFastSlice.allocCount},
            {"alloc_bytes", gFastSlice.allocBytes},
            {"memory_available", memoryOk},
            {"working_set_bytes", memoryOk ? memory.WorkingSetSize : 0},
            {"private_bytes", memoryOk ? memory.PrivateUsage : 0},
            {"pagefile_usage_bytes", memoryOk ? memory.PagefileUsage : 0},
            {"logger_events_total", io.eventsWritten},
            {"logger_bytes_total", io.bytesWritten},
            {"logger_flushes_total", io.flushCount},
            {"npcs", gFastSlice.maxNpcs},
            {"players", state.playerCount},
            {"effects", gFastSlice.maxEffects},
            {"blood_particles", gFastSlice.maxBlood},
            {"corpses", gFastSlice.maxCorpses},
            {"audio", gFastSlice.maxAudio},
            {"projectiles", gFastSlice.maxProjectiles},
            {"animation_set_cache", state.animationSetCacheCount},
            {"animation_clip_cache", state.animationClipCacheCount},
            {"avatar_pending_loads", state.avatarPendingLoads},
            {"avatar_cache_entries", state.avatarCacheEntries},
            {"model_upload_cache_entries", state.modelUploadCacheEntries},
            {"replay_memory_mb", state.replayMemoryMb},
            {"scopes", std::move(scopes)},
            {"alloc_scopes", std::move(allocScopes)}},
        __FILE__, __LINE__, __FUNCTION__);

    if (nowNs - sLastSessionGrowthNs >= 30000000000ull) {
        sLastSessionGrowthNs = nowNs;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Performance, StructuredLevel::Important,
            "performance.session-growth", "performance",
            "30s retained-resource growth snapshot",
            static_cast<uint32_t>(lastFrame.frameNumber),
            nlohmann::json{
                {"session_elapsed_s",
                 static_cast<double>(nowNs - sSessionStartNs) / 1e9},
                {"working_set_bytes", memoryOk ? memory.WorkingSetSize : 0},
                {"working_set_delta_bytes", memoryOk && sSessionFirstWorkingSet != 0
                    ? (int64_t)memory.WorkingSetSize -
                      (int64_t)sSessionFirstWorkingSet : 0},
                {"private_bytes", memoryOk ? memory.PrivateUsage : 0},
                {"private_delta_bytes", memoryOk && sSessionFirstPrivate != 0
                    ? (int64_t)memory.PrivateUsage -
                      (int64_t)sSessionFirstPrivate : 0},
                {"total_allocations", state.totalAllocations},
                {"allocation_delta", state.totalAllocations >= sSessionFirstAlloc
                    ? state.totalAllocations - sSessionFirstAlloc : 0},
                {"npcs", state.npcCount},
                {"effects", state.effectCount},
                {"blood_particles", state.bloodCount},
                {"corpses", state.corpseCount},
                {"animation_set_cache", state.animationSetCacheCount},
                {"animation_clip_cache", state.animationClipCacheCount},
                {"avatar_pending_loads", state.avatarPendingLoads},
                {"avatar_cache_entries", state.avatarCacheEntries},
                {"model_upload_cache_entries", state.modelUploadCacheEntries},
                {"replay_memory_mb", state.replayMemoryMb},
                {"logger_events_total", io.eventsWritten},
                {"logger_bytes_total", io.bytesWritten}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    gFastSlice = FastSlice{};
    gFastSlice.startNs = nowNs;
}

void emitResourceWindowSummary()
{
    if (gResourceWindow.frames == 0) return;

    PROCESS_MEMORY_COUNTERS_EX memory{};
    const bool memoryOk = GetProcessMemoryInfo(
        GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
        sizeof(memory)) != 0;
    const StructuredLogger::IoStats io = StructuredLogger::instance().ioStats(true);
    std::array<double, 60> sortedFrameTimes = gResourceWindow.frameTimesMs;
    const int measuredFrames = std::min(
        gResourceWindow.frames, static_cast<int>(sortedFrameTimes.size()));
    std::sort(sortedFrameTimes.begin(), sortedFrameTimes.begin() + measuredFrames);
    const int p95Index = measuredFrames > 0
        ? std::max(0, static_cast<int>(std::ceil(measuredFrames * 0.95)) - 1) : 0;
    const int p99Index = measuredFrames > 0
        ? std::max(0, static_cast<int>(std::ceil(measuredFrames * 0.99)) - 1) : 0;
    bool mainThreadCpuEndAvailable = false;
    const uint64_t mainThreadCpuEnd100ns = currentThreadCpu100ns(mainThreadCpuEndAvailable);
    const bool mainThreadCpuMeasured = gResourceWindow.mainThreadCpuAvailable &&
        mainThreadCpuEndAvailable &&
        mainThreadCpuEnd100ns >= gResourceWindow.mainThreadCpuStart100ns;
    const double mainThreadCpuMs = mainThreadCpuMeasured
        ? static_cast<double>(mainThreadCpuEnd100ns -
                              gResourceWindow.mainThreadCpuStart100ns) / 10000.0
        : 0.0;

    nlohmann::json scopes = nlohmann::json::array();
    for (int i = 0; i < gResourceWindow.scopeCount; ++i) {
        const WindowScopeStats& s = gResourceWindow.scopes[i];
        scopes.push_back({
            {"name", s.name}, {"calls", s.calls},
            {"total_ms", s.totalMs}, {"max_ms", s.maxMs},
            {"self_ms", s.selfMs},
            {"alloc_count", s.allocCount}, {"alloc_bytes", s.allocBytes}});
    }

    nlohmann::json fields = {
        {"window_frames", gResourceWindow.frames},
        {"frame_start", gResourceWindow.firstFrame},
        {"frame_end", gResourceWindow.lastFrame},
        {"frame_total_ms", gResourceWindow.totalFrameMs},
        {"frame_average_ms", gResourceWindow.totalFrameMs / std::max(1, gResourceWindow.frames)},
        {"frame_max_ms", gResourceWindow.maxFrameMs},
        {"frame_max_number", gResourceWindow.maxFrame},
        {"target_fps", gResourceWindow.frames > 0 && gResourceWindow.frameBudgetMs > 0.0
            ? 1000.0 * gResourceWindow.frames / gResourceWindow.frameBudgetMs : 0.0},
        {"frames_over_target_budget", gResourceWindow.framesOverBudget},
        {"frames_over_target_budget_percent", 100.0 * gResourceWindow.framesOverBudget /
            std::max(1, gResourceWindow.frames)},
        {"frame_p95_ms", measuredFrames > 0 ? sortedFrameTimes[p95Index] : 0.0},
        {"frame_p99_ms", measuredFrames > 0 ? sortedFrameTimes[p99Index] : 0.0},
        {"gpu_timer_queries_enabled", PerfGpu::enabled()},
        {"gpu_frame_samples", gResourceWindow.gpuFrameSamples},
        {"gpu_captured_regions_ms_average", gResourceWindow.gpuFrameSamples > 0
            ? gResourceWindow.gpuFrameMsTotal / gResourceWindow.gpuFrameSamples : 0.0},
        {"gpu_captured_regions_ms_max", gResourceWindow.gpuFrameMsMax},
        {"scope_entries_dropped", gResourceWindow.scopeEntriesDropped},
        {"scope_capture_entries", gResourceWindow.scopeCaptureCount},
        {"scope_capture_max_per_frame", gResourceWindow.maxScopeCaptures},
        {"frames_scope_capture_saturated", gResourceWindow.framesScopeCaptureSaturated},
        {"main_thread_cpu_measured", mainThreadCpuMeasured},
        {"main_thread_cpu_ms_total", mainThreadCpuMs},
        {"main_thread_cpu_ms_average", mainThreadCpuMs /
            std::max(1, gResourceWindow.frames)},
        {"alloc_count", gResourceWindow.allocCount},
        {"alloc_bytes", gResourceWindow.allocBytes},
        {"max_npc_count", gResourceWindow.maxNpcCount},
        {"max_effect_count", gResourceWindow.maxEffectCount},
        {"max_projectile_count", gResourceWindow.maxProjectileCount},
        {"broadphase_queries", gResourceWindow.broadphaseQueries},
        {"repeated_queries", gResourceWindow.repeatedQueries},
        {"chunk_cells_visited", gResourceWindow.chunkCellsVisited},
        {"unique_triangles", gResourceWindow.uniqueTriangles},
        {"triangle_tests", gResourceWindow.triangleTests},
        {"visible_meshes", gResourceWindow.visibleMeshes},
        {"visible_triangles", gResourceWindow.visibleTriangles},
        {"total_draw_calls", gResourceWindow.totalDrawCalls},
        {"max_broadphase_queries_frame", gResourceWindow.maxBroadphaseQueries},
        {"max_unique_triangles_frame", gResourceWindow.maxUniqueTriangles},
        {"max_visible_meshes_frame", gResourceWindow.maxVisibleMeshes},
        {"max_visible_triangles_frame", gResourceWindow.maxVisibleTriangles},
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

void perfStartResourceWatchdog(uint32_t mainThreadId)
{
    startResourceWatchdog(mainThreadId);
}

void perfStopResourceWatchdog()
{
    gResourceWatchdogStop.store(true, std::memory_order_relaxed);
    if (gResourceWatchdogThread.joinable())
        gResourceWatchdogThread.join();
}

// ── Capture current frame ───────────────────────────────────

void perfCaptureFrame(double totalMs, double budgetMs, int frameNumber)
{
    gResourceWatchdogMainThreadId.store(GetCurrentThreadId(),
                                        std::memory_order_relaxed);
    gLastCompletedFrameNumber.store(static_cast<uint32_t>(frameNumber),
                                    std::memory_order_relaxed);
    gLastCompletedFrameNanoseconds.store(steadyNowNanoseconds(),
                                         std::memory_order_relaxed);
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
        e.allocCount = cap.allocCount;
        e.allocBytes = cap.allocBytes;
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
    frame.broadphaseQueries = ps.current.broadphaseQueries;
    frame.repeatedQueries = ps.current.repeatedQueries;
    frame.chunkCellsVisited = ps.current.chunkCellsVisited;
    frame.uniqueTriangles = ps.current.uniqueTriangles;
    frame.triangleTests = ps.current.triangleTestsCount;
    frame.visibleMeshes = ps.current.visibleMeshes;
    frame.visibleTriangles = ps.current.visibleTriangles;
    frame.totalDrawCalls = ps.current.totalDrawCalls;

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
    accumulateFastSlice(frame, ps);
    emitFastSliceIfDue(frame, ps);
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
