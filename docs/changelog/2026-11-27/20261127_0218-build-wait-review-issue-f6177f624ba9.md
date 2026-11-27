# issue-f6177f624ba9 Build-Wait Review

# Task

- Task ID: review build/test wait issue-f6177f624ba9 (wake-08578d19ed29)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 62.339s, wait_id `game-dll-build:2026-11-27T01:44:56.218Z`
- Summary: Verified the wait against in-flight attempt `game-dll-20261127T014456458183Z-893a11` (gen 751), its build-timings/resource sidecar, the pinned run 20261127_014331, and the wake queue. Appended review (verdict `confirmed-intrinsic-single-TU-build-cost; identity-defect-recurred; wake-queued-once-dispatch-late`) and set `related_build`/`related_run` on the issue. Appended identity occurrence to issue-9a4503f6ed68 and late-dispatch/misattribution occurrence to issue-84c584957e65. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-27T02:18Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7176d28a

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted config, devscript, source, and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog. Append-only issue records were written through `agent-coord.py issue context/review/suggest`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code or commit without human approval.

# Findings

- The wait was real work, not a stall. Wait `game-dll-build:2026-11-27T01:44:56.218Z` (62.339s vs 60.0s budget, source_generation 0, request `2026-11-27T01:44:56.049Z-3f8f9b01`) maps to attempt `game-dll-20261127T014456458183Z-893a11` (revision `e2d0bcf4...`). It started 01:44:56.458Z and SUCCEEDED as generation 751 at 01:47:42.710Z; `total_seconds` 166.0, 2 compiled / 644 skipped of 645 sources.
- Phases: changed_source_preflight 57.703s (slowest TU `src/vehicles/vehicle-system.cpp` 57.564s), link 80.297s over 645 inputs / 502,153,396 B of objects, compile(identity-provider) 3.109s, publish 0.625s; jobs=10. The 60s crossing (~01:45:58) was inside the single-TU preflight; ~104s of work (mostly the 80s link) remained. The flat 60s budget sits below normal single-TU-plus-link game-DLL wall time, so a healthy build tripped it.
- Wait-event evidence quality: the `wait.over_budget` and `wait.reviewer-wake` events ARE retained this occurrence in `logs/2026-11-27/20261127_014331/dev-loop/events.jsonl` (lines 63 and 62), unlike the adjacent gen-749 review (journal missing). The `reason` field was clean (`[GAME BUILD] revision=e2d0bcf4... sources=645 jobs=10 priority=high ...`), so the reason-truncation defect (issue-6e08d89b55e3) did NOT recur.
- Wait identity is stale (known defect): evidence `build_id` `2026-11-27T01:43:27.658897Z` is the last EXE artifact and `generation` 750 is the previously published game generation; the true in-flight attempt 893a11 / gen 751 / run 20261127_014331 was recoverable only from the reason revision and the journal. No `run_id` or `build_attempt_id` is stored and `related_build`/`related_run` were empty before this review. Root: `check_wait_budget` reads `self.latest_build`/`self.game_build_generation` (dev-loop.py:3896-3897) and `game_build_generation` is assigned only on completion (dev-loop.py:3259). Owner: issue-9a4503f6ed68 (and issue-7d3dcfd7433b for budget/dedupe).
- Exactly one reviewer wake: `wake-08578d19ed29` queued 01:46:04.240Z (.dev/agent-wake/events.jsonl line 3730). codex claimed/launched pid 12000 01:46:15.692/15.766Z then exited; wake.retry to opencode 01:55:03.147Z; opencode claimed/launched pid 34420 02:17:51.271/51.322Z (this session). Queue-to-final-launch 1907.08s (~31m47s), attempts=2, one wake.queued and no duplicate queue. Matches issue-84c584957e65.
- Wake-record misattribution recurred: the `dev-loop.wait.reviewer-wake` event at 01:46:16.187Z is stamped with wait_id `game-dll-build:2026-11-27T01:44:56.218Z` / issue-f6177f624ba9 but its `launch_result` embeds `wake-c6fb7ac32356` (issue-dea21a9c722d, wait `game-dll-build:2026-11-27T01:33:03.139Z`), not wake-08578d19ed29. Cause: `build_friction.wake_reviewer` runs `agent-wake.py run --once`, which drains the oldest pending wake and returns that result. Owner: issue-84c584957e65.
- Resource sidecar `game-dll-20261127T014456458183Z-893a11.resource.json`: 98 samples @1s over ~166s, mean host CPU 98.902%, peak 100%, minimum available memory 350,633,984 B (~334 MiB). Pressure observed, NOT proven as cause. The matched-pre-build-baseline / commit-charge gaps for attempt 893a11 were already appended to issue-2a5c5da53565 by the sibling gen-749 review, so no duplicate resource-gap report was filed here.
- Recurrence, not a duplicate: this is another instance of the known flat-60s-budget / stale-identity / late-misattributed-wake class. One issue and one wake exist for this wait_id; the flood is per-attempt aggregation across many wait_ids (policy owners issue-7d3dcfd7433b, issue-136d4553ff92, issue-e044a931c54b).

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the wait ID is the dedupe key, the exact-wait issue was reused, later observations were appended as occurrences on the owning defect issues, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/activity set/issue context/issue review/issue suggest`, and read-only inspection of `.dev/build-timings/game-dll-20261127T014456458183Z-893a11.{json,resource.json}`, `.dev/build-timings/game-dll-20261127T014348084585Z-96489e.json`, `logs/2026-11-27/20261127_014331/dev-loop/events.jsonl`, `.dev/agent-wake/events.jsonl`, `.dev/agent-wake/requests/wake-08578d19ed29.json`, and `.dev/state.json`.
- Review artifact: issue-f6177f624ba9 review (state in_review, reviewer build-wait-reviewer); `related_build`/`related_run` set to the attempt and run.

# Measured evidence

- Wait elapsed 62.339s vs 60.0s budget; attempt started 01:44:56.458Z; terminal success 01:47:42.710Z (generation 751, total 166.0s).
- Phase timings: changed_source_preflight 57.703s (vehicle-system.cpp 57.564s), link 80.297s (645 inputs / 502,153,396 B), publish 0.625s.
- Resource: 98 samples, mean host CPU 98.902%, peak 100%, min free 350,633,984 B (~334 MiB).
- Wake: one queued 01:46:04.240Z; codex launch 01:46:15.766Z then exit; opencode final launch 02:17:51.322Z; queue-to-final-launch 1907.08s; attempts=2.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on flat vs per-class budget / per-wait_id consolidation; <=5 min fallback wake drain; wait-record in-flight attempt/run identity; durable dev-loop wait-event journal under the root daemon; whether the 80s link or host saturation dominates this attempt.

# Agent handoff context

- Wait policy/budget/duplicate owner: issue-7d3dcfd7433b, issue-136d4553ff92, issue-e044a931c54b.
- Wait-record stale identity owner: issue-9a4503f6ed68 (identity occurrences) and issue-7d3dcfd7433b.
- Wake-delivery / misattribution owner: issue-84c584957e65.
- Journal-retention owner: issue-6e2d7b91e7d9 (journal WAS retained this occurrence; no action).
- Resource-pressure owner: issue-2a5c5da53565 (893a11 gap already recorded by the sibling gen-749 review; no duplicate appended).
- Reason-truncation owner: issue-6e08d89b55e3 (not triggered — reason was clean).
- Next exact step: owners confirm the smallest budget/dedupe/identity/journal checks and whether the 80s link is resource contention; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
