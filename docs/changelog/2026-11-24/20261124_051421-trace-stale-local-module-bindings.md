// 2026-11-24 05:14 UTC

# Trace stale local module bindings

- Task ID: session-a70e89727322
- Session ID: session-a70e89727322
- Owner: hot-reload-latency-root
- Focus: hot-reload activation correctness
- Summary: Traced why host game-DLL generation 581 can coexist with NPC module generation 577. The resident ModuleHost resolves local-module function pointers once, disables polling for those modules, and is not shut down by the game's DLL shutdown path. The loader keeps retired DLLs mapped, so the old callback is still valid but stale. Sent the finding to the build-latency and resource-profiler issues and coordinated with the active Bloodtest owner. No source code changed and no build/runtime controls were used.
- Status: investigation; goal remains active
- Date, time, timezone: 2026-11-24T05:14:21Z, UTC
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Final commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83

## Pre-existing changes

- Exact status output: the shared checkout already had extensive unrelated dirty and untracked files; they were preserved.
- Files not created or modified by this session: all source and config files.

## Requested behavior

An NPC behavior change in a saved C++ edit should execute in the running game without restarting the process, and the active module generation must match the published game-DLL generation.

## Specification alignment

- Current specification paths: `docs/specs/hotreload/hot-layer-latency-and-language.md`; `docs/specs/dev-loop/change-to-evidence-latency.md`.
- Exact requirements: prove the owner path in the running process; do not count DLL publication alone as activation; preserve the prior generation on failed activation.
- Why the change follows the specification: this investigation identified a specific stale callback path that prevents a game-DLL swap from proving folded NPC behavior is live.
- Conflicts or decisions: the deferred hot-layer specification does not authorize a runtime migration; this session proposes only a resident module rebind as the next correctness prototype.

## Exact implementation changes

No C++ implementation was changed. Source inspection and issue discussion only.

### Confirmed source path

- `src/host/host-runtime.cpp`, around lines 2702–2720: the swap quiesces the old API, calls its shutdown callback, commits the candidate DLL, then begins the new generation.
- `src/host/host-loader.h`, lines 193–225: commit moves the old HMODULE into `retiredModules_` and adopts the candidate; retired code remains mapped.
- `src/game/game-module.cpp`, lines 106–109, and `src/engine/whole-game-entry.cpp`, lines 149–161: game shutdown deletes the game engine and stops audio/perf services; it does not call ModuleHost shutdown or refresh.
- `src/hot-reload/module-host.cpp`, lines 785–883 and 1155–1158: local exports are resolved into `entry.api`/`entry.activeGeneration` once; `mLocalModules` is set; `poll()` immediately returns for local modules.
- `build/source-manifest.json`: `src/hot-reload/module-host.cpp` is owned by `resident`, so changing its lifecycle requires a cold EXE build and runtime replacement.

## Validation

- Tests and exact commands: no tests requested or run; read-only source inspection only.
- Build status: no build requested by this session.
- Runtime or hot-reload evidence: server run `20261124_022440_01`, PID 33892, logs `module.local.loaded` once at startup and later `npc.phase-window` records with `module_generation=577` while the published host DLL was generation 581. The root dev-loop had no registered client, so no paired in-game behavior proof exists.
- Output files: issue discussion in `issue-136d4553ff92`; coordinated the same finding with `resource-profiler`.

## Measured evidence

- Latest clean one-source DLL attempt: 49.506 seconds total; changed-source preflight 15.446 seconds, link 12.604 seconds, object-cache scan 0.005 seconds; generation 581 activation appeared in the server journal about 0.399 seconds after build completion.
- This is not a matched client/server behavior test and does not meet the <=10-second objective.

## Human acceptance

- Visual review: not performed.
- Gameplay review: not performed; no connected client.
- Multiplayer review: not performed.
- Still unverified: whether a resident ModuleHost rebind can safely initialize the new generation without losing persistent state, and whether the resulting 15–20-file edit path can meet the latency target.

## Agent handoff context

- Owner: hot-reload-latency-root
- Focus: continue the <=10-second Mode 3 edit-to-live goal.
- Files to check: `src/host/host-runtime.cpp`, `src/host/host-loader.h`, `src/hot-reload/module-host.cpp`, `src/game/game-module.cpp`, `src/engine/whole-game-entry.cpp`.
- What another agent should not redo: the static proof that DLL commit does not rebind local ModuleHost callbacks and the current server-only generation mismatch.
- Next exact step: after the Bloodtest owner releases the shared build/runtime window, coordinate a resident ModuleHost refresh/rebind prototype, then use a fresh connected Juggernaut Horde pair to prove matching module generation and changed NPC behavior.

## Related feature record

- `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92`

## Additional paired-runtime blocker

- Root dashboard launch request `dashboard-4a81a5c4` created run `20261124_051358` for Juggernaut Horde and failed at `client_reconciliation` after 15.265 seconds.
- The exact cause was `taskkill /PID 18752 /F` timing out after 8 seconds. The dev-loop journal identifies PID 18752 as a separate Bloodtest B client from run `20261124_043709`; `stopped_pids=[]`, no client was registered for the new run, and the existing root server PID 33892 was not replaced.
- This is recorded in launch issue `issue-651bb3982693`. I did not request the launch or control/stop any process. Do not retry client reconciliation until the Bloodtest owner releases that PID.
- New issue discussion evidence was appended after the original changelog body was drafted; the one session changelog remains this file.

## Existing state-safe module reload path

- `ModuleHost::loadCandidate` in `src/hot-reload/module-host.cpp` already validates a candidate DLL, bounds old-module state export to 1 MiB, imports state into the candidate, preserves the old module if validation or migration fails, then swaps the entry API and emits a `module.activated` event at the configured boundary (around lines 2180–2385).
- The catalog reload path returns immediately when `mLocalModules` is true (lines 2078–2079), and `poll()` does the same (lines 1155–1158). The current folded NPC module therefore bypasses the available candidate/state-migration path.
- Recommended sequence: first add a resident safe-point rebind for local module callbacks and prove module-generation equality; separately measure a small coarse-module build/link path using the existing candidate/state-transfer mechanics. That can remove the full-game link from those changes, but it does not establish <=10 seconds until a 15–20-file pressured test is measured. The deferred hot-layer path remains necessary for many-file edits that must avoid native compilation/linking entirely.
