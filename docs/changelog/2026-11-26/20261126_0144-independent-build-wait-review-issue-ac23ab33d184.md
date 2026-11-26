# issue-ac23ab33d184 Build-Wait Review

# Task

- Task ID: review build/test wait issue-ac23ab33d184 (wake-56382fd7e40e)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.564s, wait_id `game-dll-build:2026-11-26T00:55:56.696Z`
- Summary: Verified the wait against attempt `game-dll-20261126T005556909723Z-fa8154`, its build-timings/resource sidecar, the run journal, and the wake queue. Appended one review (verdict `reviewed-real-build-over-budget-known-class-no-duplicate-filed`) plus a terminal occurrence to issue-ac23ab33d184, and set its related_build/related_run. Appended occurrences to issue-84c584957e65 (late fallback dispatch), issue-2a5c5da53565 (fa8154/74b4f3/2455a3/8ca897 resource gaps), issue-6e08d89b55e3 (mid-JSON reason), issue-9a4503f6ed68 (stale wait identity), and issue-136d4553ff92 (wait-policy data point). No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T01:44:43Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted config, devscript, and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records/docs written through `CoordStore.issue_review` / `issue_occurrence` / `issue_context`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- The wait was real work, not a stall. Attempt `game-dll-20261126T005556909723Z-fa8154` (run `20261126_005302`, request `2026-11-26T00:55:56.518Z-e862a38a:mode3:game`) succeeded with `status: ok`, `no_op: false`, generation 662, total 194.141s (compile 157.766s, link 11.203s, 36 compiled / 609 skipped).
- The flat 60s budget fired mid-compile. The wait clock started 00:55:56.696Z; the worker started 00:56:15.676Z (18.98s later after snapshot/codegen); only 6/36 TUs were complete at the 60s mark. The longest TU `src/network/server-gamemode.cpp` started 00:57:04.349Z (about 1s after the wait fired) and took 110.490s, so even that single TU exceeded the budget. Other slow TUs: server-npcs 70.310s, vehicle-system 70.153s, server-packets 61.373s.
- The changed paths were `src/physics/physical-entity.h`, `src/physics/physical-entity.cpp`, and `src/vehicles/vehicle-system.cpp`. `physical-entity.h` invalidated 30 cached TUs; 2 more had missing/invalid dependency files; 36 compiled.
- Auto-intake and exactly-once queue contract held: `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` are present (journal L76-L77); exactly one `wake.queued` was created for `wake-56382fd7e40e` at 00:56:59.578Z.
- Delivery did not hold (known issue-84c584957e65): codex primary launched 00:57:03.213Z and failed `model_not_found` ("The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account"); `wake.retry` occurred 00:58:51.867Z; opencode did not claim/launch until 01:19:47.074/01:19:47.086Z. Queue-to-final-launch 1367.5s (~22m47.5s), attempts=2, one queued wake and no duplicate launch. The fallback was drained 2.86s after the next over-budget wait created issue-347500dfb032 at 01:19:44.214Z, i.e. no persistent daemon was draining the retried request.
- Resource sidecar `game-dll-20261126T005556909723Z-fa8154.resource.json`: `status=captured`, 136 samples @1s over 196.813s, mean host CPU 97.950%, peak 100.0%, minimum available memory 215,298,048 B (~205 MiB), no sampler errors; `runtime_alignment=mode_unverified` (same-run vehicle_freeplay server/client; client mode stale/unreported; client FPS 157.70 with 60/158 frames over budget). CPU saturation is expected from a 10-job 36-TU compile; low free memory is a candidate only, not a proven cause.
- Stale-identity defect (known issue-9a4503f6ed68): the wait record carries `build_id 2026-11-26T00:49:32.788771Z` (last published artifact), `generation=null`, and no `run_id`; the in-flight attempt is recoverable only from the truncated reason tail.
- Truncated-reason defect (known issue-6e08d89b55e3): reason is a mid-JSON tail (`s":57328.0,...`) from `dev-loop.py:3112` `detail = line.rstrip()[-240:]` read at `dev-loop.py:3892`. The sibling `reviewer-wake` `launch_result` (`build_friction.py:250`, `stdout[-2000:]`) is also a mid-JSON/prompt fragment; noted on the same issue for owner scope decision, not filed separately.
- A new sampler-partial gap `build-resource-sampling:game-dll-20261126T013504174400Z-8ca897` appeared during the wake's brain checks and was recorded on issue-2a5c5da53565: 92 samples/120.625s, mean host CPU 98.738%, min available memory 1,227,251,712 B, `sampler background thread did not stop before timeout`, build still succeeded gen 664 in 122.062s. Its separate wait issue is issue-fa0da68ca2b3, which was not reviewed by this wake.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the wait ID is the dedupe key, the exact-wait issue was reused, later observations were appended as occurrences, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/activity set/issue show/ask`, `dev-loop.py --status`, and read-only inspection of `logs/2026-11-26/20261126_005302/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T005556909723Z-fa8154.{json,resource.json}`, `.dev/agent-wake/events.jsonl`, and `.dev/agent-wake/requests/wake-56382fd7e40e.json`.
- Review artifact: issue-ac23ab33d184 review at 01:39:31.912Z; occurrence at 01:39:32.132Z; related_build/related_run set.

# Measured evidence

- Wait elapsed 60.564s vs 60.0s budget; issue created 00:56:57.380Z; worker started 00:56:15.676Z; 6/36 TUs done at the 60s mark; terminal success 00:59:11.437Z (generation 662, total 194.141s).
- Slowest TU 110.490s; 30 TUs invalidated by `physical-entity.h`; 2 missing dependency files.
- Resource: 136 samples/196.813s, mean CPU 97.950%, peak 100%, min free 215,298,048 B, no sampler errors, 3 unavailable fields.
- Wake: one queued at 00:56:59.578Z; codex `model_not_found`; opencode final launch 01:19:47.086Z; 1367.508s queue-to-final-launch; attempts=2.
- Additional gaps: 74b4f3 (112/136.61s, 98.156%, min free 2,584,805,376 B), 2455a3 (166/230.532s, 98.119%, min free 55,664,640 B), 8ca897 (partial, 92/120.625s, 98.738%, min free 1,227,251,712 B).

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget/dedupe policy; matched pre-build resource baseline; reliable <=5 min fallback wake drain; wait-record in-flight attempt/run identity.

# Agent handoff context

- Wait policy/duplicate/identity owner: issue-136d4553ff92 (build-coordinator) and issue-9a4503f6ed68 (wake-proof-wait); a bounded question was sent to build-coordinator.
- Wake-delivery owner: issue-84c584957e65 (unowned; occurrence appended).
- Resource sampling owner: issue-2a5c5da53565; gap keys fa8154, 74b4f3, 2455a3, and 8ca897 recorded; bounded question sent to root-resource-loop.
- Reason truncation owner: issue-6e08d89b55e3 (unowned; occurrence appended).
- Not reviewed by this wake: issue-347500dfb032 (01:18 wait) and issue-fa0da68ca2b3 (01:35 wait); they need their own reviewer wakes.
- Next exact step: let the owners confirm the smallest wait-budget/sampler/wake-delivery checks; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
