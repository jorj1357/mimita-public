# issue-28feee119403: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-26T15:55:46.005Z — build-wait-reviewer

Reviewer: build-wait-reviewer

## Review: over-budget game-dll-build wait

Reviewed by `build-wait-reviewer` from the dev-loop journal and build-timing artifacts. No source edit, no duplicate issue, no runtime proof claimed.

### Observed facts (direct)
- Wait `game-dll-build:2026-11-26T15:43:01.573Z`, stage `game_build`, elapsed `60.025s` vs budget `60.0s` (over by `0.025s`). Journal: `logs/2026-11-26/20261126_145549/dev-loop/events.jsonl` `dev-loop.wait.over_budget` (line 147, recorded `2026-11-26T15:44:06.249Z`).
- Wake contract met: exactly one `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` exist for this `wait_id`. Wake `wake-3b1d8985a679` (opencode) created `15:44:03.071Z`, launched `15:45:42.502Z` (~99s queue-to-launch). Journal line 146; `.dev/agent-wake/requests/wake-3b1d8985a679.json`.
- Build succeeded: attempt `game-dll-20261126T154301815009Z-41b473`, generation `691`, status `ok`, published `15:44:10.127Z`. Worker `total_seconds=62.407`, `worker_total_seconds=54.766`, `compile_seconds=45.016`, `link_seconds=5.953`; attempt wall `total_seconds=68.593`. Source: `.dev/build-timings/game-dll-20261126T154301815009Z-41b473.json`.
- Only changed source `src/vehicles/vehicle-system.cpp` compiled `24.992s`. Three extra TUs recompiled because the preceding build left empty/invalid objects: `src/network/vehicle-networking.cpp` (`missing or invalid dependency file`, `19.343s`), `src/sim/simulate-tick.cpp` (`object file is empty`, `17.292s`), `src/vehicles/vehicle-commands.cpp` (`object file is empty`, `14.465s`). Same timing json `cache_invalidations` / `slowest_compiles`.
- Predecessor generation `690` attempt `game-dll-20261126T154118055873Z-1d5285` FAILED at compile `15:42:31` on `src/vehicles/vehicle-system.cpp:1322` (`cannot convert std::shared_ptr<const VehicleProfile> to const VehicleProfile*`) after `60.765s`; its own wait also exceeded budget (issue-ab0d4daf980a, `60.613s`). Journal lines 117-125.
- Wait clock includes staging: `game_build_started_utc=15:43:01.573Z` but worker started `15:43:09.508Z` (~8s of manifest/validation/snapshot/codegen before compile). Worker-only `54.766s` is under 60s. Journal lines 129-139.
- Resource sample (12 logical CPUs): `mean_host_cpu_percent=92.962`, `peak=100.0`, `minimum_available_memory_bytes=2,049,060,864` (~1.91 GiB of 16.8 GB), 56 samples @1s over 68.468s. `.dev/build-timings/game-dll-20261126T154301815009Z-41b473.resource.json`.

### Identity defects on this record (known, cross-linked)
- Stored `generation=688` and `build_id=2026-11-26T06:10:41.962780Z` (last published artifact) while the in-flight build was generation `691` / attempt `41b473`; `source_generation=13` and `request_ids` are correct.
- `reason` is a mid-JSON tail (`:55766.0,\"build_attempt_id\":\"game-dll-20261126T154301815009Z-41b473\",...`), not a human reason (issue-6e08d89b55e3).
- Recorded as one occurrence on issue-9a4503f6ed68; no duplicate issue.

### Inference (not proven)
- The overrun is genuine but marginal: the check fired at `60.025s`; the build then ran ~3s more and finished at `62.4s` worker / `68.6s` wall. The 1s check interval means it could be filed anywhere in `60.0-61.0s`.
- Largest avoidable contributor is the three empty/invalid objects inherited from the aborted gen-690 build. They were genuine stale artifacts (correctly recompiled for safety) but added ~51s of compile-sum (parallel) and pushed a ~25s single-TU rebuild over the budget. Worker-only time was under 60s, so staging + extra TUs together crossed the line.
- Host contention plausibly lengthened compiles (see gap `build-resource-sampling:game-dll-20261126T154301815009Z-41b473`), but there is no matched no-build/low-load baseline and the live mode is not journal-verified; pressure is NOT the proven cause.

### Contrary evidence / limits
- No missing or forbidden events; build succeeded and the issue/wake contract is satisfied. This is latency-budget friction, not a correctness defect.
- `record_wait_incident` intentionally creates a new issue per distinct `wait_id` (`devscripts/build_friction.py:143-200`), so sibling "Wait over budget" issues are by design, not accidental duplicates; volume/noise is a policy question tracked in issue-7d3dcfd7433b / issue-9a4503f6ed68.
- This review does not re-triage the many existing open sibling wait issues.

### Next discriminating check
- After a clean successful predecessor (no aborted objects), rebuild only `src/vehicles/vehicle-system.cpp` at matched host load; pin the attempt id and record per-stage durations plus a resource sample. Falsifiable hypothesis: with zero inherited empty objects, end-to-end stays under 60s. If it still exceeds 60s, staging (~8s) and/or host contention becomes the primary suspect. Owner left to issue-136d4553ff92 (build-coordinator) or issue-7d3dcfd7433b (launch-latency-investigation); unowned here.

Verdict: over-budget wait confirmed; issue fields complete (readiness ready=true, missing=[]); no code fix claimed.
