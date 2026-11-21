# DLL build timing and daemon inventory investigation

## Summary

Recorded fresh DLL build evidence in shared issues `issue-136d4553ff92` and
`issue-35fd6df3d947`, retained the requested 10-job setting, added the root
daemon count to the MIMITA PROCESSES summary, switched Windows process identity
discovery to native APIs, and specified daemon lock lifetime through active DLL
worker completion.

## Evidence

- Attempt `game-dll-20261121T155427526457Z-6240d5` captured one changed `.cpp`,
  used `jobs=10`, examined 642 source units, compiled 215 and skipped 427. It
  spent 21.465 s in changed-source preflight, 109.160 s in object-cache scan,
  594.109 s compiling, and 47.311 s linking. Its recorded end-to-end elapsed
  time was 810.257 s. It failed at link with unresolved symbols including
  `replayRecorderPtr`, `npcSystemPtr`, and `queryTransportSwapState`; no
  successful artifact is claimed.
- In that scan, parse and stat work totaled about 0.45 s, the dependency-index
  prepass took 97.163 s, and per-TU dependency checks totaled 11.333 s. There
  were 214 stale candidates but no stale-reason histogram. The remaining scan
  time and the reason for recompiling 215 TUs from one changed source remain
  investigation items.
- A separate older `jobs=0` worker was still running at about 16:08Z. It had two
  changed `.cpp` files, 49 compiled, 349 skipped, 291 stale candidates, a
  303.016 s scan, a 269.286 s prepass, and a 134.197 s changed-source
  preflight. Parsing and stat work totaled about 1.43 s. Its worker PID was
  34876, parented by daemon PID 19168.
- Native process inventory returned three same-checkout daemon PIDs (19168,
  13660, and 33892) in the 16:08Z snapshot. This confirms duplicate live
  processes existed. Concurrent writes to shared object outputs are a
  plausible link-failure cause, but have not been proven; the issue requests a
  serialized reproduction before attributing causality.
- The user screenshot's PowerShell `Get-CimInstance` failure is addressed in
  source by native Toolhelp process enumeration and native process identity
  queries. A direct Windows invocation returned live PID, executable, parent,
  start time, and command line. Dashboard visuals have not yet been reloaded
  and accepted against the new code.

## Changes

- `devscripts/dev_loop/game_build.py`: set the DLL worker's configured job
  count to 10 (the file also has unrelated existing edits; those were
  preserved).
- `devscripts/process_identity.py`: add native Windows process inventory and
  command-line identity support, avoiding WMI/PowerShell for discovery.
- `devscripts/dev-loop.py`: use native process inventory for daemon and
  dashboard identity, expose root daemon count/PIDs to the GUI, fail closed
  when daemon identity cannot be verified, and join/drain an active DLL worker
  before releasing the checkout lock.
- `devscripts/dev_loop/gui.py`: show verified daemon count/PIDs, warn on
  duplicates, and show unknown rather than zero on inventory failure.
- `docs/specs/dev-loop/dashboard.md`: specify daemon-count display and native
  inventory behavior.
- `docs/specs/dev-loop/dev-loop.md`: specify fail-closed duplicate detection
  and holding the daemon lock until every owned build reaches a terminal
  result.

## Validation and limits

- The native process inventory was invoked on Windows and returned process
  identities. Existing live processes have not been restarted or terminated.
- The 10-job attempt completed its compile phase but failed at link. This is
  not a successful performance or artifact-integrity result.
- One old all-core worker was still active during the final process snapshot;
  a safe matched 10-job retry and visual dashboard acceptance remain pending.
- No fresh EXE-mode comparison was run. “Mode 2” in the current specification
  means deferred agent source lanes, while the one-EXE path is a separate
  existing build mode. Switching output mode alone does not establish faster
  dependency scanning.

## Next proof

After all old DLL workers reach terminal results, verify exactly one daemon,
exercise the process panel, and perform matched no-op, one-source, and
header-fanout builds through the root dev-loop. Record per-TU stale reasons and
separate prepass, depfile, stat, progress, compile, and link wall time. Repeat
with concurrency disabled before testing shared-cache concurrency; compare
median and p95 latency, failure rate, and system CPU/RAM while preserving the
existing artifact/runtime proof boundaries.

At about 16:16Z, a later read-only inventory still found two same-checkout
daemons (PIDs 13660 and 33892) and two old workers. Worker PID 34876 remained
alive with parent PID 19168, which was no longer present in the same inventory;
worker PID 22464 remained under daemon 13660. Attempt
`game-dll-20261121T151936891977Z-a6abf2` was still marked running after about
56m25s, with 49 of 642 units compiled and no terminal result. This directly
supports investigating worker/daemon lock lifetime, but the snapshot alone
does not prove whether the worker is using CPU or blocked. Existing processes
were preserved.
