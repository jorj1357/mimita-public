# issue-4fb7a24c54b0 Build-Wait Review

# Task

- Task ID: review build/test wait issue-4fb7a24c54b0 (wake-6f86e0fbb409)
- Session ID: session-2ca65055273a
- Work session: work-a6770368f72e
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.924s, wait_id `game-dll-build:2026-11-26T16:56:24.029Z`
- Summary: Verified the wait against in-flight attempt `game-dll-20261126T165624137995Z-32c560`, its build-timings/resource sidecar, the run journal, and the wake queue. Appended one review (verdict `reviewed-needs-followup`) and set `related_build`/`related_run` on issue-4fb7a24c54b0. Appended occurrence evidence to issue-6e08d89b55e3 (mid-JSON reason), issue-7d3dcfd7433b (stale attempt build_id/generation), issue-84c584957e65 (late fallback dispatch), issue-2a5c5da53565 (host CPU/memory pressure), and issue-9a4503f6ed68 (per-wait_id duplicate noise / budget policy). No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T17:26:21Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7176d28a

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted config, devscript, and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through `CoordStore.issue_review` / `issue_context` / `issue_suggest`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code or commit without human approval.

# Findings

- The wait was real work, not a stall. Attempt `game-dll-20261126T165624137995Z-32c560` (gen 698, source_generation 20, revision `471a974f...`, request ids `2026-11-26T16:56:23.707Z-38e6610b:mode3:game` and `2026-11-26T16:56:48.675Z-850e6df8`) started 16:56:24.137Z and SUCCEEDED at 16:58:55.298Z. `total_seconds` 151.156, `worker_total_seconds` 145.766; phase compile 134.375s (worker `compile_seconds` 140.329), 64 TUs compiled, `compile_max_ms` 57.319s; link 2.422s; object_scan 1.672s. The 60s clock crossed inside compile and the build finished ~90s later.
- Exactly-once contract held: `dev-loop.wait.over_budget` (created=true) and `dev-loop.wait.reviewer-wake` (queued=true) are present once for this wait_id at `logs/2026-11-26/20261126_165201/dev-loop/events.jsonl` lines 128 and 127; exactly one `wake.queued` was created for wake-6f86e0fbb409 at 16:57:27.591Z.
- Delivery did not hold (known issue-84c584957e65): codex claimed/launched pid 19112 at 16:57:38.674Z and exited; `wake.retry` -> opencode at 17:09:06.725Z; opencode launched pid 4136 at 17:10:10.197Z. Queue-to-effective-launch ~12m43s, attempts=2, one queued wake and no duplicate launch.
- Resource sidecar `game-dll-20261126T165624137995Z-32c560.resource.json`: `status=captured`, 119 samples @1s over 151.109s, mean host CPU 91.693%, peak 100.0%, minimum available memory 51,388,416 B (~49.0 MiB), no sampler errors; host logical CPUs=12 (so mean CPU is ~11.0/12 logical CPUs of whole-machine capacity). `runtime_alignment` client mode stale/unreported (client PID 11544, arena1, ~440 FPS, 0 NPC in view). CPU saturation is expected from a 64-TU compile; low free memory is a candidate only, not a proven cause.
- Recurrence, not a duplicate issue: three distinct same-titled game-dll-build/game_build waits in ~19 min, all successful — issue-404bd5b44b6e (16:49:17.979Z, gen 696 attempt `...164918058647Z-26c738`, 141.047s), this issue (16:56:24.029Z, gen 698 attempt `...165624137995Z-32c560`, 151.156s), issue-45733b82bbbc (17:07:53.638Z, gen 702 attempt `...170753867026Z-7428d5`, 103.313s). The flat 60s budget is below normal full game-DLL compile time (100-150s). `build_friction.py:143-169` keys uniqueness on `wait_id`, so each build opens a new issue — matching this issue's risk note; consolidation/cooldown or a per-kind budget is a policy decision (issue-9a4503f6ed68 / issue-e044a931c54b).
- Stale-identity defect (known issue-7d3dcfd7433b): the wait record carries `build_id 2026-11-26T16:48:56.265247Z` (last published artifact) and `generation=null`, while the in-flight attempt was gen 698; sibling issue-404bd5b44b6e (gen 696) carries the identical build_id. Stage is hardcoded `game_build` (`dev-loop.py:3891`). The correct attempt id was recoverable only from the truncated reason tail; this session set `related_build`/`related_run` to the true attempt.
- Truncated-reason defect (known issue-6e08d89b55e3): `reason` is a mid-JSON tail (`"s":57812.0,"build_attempt_id":...`) from `dev-loop.py:3112` `detail = line.rstrip()[-240:]` read at `dev-loop.py:3892`; the actual compile phase is not readable from the record.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the wait ID is the dedupe key, the exact-wait issue was reused, later observations were appended as occurrences on the owning defect issues, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/activity set/issue show/issue context/issue suggest/issue review`, `dev-loop.py --status`, and read-only inspection of `logs/2026-11-26/20261126_165201/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T165624137995Z-32c560.{json,resource.json}`, `.dev/build-timings/game-dll-20261126T164918058647Z-26c738.json`, `.dev/build-timings/game-dll-20261126T170753867026Z-7428d5.json`, and `.dev/agent-wake/events.jsonl`.
- Review artifact: issue-4fb7a24c54b0 review (verdict `reviewed-needs-followup`, state `in_review`, reviewer `build-wait-reviewer`); `related_build`/`related_run` set to the in-flight attempt and run.

# Measured evidence

- Wait elapsed 60.924s vs 60.0s budget; issue created 16:57:25.136Z; attempt started 16:56:24.137Z; terminal success 16:58:55.298Z (generation 698, total 151.156s).
- Phase timings: compile 134.375s, compile_sum 1,268,556ms across 64 TUs, compile_max 57.319s, link 2.422s, object_scan 1.672s.
- Resource: 119 samples/151.109s, mean host CPU 91.693%, peak 100%, min free 51,388,416 B, 12 logical CPUs, no sampler errors.
- Wake: one queued 16:57:27.591Z; codex launch 16:57:38.674Z then exit; opencode final launch 17:10:10.197Z; ~762.6s queue-to-effective-launch; attempts=2.
- Sibling attempts: gen 696 `26c738` total 141.047s (compile 131.703s, 63 TUs, max 59.296s); gen 702 `7428d5` total 103.313s (compile 79.343s, 21 TUs, max 44.446s). All succeeded.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget/per-kind budget and per-wait_id duplicate consolidation; reliable <=5 min fallback wake drain; wait-record in-flight attempt/run identity; whether host memory pressure contributed to compile time.

# Agent handoff context

- Wait policy/budget/duplicate owner: issue-9a4503f6ed68 and issue-e044a931c54b; issue-136d4553ff92 for stage accounting. Occurrence/evidence appended.
- Wake-delivery owner: issue-84c584957e65 (occurrence appended).
- Resource-pressure owner: issue-2a5c5da53565 (32c560 occurrence plus new gap_keys `build-resource-sampling:game-dll-20261126T172047133775Z-30f81f` (gen 704) and `...171310909318Z-faf8c7` (gen 703) appended; both captured but missing matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation).
- Reason-truncation owner: issue-6e08d89b55e3 (occurrence appended).
- Stale-identity owner: issue-7d3dcfd7433b (occurrence appended).
- Next exact step: the owners confirm the smallest budget/dedupe/identity checks; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
