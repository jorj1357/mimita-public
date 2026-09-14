# Generic runtime bootstrap (Phase 0b): dynamic components, capability resolution, event emit, domains

- EST timestamp: 2026-09-13 21:11:44 EDT (UTC 2026-09-14T01:11:44Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); runtime human/multiplayer proof pending

## What was implemented (second staged cold build)

### Dynamic component storage — `src/ecs/dynamic-components.h/.cpp` (new)
- `DynamicComponentStore`: package-declared component schemas (id/schemaHash/
  size/align/copyPolicy/networkPolicy) stored per entity as byte blobs, without a
  compile-time C++ type or a cold enum entry.
- `registerSchema`, `read`, `write`, `has`, `remove`, `eraseEntity`, counts.
- Package component schemas are registered into the store during
  `GenericRuntime::activate`.

### Capability resolution + registry — `src/hot-reload/generic-runtime.*`
- At activation, every `capabilityRequirement` must be satisfied by a package
  provider or a kernel primitive capability; otherwise the candidate fails and
  the active generation is kept.
- Kernel-provided primitive capability ids: `component.read/write`,
  `entity.find`, `physics.query`, `log.write`, `time.now`,
  `dynamic.component.read/write`.
- `capability(id)` / `hasCapability(id)` / `kernelProvidesCapability(id)`.

### Generic event emit — `generic-runtime.*`
- `emit(typeId, source, target, payload, size, tick, host)` builds a
  `GameEventV1` and dispatches to the registered event-type handler.

### Dynamic scheduler domains — `generic-runtime.*` + kernel ticks
- `runRegisteredDomains(tick, dt, host)` runs every non-reserved domain declared
  by a package's systems once per fixed tick; reserved `gameplay.60` /
  `render.frame` stay kernel-timed.
- `simulate-tick.cpp` runs gameplay.60 + all package domains with a real host
  context; `engine-tick-ui.cpp` runs render.frame with a host context.

### Host capability context — `src/live-code/live-behavior.cpp/.h`
- Added `dynamicReadComponent` / `dynamicWriteComponent` to `GameplayContextV1`
  (v5 append-only) backed by `DynamicComponentStore`.
- Added `LiveBehavior::hostContext(tick)` returning the kernel capability context
  generic systems receive as `host`.

### Hot demo package — `modules/generic-demo-package.cpp`
- Now registers 3 systems (gameplay.60, render.frame, custom `demo.custom`
  domain), 1 event, 1 component schema, 1 command, and requires `log.write`
  (kernel-provided) — proving requirement resolution and custom-domain timing.

## Evidence

- Startup log: `[HOT RELOAD] package=mimita.core ... systems=3 commands=1
  schemas=1 providers=0 requirements=1` (observed via `--ragdoll-slice-selftest`),
  proving requirement resolution succeeded.
- `python build_agent.py` -> `BUILD SUCCESS`, relinked.
- `-fsyntax-only` clean (EXE + DLL sides) for all changed TUs.
- Self-tests PASS: creation, ragdoll-slice, live-code, hot-authoritative,
  entity-slice, project, phase456, telemetry.

## Pending

- Phase 0c: per-path watcher events, dependency-graph incremental build, package
  manifests, whole-project hot graph, generation manifest.
- Phase 1: `modecreate` as a package (hot free-fly movement system, hovered vs
  selected, overlap cycling, copy/cut/paste, ChangeSet).
- Runtime human + multiplayer proof.

## Files

New: `src/ecs/dynamic-components.h/.cpp`.

Changed: `src/hot-reload/game-api.h` (`GameplayContextV1` dynamic component
capabilities), `src/hot-reload/generic-runtime.h/.cpp`,
`src/hot-reload/modules/generic-demo-package.cpp`,
`src/live-code/live-behavior.h/.cpp`, `src/sim/simulate-tick.cpp`,
`src/engine/engine-tick-ui.cpp`.
