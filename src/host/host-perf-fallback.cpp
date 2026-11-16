// Resident performance facade for host-owned services.
//
// Game code owns the real profiler. The weak definitions below keep cold
// host sources (audio/music workers) linkable without pulling game policy into
// the EXE; the game-owned strong symbols win in the migration EXE.

#include "perf/perf.h"
#include "perf/perf-spike.h"

__attribute__((weak)) PerfState& Perf::state() {
    static PerfState state;
    return state;
}

__attribute__((weak)) PerfScopeGuard::PerfScopeGuard(
    const char*, int, const char*, const char*) {}

__attribute__((weak)) PerfScopeGuard::~PerfScopeGuard() {}

__attribute__((weak)) void perfStopResourceWatchdog() {}
