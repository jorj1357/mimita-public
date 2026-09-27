# Generic runtime bootstrap (Phase 0a): package/system/command/schema registration

- EST timestamp: 2026-09-13 21:03:29 EDT (UTC 2026-09-14T01:03:29Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); runtime human/multiplayer proof pending

## Direction recorded

- Added the governing principle to `AGENTS.md`: aim to make as much of the repo
  hot reloadable as possible; do not add a subsystem-specific EXE slot when an
  existing generic primitive can express it; `modecreate` is the first proof.
- Added the full plan as a gold reference:
  `docs/gold/2026-09-13-live-runtime-generic-bootstrap.md`.

## What was implemented (Phase 0a — first of 2-3 staged cold builds)

### Generic package ABI (v5) — `src/hot-reload/game-api.h`
- `MIMITA_GAME_API_VERSION = 5`; `MIMITA_PACKAGE_ABI_VERSION = 1`.
- Compile-time `constexpr gameHash(name)` (FNV-1a) so kernel and packages hash
  names identically; reserved domain ids `GAME_DOMAIN_GAMEPLAY` / `GAME_DOMAIN_RENDER`.
- `GameSystemDescriptorV1`, `GameEventTypeDescriptorV1`,
  `GameComponentSchemaDescriptorV1` (+ `GameCopyPolicy`), `GameCapabilityDescriptorV1`,
  `GameCommandDescriptorV1`, `GameResourceDescriptorV1`, `GameMigrationDescriptorV1`,
  `GamePackageDescriptorV1` (pointer+count arrays), and `GameAPI.packageDescriptor`.
- Kernel knows the descriptors, not the names inside them. Adding a system/event/
  schema/capability/command is an array entry, not a new ABI field.

### Kernel generic runtime — `src/hot-reload/generic-runtime.h/.cpp` (new)
- Owns the active package; registers systems/commands/events/schemas/capability
  providers/requirements atomically (failure keeps the previous registration).
- Systems sorted by (domain, priority, id); `runDomain(domainId, tick, dt, host)`
  iterates cached entries — no per-call hashing.
- `hasCommand`/`runCommand`, `dispatchEvent`, `describe()`.

### Loader wiring — `src/hot-reload/hot-reload-system.cpp`
- Initial startup now activates the existing DLL's package (systems/commands
  available from startup, not only after the first hot rebuild).
- `tryActivateCandidate` validates/activates the candidate's package before
  committing; a registration failure rejects the candidate and keeps the active
  generation. `rollback()` re-registers the restored generation; `unloadGameDLL()`
  deactivates.

### Runtime command + scheduler seams
- `src/devtools/terminal.cpp`: unknown commands are routed to the generic runtime,
  so hot packages can add commands (`invlist`, `my_command`, ...) without an EXE
  edit.
- `src/sim/simulate-tick.cpp`: runs `gameplay.60` generic systems each fixed tick.
- `src/engine/engine-tick-ui.cpp`: runs `render.frame` generic systems each frame.

### Hot demo package — `src/hot-reload/modules/generic-demo-package.cpp` (new, hot)
- Registers 2 systems (`demo.gameplay-tick`, `demo.render-tick`), 1 event type
  (`demo.ping`), 1 component schema (`DemoTag`, copy policy `AUTHORING_COPYABLE`),
  and 1 command (`hotdemo`). `effect-part.cpp` exposes it via
  `MimitaGetPackageDescriptor()`.
- `hot-modules.json` marks `generic-runtime.*` cold (EXE-owned).

### Bug fix
- `CreationMode::setEnabled` dereferenced `ACTIVE_MAP_PATH` when the subsystem
  pointer was null (headless selftest), causing an access violation. Guarded.

## Evidence

- `-fsyntax-only` clean: `generic-runtime.cpp`, `hot-reload-system.cpp`,
  `terminal.cpp`, `simulate-tick.cpp`, `engine-tick-ui.cpp`; DLL-side clean:
  `generic-demo-package.cpp`, `effect-part.cpp`.
- `python build_agent.py` -> `BUILD SUCCESS`, relinked (45 + 1 compiled).
- Startup log proves registration: `[HOT RELOAD] package=mimita.core
  systems=... commands=1 schemas=1` (observed via `--ragdoll-slice-selftest`).
- Hot DLL link success with 6 sources.
- Self-tests PASS: ragdoll-slice, live-code, hot-authoritative, entity-slice,
  project, phase456, telemetry, creation.

## Pending

- Phase 0b: dynamic component byte-store, subscription-driven scheduler, event
  dispatch, capability resolution/enforcement, runtime command lifecycle.
- Phase 0c: per-path watcher, dependency-graph incremental build, package
  manifests, generation manifest.
- Phase 1: `modecreate` as a package (hot free-fly movement system, hovered vs
  selected, overlap cycling, copy/cut/paste, ChangeSet).
- Runtime human + multiplayer proof.

## Files

New: `docs/gold/2026-09-13-live-runtime-generic-bootstrap.md`,
`src/hot-reload/generic-runtime.h/.cpp`,
`src/hot-reload/modules/generic-demo-package.cpp`.

Changed: `AGENTS.md`, `src/hot-reload/game-api.h`, `src/hot-reload/game-modules.h`,
`src/hot-reload/hot-reload-system.cpp`, `src/hot-reload/hot-modules.json`,
`src/effects/effect-part.cpp`, `src/devtools/terminal.cpp`,
`src/sim/simulate-tick.cpp`, `src/engine/engine-tick-ui.cpp`,
`src/editor/creation-mode.cpp`.
