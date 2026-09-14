# Generic runtime: cold bootstrap installed and proven live (same process)

- EST timestamp: 2026-09-13 21:45:00 EDT (UTC 2026-09-14T01:45:00Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `LIVE_PROVEN` on a real `mimita.exe` (dedicated server process),
  no relink after bootstrap.

## Summary

Installed the v5 `GenericRuntime` (generic packages/systems/events/schemas/
capabilities/commands) into `mimita.exe` with one cold link, then proved on the
running process that a system concept unknown at startup can be created,
registered, activated, edited, renamed, split, deleted, replaced, broken,
recovered, and removed without restarting the process.

## Cold changes (bootstrap)

- `build.py`: `write_build_record()` writes `build/changelog.txt`,
  `build/build-result.json`, and appends `build/build-history.jsonl` on every
  cold build (success and link failure). `buildv2.py` inherits this.
- `src/hot-reload/hot-package.h` (new): header-only DLL-side package builder with
  self-registration (`MimitaHotPackage::SystemRegistrar`, `EventRegistrar`,
  `CapabilityRegistrar`, `SchemaRegistrar`, `CommandRegistrar`). Adding a new
  `.cpp` under `src/hot-reload/modules/` now registers new concepts with no
  aggregator edit and no EXE slot.
- `src/hot-reload/game-api.h`: `GameEventV1.typeId` widened to 64-bit and
  `schemaHash` added, so runtime event ids (`gameHash("pkg.event")`) are never
  truncated. Generic event identity is the general mechanism.
- `src/hot-reload/generic-runtime.{h,cpp}`: event schema hash retained in
  `EventEntry`; activation observability (`[GENERIC_RUNTIME]`,
  `[SYSTEM_REGISTERED]`, `[EVENT_REGISTERED]`, `[CAPABILITY_RESOLVED]`);
  capability requirement resolution/observability.
- `src/project/capabilities.{h,cpp}`: id-based `provideId`/`requestId`/
  `allowsId` so arbitrary capability ids work without extending the enum.
- `src/network/server.cpp`: dedicated server runs generic
  `GAME_DOMAIN_GAMEPLAY` systems + registered domains per fixed step and drains
  emitted events (`LiveBehavior::drainEvents`).
- `src/sim/simulate-tick.cpp`, `src/engine/engine-tick-ui.cpp`: drain generic
  events once per tick/frame after running domains (the missing delivery step).
- `src/engine/engine-tick-render.cpp`: removed a duplicate render-domain
  dispatch (UI tick owns it).
- `src/hot-reload/hot-modules.json`: `hot-package.h` added to headers; cold list
  updated.

## Hot probes (live, not committed as product code)

- `src/hot-reload/modules/movement-system.cpp` (new): `movement.main` on
  `gameplay.60`.
- `src/hot-reload/modules/banana-system-renamed.cpp` (created live, renamed):
  `banana.peel-slip` on `gameplay.60`, emits `banana.eaten`.
- `src/hot-reload/modules/banana-events.cpp` (created/split/replaced):
  registers `banana.eaten` and the capability `banana.unknown.capability`.

## Evidence (all one process: pid 26248, session 78779531)

- Cold bootstrap: `build.py build-only` SUCCESS; record written.
- Startup registration: `[GENERIC_RUNTIME] systems=5`, including
  `movement.main`, `banana.peel-slip`.
- Live edit: generation 4, `[MOVEMENT.MAIN] ... REV=2`.
- Brand-new system: `[SYSTEM_REGISTERED] system=banana.peel-slip`,
  `[HOT_SYSTEM] banana.peel-slip alive`.
- New event: `[EVENT_REGISTERED] event=banana.eaten` and
  `[HOT_EVENT] banana.eaten delivered`.
- Rename: generation 27, system still registered/executing.
- Split: `banana-events.cpp` separate file.
- Delete: generation 26 active without the file; system still ran.
- Replace: generation 31, event delivered again.
- Syntax error: `compile_finished failed`, `active_generation=5` unchanged,
  banana kept running; fix -> generation 14 active.
- Capability missing: `validation_result failed: missing capability provider`;
  provider added -> generation 25 active, `[CAPABILITY_RESOLVED]`.
- Dependencies: `drainEvents` added (missing generic event-delivery step).

## Remaining meta-mechanism notes

- The event-delivery gap (queued events only drained inside payload dispatches)
  was the one real blocker; fixed generally by draining per tick.
- `game-api.h` is still hand-maintained ABI; dynamic component storage and
  schema-driven events are the next general steps (already partially present in
  `ecs/dynamic-components.h`).
