# Mode 3 reverted target filtering

## Scope

Added a net-delta check when an explicit Mode 3 batch is sealed. It hashes only paths already recorded as changed and compares them with the published target source manifest. Reverted paths can therefore be removed from the target set without rescanning the repository or dependency graph.

## Evidence

- The prior generation-27 batch retained a reverted `src/host/host-runtime.cpp` path and selected both the cold EXE and game-DLL targets. That added an avoidable EXE build before the DLL attempt.
- The published EXE identity records source generation 27. The successful DLL attempt also records source generation 27, and its revision matches `build/game/current.json`.
- The DLL source snapshot manifest matches the current bytes of `src/host/host-runtime.cpp`; the new net-delta helper classifies that exact restored path as reverted.
- A game manifest may stand in for a missing cold manifest only when both artifacts share a nonzero generation, the same Mode 3 batch request ID, and the game revision matches its last successful attempt. Unknown provenance fails open.
- No C++ build was requested, no executable was published, and no game process was restarted by this change.

## Validation

- `python -m devscripts.quality.test_mode3_batch_net_delta`: 6 tests passed.
- `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop/batch_targets.py devscripts/quality/test_mode3_batch_net_delta.py`: passed.
- `git diff --check` on the owned files: passed; Git reported only its LF-to-CRLF working-copy warning for `devscripts/dev-loop.py`.
- Direct historical snapshot check: source generation 27 matched on EXE and DLL; current and attempt revisions matched; restored `src/host/host-runtime.cpp` was filtered from the cold target.

## Limits and next proof

The direct historical check validates this known reverted path, and the focused tests validate the provenance guard. After a controlled root dev-loop stop/ensure, the running daemon reports PID 17268 and its source hash matches the watcher's current on-disk hash. The registered Juggernaut runtime remained on server PID 24692/client PID 23980; no game process was restarted. A real Mode 3 batch has not yet been sealed through the updated daemon. The next controlled step is to use one intentionally reverted compiler-input path and confirm that the sealed event reports the removed path and queues no cold target. This control-path fix does not shorten actual TU compilation or DLL linking; a matched save-to-live DLL run remains required to measure progress toward the 10-second goal.

## Retained single-source timing comparison

Parsing 350 retained DLL timing summaries found 61 successful attempts with one changed path and at most two compiled objects. The fastest recent example was `src/npc/npc-world-query.cpp`: 2.499 s changed-source preflight, 3.500 s link, and 13.859 s total. Another one-path attempt took 18.490 s. These are historical worker results under uneven machine load, not matched save-to-live measurements, but they show that current pipeline work misses 10 s even for one source. The latest 20-TU attempt took 17.732 s changed-source preflight and 5.365 s to link 642 objects / 483.5 MB; it took 33.783 s worker time and 34.559 s attempt wall.

The current `ModuleHost::startup` path calls `loadLocalModules`; that sets `mLocalModules`, and `poll()` returns immediately for local modules. The module API already has candidate loading/activation methods, but module-owned builds must not bypass the root dev-loop. A meaningful multi-file latency path therefore needs a root-dev-loop-owned coarse candidate module or hot behavior layer; stage tuning in the monolithic game DLL alone cannot meet the requested budget. A runtime/C++ prototype and matched activation measurement are still outstanding.

## Provenance correction

The first guard was not safe across daemon restarts: `DevLoop.__init__` resets
its source generation to zero, so generation 27 can occur in separate daemon
epochs. Generation and revision equality alone do not prove the EXE and DLL
manifests describe the same published batch. The cold-manifest fallback now
also requires the EXE identity and game attempt to carry matching Mode 3 batch
IDs (`:mode3:cold` / `:mode3:game`). New EXE identities persist their request
IDs; older artifacts without them fail open and cannot use the game manifest
as a cold baseline. This may retain an otherwise reverted cold path until a
cold artifact is published with batch provenance, but cannot drop a real cold
change due to reused generation numbers.

The same restart review found that `.dev/state.json` records a pending Mode 3
batch but startup previously initialized its in-memory queue empty. Startup now
restores only a validated pending, unsealed batch; an active batch is never
replayed. The restored source paths still pass through the normal seal-time
hash and manifest checks. This allows a controlled daemon reload to pick up
new Python code without silently discarding the waiting profiler batch.
