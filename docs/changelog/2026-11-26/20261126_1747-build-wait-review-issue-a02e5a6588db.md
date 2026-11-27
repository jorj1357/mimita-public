# issue-a02e5a6588db Build-Wait Review

# Task

- Task ID: review build/test wait issue-a02e5a6588db (wake-35c5c0492a2a)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 61.28s, wait_id `game-dll-build:2026-11-26T17:20:46.773Z`
- Summary: Verified the wait against in-flight attempt `game-dll-20261126T172047133775Z-30f81f` (gen 704), its build-timings/resource sidecar, the run journal, and the wake queue. Appended review (verdict `confirmed-legitimate-wait-not-stall; single-TU-exceeds-budget; identity-defect-recurred; wake-queued-once-dispatch-late`) and set `related_build`/`related_run` on issue-a02e5a6588db. Appended 30f81f occurrences to issue-7d3dcfd7433b (stale wait-record identity) and issue-84c584957e65 (late fallback dispatch). The 30f81f resource gap was already recorded on issue-2a5c5da53565 and was not re-reported. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T17:47Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7176d28a

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted config, devscript, source, and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog. Append-only issue records were written through `agent-coord.py issue context/review/suggest`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code or commit without human approval.

# Findings

- The wait was real work, not a stall. Wait `game-dll-build:2026-11-26T17:20:46.773Z` (61.28s vs 60.0s budget, source_generation 12, request `2026-11-26T17:20:46.345Z-9c69e020:mode3:game`) maps to attempt `game-dll-20261126T172047133775Z-30f81f` (gen 704, revision `da4661b3...`, run 20261126_170323). It started 17:20:47.133Z and SUCCEEDED at 17:22:41.672Z; `total_seconds` 114.547, `worker_total_seconds` 89.828.
- Phases: changed_source_preflight 73.328s, compile 2.297s, link 11.578s, coherence_validation 0.547s, publish 0.531s; 644 sources, 642 skipped, 3 compiled, jobs=10. Two directly changed TUs compiled in the preflight wave: `src/engine/engine-tick-ui.cpp` 73.084s and `src/render/postfx-commands.cpp` 46.342s (plus identity provider 2.149s). A single TU (engine-tick-ui.cpp) exceeded the 60s budget by itself, then an 11.578s link followed. The build finished ~46s after the wait was flagged.
- Exactly-once contract held: `dev-loop.wait.over_budget` (created=true) and `dev-loop.wait.reviewer-wake` (queued=true) appear once for this wait_id at `logs/2026-11-26/20261126_170323/dev-loop/events.jsonl` lines 124 and 123; exactly one `wake.queued` (wake-35c5c0492a2a) was created at 17:21:51.344Z.
- Delivery did not hold (known issue-84c584957e65): codex claimed/launched pid 3516 at 17:21:55.154/17:21:55.294Z and exited; `wake.retry` -> opencode at 17:35:27.028Z; opencode launched pid 33192 at 17:42:01.859Z. Queue-to-effective-launch ~1210.5s (~20m10s), attempts=2, one queued wake.
- Resource sidecar `game-dll-20261126T172047133775Z-30f81f.resource.json`: 81 samples @1s over 114.484s, mean host CPU 98.271%, peak 100.0%, minimum available memory 1,920,593,920 B (~1.79 GiB); same run hosted server vehicle_freeplay (pid 33008) and client pid 34624. Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation. This gap_key was already recorded on issue-2a5c5da53565; no pressure inference.
- Stale-identity defect (known issue-7d3dcfd7433b): the wait record carries `build_id 2026-11-26T16:59:26.767416Z` (last published EXE artifact) and `generation=703` (last published game generation) with no `run_id`, while the in-flight attempt was gen 704 / `30f81f`. The correct attempt was recoverable only from adjacent `dev-loop.game-build.started` lines and the reason revision. This session set `related_build`/`related_run` to the true attempt.
- Reason field was clean here (`[GAME BUILD] revision=... sources=644 jobs=10 priority=high cache=... seeded=0`); the truncated-JSON-tail defect (issue-6e08d89b55e3) did NOT apply.
- Recurrence, not a duplicate issue: distinct wait_ids in this window, all compile-bound and all published successfully — issue-404bd5b44b6e (16:49:17.979, 60.023s), issue-4fb7a24c54b0 (16:56:24.029, 60.924s), issue-45733b82bbbc (17:07:53.638, 60.263s), issue-24863e1f595e (17:13:10.456, 60.731s), this issue (17:20:46.773, 61.28s). Further same-titled wait issues were claimable at review time (ca900e60ed4b, 4c12dde1d7ba, ad873eff97f3, dc6792df3815, 194c538d7cbc). The flat 60s budget is below normal heavy-TU game-DLL build time, so healthy builds mint a new friction issue each time (policy owner issue-9a4503f6ed68 / issue-e044a931c54b).

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the wait ID is the dedupe key, the exact-wait issue was reused, later observations were appended as occurrences on the owning defect issues, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/activity set/issue context/issue review/issue suggest`, and read-only inspection of `logs/2026-11-26/20261126_170323/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T172047133775Z-30f81f.{json,resource.json}`, and `.dev/agent-wake/events.jsonl`.
- Review artifact: issue-a02e5a6588db review (verdict `confirmed-legitimate-wait-not-stall; single-TU-exceeds-budget; identity-defect-recurred; wake-queued-once-dispatch-late`, state `in_review`, reviewer `build-wait-reviewer`); `related_build`/`related_run` set to the attempt and run.

# Measured evidence

- Wait elapsed 61.28s vs 60.0s budget; over-budget event written 17:21:55.483Z; attempt started 17:20:47.133Z; terminal success 17:22:41.672Z (generation 704, total 114.547s).
- Phase timings: changed_source_preflight 73.328s, compile 2.297s, link 11.578s; slowest TU engine-tick-ui.cpp 73.084s, postfx-commands.cpp 46.342s.
- Resource: 81 samples/114.484s, mean host CPU 98.271%, peak 100%, min free 1,920,593,920 B; live pair server 33008 / client 34624.
- Wake: one queued 17:21:51.344Z; codex launch 17:21:55.294Z then exit; opencode final launch 17:42:01.859Z; ~1210.5s queue-to-effective-launch; attempts=2.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget / per-class budget and per-wait_id duplicate consolidation; reliable <=5 min fallback wake drain; wait-record in-flight attempt/run identity; whether host CPU saturation or engine-tick-ui.cpp include cost dominates.

# Agent handoff context

- Wait policy/budget/duplicate owner: issue-9a4503f6ed68 and issue-e044a931c54b; issue-136d4553ff92 for stage accounting. Occurrence/evidence appended via review.
- Wait-record stale identity owner: issue-7d3dcfd7433b (30f81f occurrence appended).
- Wake-delivery owner: issue-84c584957e65 (30f81f occurrence appended).
- Resource-pressure owner: issue-2a5c5da53565 already holds gap_key `build-resource-sampling:game-dll-20261126T172047133775Z-30f81f`; not re-reported.
- Reason-truncation owner: issue-6e08d89b55e3 (not triggered by this occurrence — reason was clean).
- Next exact step: the owners confirm the smallest budget/dedupe/identity checks; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
