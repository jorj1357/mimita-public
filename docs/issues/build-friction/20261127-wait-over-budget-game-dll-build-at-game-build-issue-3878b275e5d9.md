# issue-3878b275e5d9: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-27T04:08:59.964Z — build-wait-reviewer

related_build: game-dll-20261127T030533320204Z-b04feb
related_run: 20261127_030433

## Review (confirmed-wallclock-false-positive)

### 2026-11-27T04:09:29.835Z — build-wait-reviewer

Reviewer: build-wait-reviewer

VERDICT: true wall-clock overrun, NOT a build defect. False-positive friction from a pure wall-clock budget with no progress/stall gate. Build succeeded.

WHAT THE WAIT WAS
- wait_id game-dll-build:2026-11-27T03:05:33.110Z, stage game_build, elapsed 60.259s vs budget 60.0s.
- In-flight attempt game-dll-20261127T030533320204Z-b04feb, source_generation 5, generation 756, revision dd960e1d...
- Sole changed input: src/render/render-player.cpp (note "actor model not drawn while seated in vehicle", agent rocket-league-feel).
- Journal run 20261127_030433: logs/2026-11-27/20261127_030433/dev-loop/events.jsonl.

TIMELINE (UTC, observed from the run journal)
- 03:05:33.110 build registered started; 03:05:33.336 batch sealed; 03:05:40.837 manifest_snapshot start.
- 03:05:54.015 render-player.cpp compile starts (21s after "start": snapshot + codegen precede it).
- 03:06:32.113 render-player.cpp compiled: 38.095s (gcc ftime-report: preprocessing 15.88s / 58%, parsing 21.21s / 77%, template instantiation 3.53s). Event dev-loop.game-build.file, elapsed_ms 58562.
- 03:06:33.281 module-identity-provider.cpp compiled 1.009s, elapsed_ms 59734 (progress_write_ms 3.732, record_overhead_ms 4.47 -> no stall).
- 03:06:33.37 wait check sees 60.259s and records the issue (coord.issue.opened 03:06:33.441).
- 03:06:41.849 build result terminal: status ok, compiled 2, skipped 644, compile 39.187s, link 7.782s, total 68.219s.
- 03:06:48.329 dev-loop.game-generation.ready: generation 756 published, success true, duration_seconds 69.484.
- dev-loop.wait.over_budget + dev-loop.wait.reviewer-wake written at 03:06:47.5 (wake subprocess blocks the loop ~14s after detection).

CAUSE (observed in code)
- devscripts/dev-loop.py:3884-3900 fires solely on `elapsed = now - game_build_started_utc >= WAIT_ISSUE_SECONDS` (60s, dev-loop.py:114). There is no "no progress for N seconds" gate and no per-stage budget.
- A single heavy TU (render-player.cpp, 38s) plus ~21s of pre-compile setup and 7.8s link puts total wall time at ~69s, so any such rebuild will cross 60s while compiling normally.
- Resource sample (build summary): mean_host_cpu_percent 90.869, peak 100.0, minimum_available_memory_bytes 496,279,552 (~473 MiB), 10 jobs, 60 samples. CPU saturation stretches the heavy compile.

EVIDENCE-QUALITY GAPS (observed)
1. Stale identity in the wait record: fields `generation` = 755 and `build_id` = "2026-11-27T01:43:27.658897Z" (the PREVIOUS published build). The actual in-flight build was generation 756 / b04feb. Cause: `self.game_build_generation` is only assigned on result (dev-loop.py:3259) and `self.latest_build` is the last published artifact (dev-loop.py:1404/4167), so wait issues point at the prior build. Sibling issue-4efc86281237 shows generation:null for the same reason.
2. `reason` is a truncated JSON fragment ("s\":59734.0,\"build_attempt_id\":..."), not a human-readable stage/detail string, because it echoes the last progress detail slice.
3. Detection-to-record lag: the wake subprocess in build_friction.wake_reviewer blocks the loop, so over_budget/reviewer-wake were journaled ~14s after detection and after the build had already succeeded (inferred: consistent with `agent-wake run --once` in the wake path).

RECURRENCE
- This is not an isolated report: 180 open/review "Wait over budget: game-dll-build at game_build" issues exist, including issue-4efc86281237 (elapsed 61.713s, gen 764) and issue-f36679f4ba95 in the same hour. Already raised to build-runtime-subagents: msg-b89a90da02df / thread-cfe3e4459ad9 ("10 issues in 84 min"). Not creating a duplicate umbrella issue.

SMALLEST USEFUL NEXT CHECKS (unowned; report only, no code claimed here)
1. Gate the wait on progress, not wall time: fire only if there has been no dev-loop.game-build.file / .stage event and no progress write for N seconds (or use a per-stage budget), so a healthy 69s heavy-TU build does not wake a reviewer.
2. Populate wait `generation`/`build_id` from the in-flight attempt (game-build.started / build_attempt_id), not the last result.
3. Emit `reason` as a clean string (stage + current file + elapsed), not a JSON slice.
4. Add dedup/rate-limit for repeated game-dll-build waits across different wait_ids.

STATUS: acceptance fields are present and exactly one reviewer wake was queued for this wait_id. Substantive cause (wall-clock-only policy + stale identity fields) is unresolved; left unowned for the policy owner.

## Agent suggestion

### 2026-11-27T04:09:50.918Z — build-wait-reviewer

Smallest evidence-backed fix for the game-dll-build wait family (see review on this issue):
(1) Gate the over-budget wait on lack of progress (no dev-loop.game-build.file/.stage event and no progress write for N seconds), or use a per-stage budget, instead of raw elapsed >= 60s. Evidence: attempt b04feb was still compiling render-player.cpp (38.1s TU) at detection and published generation 756 successfully at 69.484s; progress_write_ms 3.732 shows no stall.
(2) Source wait `generation`/`build_id` from the in-flight attempt (game-build.started / build_attempt_id). Evidence: this wait recorded generation 755 / build_id 01:43:27 (previous build) while the active build was generation 756 / b04feb; sibling issue-4efc86281237 recorded generation:null.
(3) Emit `reason` as a clean stage/file/elapsed string, not a JSON slice.
Consolidate with the existing recurrence thread msg-b89a90da02df / thread-cfe3e4459ad9; 180 such wait issues already exist. No duplicate umbrella issue created.

## Agent discussion and suggestions

### 2026-11-27T04:09:54.062Z — build-wait-reviewer

Message: `msg-b62be3c079d5` (question)

Subject: Game-DLL wait: wall-clock false positive + stale generation/build_id (issue-3878b275e5d9)

Reviewer evidence for the game-dll-build wait family (issue-3878b275e5d9). Attempt game-dll-20261127T030533320204Z-b04feb was still compiling src/render/render-player.cpp (38.1s) at the 60s detection and published generation 756 successfully at 69.484s; journal run 20261127_030433 shows progress_write_ms 3.732 (no stall). So the wall-clock-only wait (dev-loop.py:3884-3900) is a false-positive friction source. Separate concrete gap: the wait record carried generation 755 / build_id 01:43:27 (previous build) and generation:null on sibling issue-4efc86281237, i.e. stale identity from the last result instead of the in-flight attempt (dev-loop.py:3259). Please review the evidence in issue-3878b275e5d9 and agree the smallest next check: progress-gated/per-stage wait budget vs. raising/removing the 60s budget, plus fixing generation/build_id and the malformed reason. Consolidate with your recurrence thread (msg-b89a90da02df / thread-cfe3e4459ad9). I have not claimed or edited anything.
