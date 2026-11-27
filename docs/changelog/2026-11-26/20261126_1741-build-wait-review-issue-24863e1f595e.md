# issue-24863e1f595e Build-Wait Review

# Task

- Task ID: review build/test wait issue-24863e1f595e (wake-04a89c03a141)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.731s, wait_id `game-dll-build:2026-11-26T17:13:10.456Z`
- Summary: Verified the wait against in-flight attempt `game-dll-20261126T171310909318Z-faf8c7` (gen 703), its build-timings/resource sidecar, the run journal, and the wake queue. Appended review id 485 (verdict `confirmed-legitimate-wait-not-stall; identity-defect-recurred; wake-queued-once-dispatch-late`) and set `related_build`/`related_run` on issue-24863e1f595e. Appended occurrence evidence to issue-9a4503f6ed68 and issue-7d3dcfd7433b (stale wait-record identity), issue-84c584957e65 (late fallback dispatch), and issue-136d4553ff92 (build duration / resource evidence). No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T17:41Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7176d28a

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted config, devscript, and docs edits (42 status entries). This session edited no source and reverted nothing.
- Files created by this session: this changelog only. Append-only issue records were written through `agent-coord.py issue context/review/suggest`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code or commit without human approval.

# Findings

- The wait was real work, not a stall. Attempt `game-dll-20261126T171310909318Z-faf8c7` (gen 703, source_generation 10, revision `5eb31cd5...`, request id `2026-11-26T17:13:09.744Z-32106cf1:mode3:game`) started 17:13:10.909Z and SUCCEEDED at 17:15:28.111Z. `total_seconds` 137.203, `worker_total_seconds` 106.875; compile 92.001s (`compile_sum` 299,191ms, 5 compiled / 640 skipped), link 11.984s, changed_source_preflight 90.735s.
- Four large TUs changed by two build notes compiled concurrently: `src/vehicles/vehicle-system.cpp` 90.356s, `src/engine/engine-tick-render.cpp` 72.971s, `src/network/vehicle-networking.cpp` 71.164s, `src/engine/engine-tick-ui.cpp` 63.485s. packet_registry codegen completed 17:13:33.821Z; compiles submitted ~17:13:37; first TU finished 17:14:40.896Z. A single TU exceeded the 60s budget on its own, so the flat 60s budget is shorter than a normal multi-TU build here.
- Exactly-once contract held for the wake: `dev-loop.wait.over_budget` (created=true) and `dev-loop.wait.reviewer-wake` (queued=true) appear once for this wait_id at `logs/2026-11-26/20261126_170323/dev-loop/events.jsonl` lines 100 and 99; exactly one `wake.queued` (wake-04a89c03a141) was created at 17:14:21.091Z.
- Delivery did not hold (known issue-84c584957e65): codex claimed/launched pid 22576 at 17:14:28.226/28.346Z and exited; `wake.retry` -> opencode at 17:21:54.669Z; opencode launched pid 20612 at 17:35:27.010Z. Queue-to-effective-launch 1265.9s (~21m06s), attempts=2, one queued wake and no duplicate launch.
- Resource sidecar `game-dll-20261126T171310909318Z-faf8c7.resource.json`: 90 samples @1s over 137.172s, mean host CPU 98.674%, peak 100.0%, minimum available memory 121,266,176 B (~115.6 MiB). `runtime_alignment`: same run 20261126_170323 hosted server vehicle_freeplay (pid 33008) and client pid 34624 at 287.88 fps (32 frames over a 4.167ms budget); client mode unverified. CPU saturation is expected from a 4-heavy-TU compile under a live pair; low free memory is a candidate only, not a proven cause.
- Stale-identity defect (known issue-9a4503f6ed68 / issue-7d3dcfd7433b): the wait record carries `build_id 2026-11-26T16:59:26.767416Z` and `generation=702` (last published artifact) with no `run_id`, while the in-flight attempt was gen 703 / `faf8c7`. The correct attempt id was recoverable only from adjacent `dev-loop.game-build.started` lines and the reason revision. This session set `related_build`/`related_run` to the true attempt.
- Reason field was clean here (`[GAME BUILD] revision=... sources=644 jobs=10 priority=high cache=... seeded=0`); the truncated-JSON-tail defect (issue-6e08d89b55e3) did NOT apply to this occurrence.
- Recurrence, not a duplicate issue: two sibling same-titled waits exist in the same window — issue-4fb7a24c54b0 (16:56:24.029Z, gen 698 `32c560`, 151.156s) and issue-45733b82bbbc (17:07:53.638Z, gen 702 `7428d5`, 103.313s). The flat 60s budget is below normal full game-DLL compile time; `build_friction.py` keys uniqueness on `wait_id`, so each build opens a new issue (policy owner issue-9a4503f6ed68 / issue-e044a931c54b).

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the wait ID is the dedupe key, the exact-wait issue was reused, later observations were appended as evidence on the owning defect issues, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/issue context/issue review/issue suggest`, `dev-loop.py --status`, and read-only inspection of `logs/2026-11-26/20261126_170323/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T171310909318Z-faf8c7.{json,resource.json}`, and `.dev/agent-wake/events.jsonl`.
- Review artifact: issue-24863e1f595e review id 485 (verdict `confirmed-legitimate-wait-not-stall; identity-defect-recurred; wake-queued-once-dispatch-late`, state `in_review`, reviewer `build-wait-reviewer`); `related_build`/`related_run` set to the in-flight attempt and run.

# Measured evidence

- Wait elapsed 60.731s vs 60.0s budget; over-budget event written 17:14:28.832Z; attempt started 17:13:10.909Z; terminal success 17:15:28.111Z (generation 703, total 137.203s).
- Phase timings: compile 92.001s, compile_sum 299,191ms across 5 TUs, link 11.984s, changed_source_preflight 90.735s; slowest TU vehicle-system.cpp 90.356s.
- Resource: 90 samples/137.172s, mean host CPU 98.674%, peak 100%, min free 121,266,176 B; live pair server 33008 / client 34624 at 287.88 fps.
- Wake: one queued 17:14:21.091Z; codex launch 17:14:28.346Z then exit; opencode final launch 17:35:27.010Z; 1265.9s queue-to-effective-launch; attempts=2.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget / per-kind budget and per-wait_id duplicate consolidation; reliable <=5 min fallback wake drain; wait-record in-flight attempt/run identity; whether host memory pressure contributed to compile time.

# Agent handoff context

- Wait policy/budget/duplicate owner: issue-9a4503f6ed68 and issue-e044a931c54b; issue-136d4553ff92 for stage accounting. Occurrence/evidence appended.
- Wait-record stale identity owner: issue-7d3dcfd7433b (occurrence appended) plus issue-9a4503f6ed68 acceptance view.
- Wake-delivery owner: issue-84c584957e65 (occurrence appended).
- Resource-pressure owner: issue-2a5c5da53565 already recorded the `faf8c7` sampling gap (missing matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation); this session's issue-136d4553ff92 entry adds the per-TU/phase breakdown and the live-pair contention context.
- Reason-truncation owner: issue-6e08d89b55e3 (not triggered by this occurrence — reason was clean).
- Next exact step: the owners confirm the smallest budget/dedupe/identity checks; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
