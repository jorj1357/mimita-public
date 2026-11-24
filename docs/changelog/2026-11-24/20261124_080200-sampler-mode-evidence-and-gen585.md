# Sampler mode validation and generation 585 evidence

## Objective

Continue the Mode 3 edit-to-live latency investigation with reliable build and
runtime evidence, while preserving the shared root dev-loop and all live game
processes.

## Findings

- Generation 585 attempt `game-dll-20261124T072533876573Z-7080a4` ran for
  517.609 seconds. It compiled 151 and skipped 490 of 641 translation units.
  The exact timeline accounts for the 151 as 147 dependents invalidated by
  `camera.h`, three directly changed `.cpp` files, and one generated module
  identity provider. Stage times were 41.953 seconds changed-source preflight,
  420.891 compile, 22.875 LLD link, 7.250 object-cache scan, and 0.594 publish.
  This reinforces that header fanout plus full compile/link dominate; the
  object scan is a small part of this attempt.
- Its resource sidecar sampled host CPU at 99.943% mean and available RAM as
  low as 12,816,384 bytes. Runtime mode was Bloodtest, not Juggernaut. These
  values show coincident pressure, not that the game caused build slowdown.
- The sidecar misclassified `client_actual_mode=repeat` from
  `dev_timer_tips.config_loaded`; the same run's authoritative
  `network.mode.active` event says Bloodtest. The sampler now only accepts
  actual mode from runtime mode events, requires fresh authoritative mode
  evidence on both sides, marks mismatches/unverified modes, and withholds
  aligned workload metrics unless modes match.
- Focused validation passed: 12 sampler unit tests, `py_compile`, and scoped
  `git diff --check`. The root dev-loop has not been restarted to load this
  Python change, so later sidecars must not be assumed to contain the fix yet.
- A fresh runtime check for run `20261124_075114` found server PID 380 and
  client PID 34868 on Juggernaut Horde / dust2cyberiav4. The server reports
  generation 587, 67 NPCs, and zero players; the client reports one player.
  Treat the roster as inconsistent. Server NPC phase events still report
  module generation 586. Server tick windows around 08:01:49–08:01:53 UTC
  average roughly 40–56 ms and peak at 109–468 ms; client frame slices around
  08:01:54–08:01:55 UTC have p95 4.7–7.7 ms and 9–17 over-budget frames.
  Neither the 60 Hz server nor 240 FPS client target is met.
- `ModuleHost::loadLocalModules` caches the local API generation, sets
  `mLocalModules`, and `ModuleHost::poll` returns early in local-module mode
  (`src/hot-reload/module-host.cpp:870,874,1155–1157`). This is a concrete
  stale-generation investigation lead. The NPC behavior module initializer
  passes `MIMITA_MODULE_GENERATION` into its API (`src/npc/npc-behavior-module.cpp:949`),
  and the game-build worker defines that macro from the generation identity
  provider (`devscripts/game_build_worker.py:845`). Therefore gen587 versus
  module_generation586 is not explained by a hardcoded NPC module version in
  current source; it is evidence that module registration/code identity did
  not advance with the outer DLL, though the exact stale-pointer/lifecycle step
  still needs runtime tracing.
- The lifecycle trace now narrows that lead: `gameShutdown()` calls
  `shutdownWholeGameEntry()` (`src/game/game-module.cpp:106–109`), which shuts
  down audio/perf and deletes the engine but does not shut down the resident
  `ModuleHost` (`src/engine/whole-game-entry.cpp:149–159`). `ModuleHost::startup`
  returns while `mStarted` is true (`src/hot-reload/module-host.cpp:1104–1108`),
  and the host loader retains the old DLL in `retiredModules_` during commit
  (`src/host/host-loader.h:193–204`). This can leave the host's cached local
  function pointers callable but bound to the old generation. Recommended
  prototype: a resident generation-commit invalidation/rebind step with explicit
  module state handling, followed by same-PID client/server proof that host
  DLL and NPC module generations match without stale callbacks or crashes.
  This is a one-time cold host/EXE change; it does not reduce the separate
  monolithic C++ compile/link time.

## Next work

1. Keep host DLL generation and NPC behavior module generation as separate
   activation checks; resolve why they differ before claiming NPC behavior
   hot reload.
2. Coordinate with the resource profiler before any new process sample or
   bounded callsite probe.
3. After source owners release the pending batch, run a clean one-`.cpp`,
   unchanged-header baseline; then measure a separate small behavior-module
   boundary and its link/activation costs. Preserve the MinGW monolithic DLL
   fallback.
4. Restart the root daemon only after confirming it will preserve owned live
   clients, then verify the new sampler's runtime alignment fields on a fresh
   attempt.

No C++ source, active compiler, game process, or dev-loop runtime was changed
or restarted in this work session.
