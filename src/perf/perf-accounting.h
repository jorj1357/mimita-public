#pragma once

#include <cstdint>

#include "perf/perf.h"

struct PerfAccountedScope {
    char label[64] = {};
    double selfMs = 0.0;
    double inclMs = 0.0;
    int depth = 0;
};

struct PerfFrameAccounting {
    static constexpr int kMaxScopes = 16;

    bool valid = false;
    std::uint32_t frameNumber = 0;
    double cycleMs = 0.0;
    double hostGapMs = 0.0;
    double frameMs = 0.0;
    double rootAccountedMs = 0.0;
    double unaccountedMs = 0.0;
    double swapMs = 0.0;
    double sleepMs = 0.0;
    double overlayMs = 0.0;
    double serverRxHz = 0.0;
    double serverActualHz = 0.0;
    std::uint32_t serverTick = 0;
    bool serverValid = false;
    bool serverStale = false;
    bool serverActualValid = false;
    char publishedGeneration[32] = {};
    int scopeCount = 0;
    PerfAccountedScope scopes[kMaxScopes];
    PerfTimes times;
};

extern PerfFrameAccounting gPerfFrameAccounting;
