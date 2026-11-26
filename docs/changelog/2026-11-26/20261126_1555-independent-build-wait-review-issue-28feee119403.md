# issue-28feee119403 Build-Wait Review

# Task

- Task ID: review build/test wait issue-28feee119403 (wake-3b1d8985a679)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.025s, wait_id `game-dll-build:2026-11-26T15:43:01.573Z`
- Summary: Verified the wait against attempt `game-dll-20261126T154301815009Z-41b473`, its build-timings/resource sidecar, the run journal, and the wake queue. Appended one review (verdict `reviewed`), appended the resource gap `build-resource-sampling:game-dll-20261126T154301815009Z-41b473` to issue-2a5c5da53565, appended one identity/reason occurrence to issue-9a4503f6ed68, and sent one bounded question to launch-latency-investigation. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T15:55Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted edits (config, devscripts, src, docs, shaders). This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through `CoordStore.issue_review` / `issue_suggest` / `issue_occurrence` plus one coordination question.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- The wait was real work, not a stall. Attempt `game-dll-20261126T154301815009Z-41b473` succeeded: generation 691, revision `8029c2c70b770842bceca677784928a48d5f050dcc693aa5b14467b88f275a8d`, changed only `src/vehicles/vehicle-system.cpp`, worker_total 54.766s, build worker total 62.407s, attempt wall 68.593s, published 15:44:10.127Z.
- The flat 60s budget fired at 60.025s (over by 0.025s) during a legitimate build. The check ran during the final TU; the build finished ~3s later. Journal `logs/2026-11-26/20261126_145549/dev-loop/events.jsonl` lines 146-147.
- Cause of crossing: the changed TU `src/vehicles/vehicle-system.cpp` compiled 24.992s, and three extra TUs were recompiled because the immediately preceding generation-690 build failed and left empty/invalid objects: `src/network/vehicle-networking.cpp` (missing/invalid dependency file, 19.343s), `src/sim/simulate-tick.cpp` (object file empty, 17.292s), `src/vehicles/vehicle-commands.cpp` (object file empty, 14.465s). Cache-invalidation reasons in `.dev/build-timings/game-dll-20261126T154301815009Z-41b473.json`.
- Predecessor failure: generation 690, attempt `game-dll-20261126T154118055873Z-1d5285`, failed compile at 15:42:31 on `src/vehicles/vehicle-system.cpp:1322` (`cannot convert std::shared_ptr<const VehicleProfile> to const VehicleProfile*`) after 60.765s; its own wait also exceeded budget (issue-ab0d4daf980a, 60.613s). Journal lines 117-125.
- The wait clock starts at request acceptance (`game_build_started_utc` 15:43:01.573Z) but the worker started 15:43:09.508Z, so ~8s of manifest/validation/snapshot/codegen staging is inside the budget window; worker-only 54.766s was under 60s.
- Auto-intake held: exactly one `dev-loop.wait.over_budget` (created=true) and one `dev-loop.wait.reviewer-wake` exist for this wait_id (both 15:44:06.249Z). Wake `wake-3b1d8985a679` (opencode) created 15:44:03.071Z, launched 15:45:42.502Z (~99s queue-to-launch) and produced this review.
- Record defects reproduced: (1) build_id `2026-11-26T06:10:41.962780Z` is the prior published artifact, not the in-flight attempt; (2) generation=688 vs in-flight 691; (3) `reason` is a mid-JSON tail `:55766.0,"build_attempt_id":"game-dll-20261126T154301815009Z-41b473",...`. source_generation=13 and request_ids were correct.
- Resource sidecar `game-dll-20261126T154301815009Z-41b473.resource.json`: status captured, 56 samples @1s over 68.468s, 12 logical CPUs, mean host CPU 92.962%, peak 100.0%, minimum available memory 2,049,060,864 B (~1.91 GiB). Competing whole-host averages included dwm 8.66%, firefox 5.07%, MsMpEng 4.53%, obs64 3.59%, ChatGPT 3.35%; tracked `mimita.exe` averaged 14.92% whole-host. dev-loop reported mode vehicle_freeplay / arena1 / 0 NPCs but runtime_alignment=unavailable, so this is observation only, not proven cause.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the exact wait is recorded, the class recurrence and resource gap were appended to the existing trackers, and one reviewer wake was queued exactly once.

# Exact implementation changes

- None. Append-only issue records, one coordination question, and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/issue review/issue suggest/ask/edit begin/edit commit`, and read-only inspection of `logs/2026-11-26/20261126_145549/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T154301815009Z-41b473.{json,jsonl,resource.json}`, and `.dev/agent-wake/requests/wake-3b1d8985a679.json`.
- Review artifact: issue-28feee119403 review (verdict `reviewed`) at 15:55:46Z; resource gap appended to issue-2a5c5da53565; identity/reason occurrence appended to issue-9a4503f6ed68; one bounded question sent to launch-latency-investigation (thread-95cf3cdabf13).

# Measured evidence

- Wait elapsed 60.025s vs 60.0s budget; build succeeded at worker 54.766s / attempt 68.593s (generation 691); compile 45.016s, link 5.953s; 5 compiled / 640 skipped of 644; jobs 10.
- Extra recompiles from aborted predecessor: 19.343s + 17.292s + 14.465s (parallel) plus the 24.992s changed TU.
- Resource: 56 samples/68.468s, mean host CPU 92.962%, peak 100%, min free 2,049,060,864 B; unmatched baseline/commit-charge/journal-aligned-mode unavailable.
- Wake: one queued and one reviewer-wake for this wait_id; ~99s queue-to-launch.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget/dedupe and staging-clock inclusion (issue-7d3dcfd7433b); avoidable recompile after an aborted build (issue-136d4553ff92); matched pre-build resource baseline, host commit charge/limit, and journal-aligned live mode (issue-2a5c5da53565); wait-record in-flight attempt/generation identity and reason text (issue-6e08d89b55e3).

# Agent handoff context

- Wait policy/duplicate/identity owner: issue-7d3dcfd7433b (owner launch-latency-investigation); recurrence noted, one bounded question sent.
- Build-stage owner: issue-136d4553ff92 (owner build-coordinator); next check is a matched single-TU rebuild after a clean predecessor to test whether the inherited empty objects are the avoidable latency.
- Resource sampling: gap_key `build-resource-sampling:game-dll-20261126T154301815009Z-41b473` appended to issue-2a5c5da53565; not duplicated.
- Next exact step: owner/human confirms the smallest wait-budget/staging and aborted-object-recompile checks; shared dev-loop changes need human approval.
- Do not redo: this attempt's evidence collection. Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
