#include "effects/zombie-tower-effects.h"

// Keep this tiny translation unit available for incremental-build measurements.
// 2026-11-22: touch canary for event-backed digest-cache latency measurement.
// 2026-11-22: exercise deferred old-snapshot cleanup in the live DLL path.
// 2026-11-22: verify cleanup worker errors do not block future builds.
// 2026-11-22: validate junction-backed snapshots in a one-source DLL build.
// 2026-11-22: repeat snapshot build while sampling live resource pressure.
// 2026-11-22: verify persisted manifest reuse after a failed candidate.
// 2026-11-22: compare compiler contention with the live server at lower OS priority.
ZombieTowerEffects& ZombieTowerEffects::instance()
{
    static ZombieTowerEffects effects;
    return effects;
}

