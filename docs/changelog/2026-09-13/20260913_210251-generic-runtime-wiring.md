# Generic runtime wiring: project registries, domain/event dispatch, movement.main, build visibility

- EST timestamp: 2026-09-13 21:02:51 EDT (UTC 2026-09-14T01:02:51Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `SOURCE_COMPLETE / COLD_BOOTSTRAP_PENDING` (mimita.exe running)

## Goal

Finish wiring the generic hot-runtime mechanism (the v5 `GenericRuntime` package
descriptor that concurrent work introduced) so that new systems/events/
capabilities/schemas can be registered at runtime instead of adding a permanent
subsystem-specific slot to `mimita.exe`. Movement is the first system migrated.

## What was wired

### Generic runtime -> project-layer registries (cold) — `src/hot-reload/generic-runtime.cpp`
- `GenericRuntime::activate` now registers each package component schema into
  `Project::ComponentSchemaRegistry` and each package migration into
  `Project::StateSchemaRegistry`.
- Generic capability providers/requirements are registered into
  `Project::CapabilityRegistry` by hashed capability id.

### Generic capability ids (cold) — `src/project/capabilities.{h,cpp}`
- Added `provideId` / `requestId` / `providesId` / `allowsId` /
  `providedIdCount` so arbitrary runtime ids (`entity.spawn`,
  `physics.applyImpulse`, …) work without extending the fixed `Capability` enum.

### Kernel event bridge (cold) — `src/live-code/live-behavior.cpp`
- `dispatchEvent` and `dispatchPayload` now route every event to
  `MimitaRuntime::GenericRuntime::dispatchEvent` first, then the legacy
  gameplay module. Runtime-registered event types are the forward path.

### Generic domain dispatch (cold)
- `src/sim/simulate-tick.cpp`: runs `GAME_DOMAIN_GAMEPLAY` systems every fixed
  60 Hz tick via `GenericRuntime::runDomain`.
- `src/engine/engine-tick-render.cpp`: runs `GAME_DOMAIN_RENDER` systems each
  render frame.
- The kernel only iterates the registered subscriber list, so a new domain name
  or system needs no new EXE call site.

### movement.main (hot) — `src/hot-reload/modules/generic-demo-package.cpp`
- Added `movement.main` subscribed to `gameplay.60` (priority 50). It is the
  first gameplay system on the generic path. The kernel keeps its built-in
  movement step for now; the system takes ownership incrementally as
  capabilities land. Editing the file applies live.

### Build visibility (cold) — `build.py`
- New `write_build_record()` writes `build/changelog.txt`,
  `build/build-result.json`, and appends `build/build-history.jsonl` on every
  cold build (success and link failure). `buildv2.py` runs `build.py`, so it now
  records both the changelog and the structured record.

### Manifest — `src/hot-reload/hot-modules.json`
- Added `src/hot-reload/generic-runtime.cpp` and `src/project/capabilities.cpp`
  to the `cold` list.

## Evidence

- Source: `g++ -fsyntax-only` passes for generic-runtime, live-behavior,
  simulate-tick, engine-tick-render, hot-reload-system, capabilities.
- Hot DLL: `build_game_dll.py --generation 9005` -> `DLL build success`
  (6 sources, includes `generic-demo-package.cpp` with `movement.main`).
- Python: `py_compile` passes for build.py, buildv2.py, build_game_dll.py.
- Cold link: not run here (mimita.exe running). Required to install the kernel
  wiring; run `python buildv2.py` when ready.

## Deferred (per agreed phasing)

- Dynamic canonical component storage (typed-first for now).
- `Project::PackageRegistry` runtime `package.json` discovery.
- READY/SWITCH multiplayer handshake (compatibility designed, not implemented).
- Moving movement's actual step behind capabilities; ragdoll stays on its
  current branch until the hot system path is proven.
