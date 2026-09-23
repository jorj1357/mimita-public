# Stage A finish: reclassify dispatch-only bridges; hot codec proven headless

Date (UTC): 2026-09-23T18:20:00Z
Status: implemented; cold build and selftests verified

## Scope

Close out Stage A. A1/A2 moved the snapshot-chunk codec and the server
movement-validation policy into hot headers + hot providers. What remained was
manifest bookkeeping (the two `.cpp` files were still labelled `cold`) and
proving the hot snapshot codec in a headless test.

## Changes

1. **Manifest `bridges` category** (`src/hot-reload/hot-modules.json`): moved
   `src/network/snapshot-chunks.cpp` and `src/network/movement-validation.cpp`
   out of `cold` and into a new `bridges` array. A bridge is an EXE-owned
   dispatch-only file: its behavior already lives in a hot header/provider, it
   contains no policy, but a change still requires a cold relink.

2. **Reload system** (`hot-reload-system.h`/`.cpp`): added `bridgeSources_` and
   load the `bridges` array. `pollColdBoundary` now watches both lists; a cold
   change records `HOT_RELOAD_BOUNDARY_VIOLATION` and a bridge change records
   `HOT_RELOAD_BRIDGE_CHANGE` (still a relink requirement, still notified), so
   editing a bridge is never silently ignored.

3. **Headless hot-codec proof** (`game/game-cli.cpp`): `--snapshot-chunk-selftest`
   now calls `HotReloadSystem::startup()` before the test and `unloadGameDLL()`
   after, so the selftest exercises the hot `net.snapshot-codecs` provider rather
   than the compiled fallback. A missing/incompatible DLL simply leaves the
   fallback active, so the test cannot regress.

## Why the fallback stays

The shared implementation is header-only and compiled into both the cold bridge
and the hot provider, so it is a single owner, not a second copy. The bridge's
fallback branch calls the same code. Registering the shared implementation as a
kernel capability instead was rejected because `GenericRuntime::capability`
resolves `kernelCapabilities_` before `capabilityProviders_`, so a kernel
provider would shadow the hot provider. The fallback is therefore the correct
mechanism; the file is a bridge, not a behavior owner.

## Evidence

- Source changes: `src/hot-reload/hot-modules.json`,
  `src/hot-reload/hot-reload-system.h`, `src/hot-reload/hot-reload-system.cpp`,
  `src/game/game-cli.cpp`.
- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T181225.exe` (146 compiled).
- Automated tests (test evidence), all on `mimita-20260923T181225.exe`:
  - `--snapshot-chunk-selftest` -> PASS, now with
    `[CAPABILITY_RESOLVED] provider=net.snapshot-codecs` (hot path exercised).
  - `--live-code-selftest` -> PASS (hot snapshot-codec + movement-validation checks).
  - `--movement-selftest` -> PASS.
  - `--movement-parity-selftest` -> PASS.
- Runtime / human acceptance: pending.

## Stage A status

- `src/network/client.cpp`, `src/network/server-melee.cpp`: deleted.
- `src/network/snapshot-chunks.cpp`, `src/network/movement-validation.cpp`: no
  longer in `cold`; dispatch-only bridges with hot behavior.
- Hot codec/validator behavior is editable via
  `hot-reload/hot-snapshot-codec.h` and `hot-reload/hot-movement-validation.h`
  (both in the manifest `headers`), proven headless.
