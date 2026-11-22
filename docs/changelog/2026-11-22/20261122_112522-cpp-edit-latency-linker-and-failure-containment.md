# C++ edit-to-live latency: linker and failure-containment checkpoint

## Task

- Goal: continue issue `issue-136d4553ff92` toward <=10-second edit-to-live under measured game load, preserving correct incremental builds and the root dev-loop authority.
- Related user concern: optional presentation-DLL failures should not fail or repeatedly delay the primary build; long-running goals should continue across “continue” turns.
- Branch / base: `checkpoint/one-checkout-condensation-20261111` / `9fd637d595a308483a9f82c88349a5915c82a0ca`.

## Changes in this work session

- Added a successful-result-only validation cache for resident-state and packet-registry codegen checks in `devscripts/dev_loop/game_build.py`. The cache validates the scripts, direct inputs and outputs, and packet source-manifest identity; failed checks still fail closed. Inputs are rechecked before worker launch.
- Added automatic game-DLL linker selection, preferring `ld.lld.exe` beside the configured MinGW compiler and falling back to GNU `ld.exe` when LLD is absent. Explicit `gnu` and `lld` selection remains available. Linker identity participates in cache identity so a published DLL linked by another linker is not incorrectly treated as equivalent.
- Preserved the existing single root dev-loop builder, MinGW compiler, successful DLL activation path, and running server. No game process was launched or stopped.

## Measurements and validation

- `python -m py_compile devscripts/dev_loop/game_build.py devscripts/game_build_worker.py devscripts/dev_loop/cli.py devscripts/dev-loop.py` passed.
- `git diff --check` passed for the four linker/control files; repository line-ending warnings remain informational.
- A matched restored-revision comparison used the same 642 object inputs (480,395,963 bytes; 666 link arguments). GNU link: 20.875 seconds. LLD link: 7.578 seconds, about 63.7% lower. LLD candidate generation 489 published successfully and activated in the already-running server PID 26748 at 2026-11-22T11:14:05.664Z.
- A subsequent root-loop request `2026-11-22T11:22:14.664Z-622530c8` with `--game-dll-linker auto` completed as a no-op: attempt `game-dll-20261122T112214877550Z-197f20`, 642 skipped, 0 application translation units compiled, 0 linked, 13.582 seconds total / 2.882 seconds worker. Generation 489 remained current. This verifies the auto request path on unchanged inputs, not an actual auto-selected relink.
- The codegen cache reduced a later unchanged no-op attempt to 11.782 seconds (both codegen checks cache-hit, 642 skipped); it remains above the <=10-second target. The preceding no-op attempts ranged from 15.360 to 19.719 seconds and were not perfectly matched for manifest and host load.
- Current root dev-loop status: run `20261122_011031` running; server PID 26748 alive; latest game-DLL generation 489; mode/status reports FCS-like identity with no clients, not the requested Juggernaut/AI-controller pressure case. The EXE artifact is marked stale. No gameplay behavior change was tested.

## Failure containment and goal continuity

- The user’s “failure like that” refers to the optional legacy presentation DLL nested in EXE setup. Existing `build.py` already keeps that helper fail-open, records the outcome, and skips an unchanged failed input fingerprint for up to 30 minutes; prior root-loop evidence shows the EXE succeeds and an identical retry skips the helper in 0 seconds. This work did not change that implementation. Required compile, link, generated-source, coherence, and publication gates remain fail-closed to prevent publishing stale or mixed code.
- The optional helper’s fingerprint does not yet cover all transitive headers. A dependency-aware fingerprint or `.d` generation is still needed to ensure an indirect header edit invalidates the negative cache.
- The gold guidance for long-running goals already exists in `docs/gold/20261030_033224-agent-session-handoff-gold.md`: continue across turns, checkpoint exact evidence, and do not treat optional-tool failure or a partial speedup as goal completion. Persistent goal remains active.
- Findings were appended to issue `issue-136d4553ff92`; the failure path remains cross-linked to `issue-ae248907f1f1` and the continuity guidance to `issue-91e3384b219b`.

## Outstanding work

- Auto mode has not yet been exercised on a changed candidate that actually relinks; perform a controlled root-loop request and verify the recorded selected linker.
- Reduce the remaining 11.8–13.6-second warm no-op path. Current issue evidence locates much of it before/around worker startup and manifest enumeration; keep measuring rather than dropping freshness checks.
- The single changed C++ translation unit still dominates edit builds (about 40.7 seconds in the recent canary), while a full DLL link remains 7.6 seconds even with LLD. Continue profiling before attempting TU splits or a function-patching architecture.
- Run a matched no-op, changed-source compile, and live activation under the requested Juggernaut/AI-controller pressure scenario; retain the current server and use the root dev-loop only.

## Coordination and limitations

- Shared files changed during their edit leases. Leases were released and four coordination conflict records were created (`conflict-28c39cc357c3`, `conflict-bf022f236b9a`, `conflict-5273af46a3ee`, `conflict-2babd9dc33d1`). The shared diffs were inspected; no further source writes were made after the conflicts were recorded. Another owner should reconcile these files before the next edit.
- These measurements demonstrate a faster linker and a successful no-op only. They do not demonstrate a sub-10-second changed-source activation, a sub-1-second full link, or the target stress workload.
