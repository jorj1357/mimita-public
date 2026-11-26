# issue-fa0da68ca2b3 Build-Wait Review

# Task

- Task ID: review build/test wait issue-fa0da68ca2b3 (wake-793dc730ceba)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.738s, wait_id `game-dll-build:2026-11-26T01:35:04.053Z`
- Summary: Verified the wait against attempt `game-dll-20261126T013504174400Z-8ca897`, its build-timings/resource sidecar, the run journal, and the wake queue. Appended one review (verdict `reviewed-wait-accepted-known-class-no-duplicate-wake-late-dispatch-recorded-elsewhere`), set related_build/related_run, and added occurrences to issue-7d3dcfd7433b and issue-84c584957e65. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T02:32Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through `CoordStore.issue_review` / `issue_context` / `issue_suggest`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- The wait was real work, not a stall. Attempt `game-dll-20261126T013504174400Z-8ca897` (run `20261126_013130`) succeeded: `status: ok`, `no_op: false`, generation 664, total 116.234s (worker 94.203, compile 78.718s, link 13.156s, 2 compiled / 643 skipped of 644, jobs 10).
- The flat 60s budget fired mid-compile. The wait started 01:35:04.053Z and crossed at ~01:36:04.79Z, while the only changed TU `src/vehicles/vehicle-system.cpp` was still compiling; that TU started 01:35:28.062Z and completed 01:36:43.968Z (75.902s). `changed_source_preflight` was 75.984s.
- Compiler ftime for that TU: parsing 32.47s (45%), template instantiation 19.58s (27%), opt+generate 26.66s (37%); the crossing is genuine compile cost, not I/O or a wedge.
- One build note was present (rocket-league-landing, note_id 6dce3b95c11b) for the single changed file.
- Auto-intake and exactly-once queue contract held: `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` are present at 01:36:12.011Z; the issue was created (`created=true`) and exactly one `wake.queued` exists for wake-793dc730ceba (01:36:10.215Z); exactly one request file references this issue. The reviewer-wake launch_result was correctly attributed this time.
- Delivery did not hold (known issue-84c584957e65): codex claimed/launched 01:36:11.920/11.938Z (pid 10284) but wrote no review; `wake.retry` "stale launched request recovered by scheduler" at 02:26:48.876Z; opencode claimed/launched 02:26:50.693/50.742Z (pid 20656, this session). Queue-to-final-launch 3040.5s (~50m40.5s), attempts=2, two wake.launched rows for one wait_id.
- Resource sidecar `game-dll-20261126T013504174400Z-8ca897.resource.json`: `status=partial`, 92 samples/120.625s, mean host CPU 98.738%, peak 100%, minimum available memory 1,227,251,712 B, error "sampler background thread did not stop before timeout". The gap_key `build-resource-sampling:game-dll-20261126T013504174400Z-8ca897` was already recorded on issue-2a5c5da53565 by a prior wake; not duplicated.
- Stale-identity defect (known issue-7d3dcfd7433b): the wait record carries `build_id 2026-11-26T01:00:35.913746Z` (last published artifact) and `generation=null`, no `run_id`; the waited-on attempt is recoverable only from build-timings.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the exact wait is recorded, the class-level recurrence was appended to the umbrella tracker, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/issue review/issue suggest/issue context/activity set/end`, and read-only inspection of `logs/2026-11-26/20261126_013130/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T013504174400Z-8ca897.{json,resource.json}`, `.dev/agent-wake/events.jsonl`, and `.dev/agent-wake/requests/wake-793dc730ceba.json`.
- Review artifact: issue-fa0da68ca2b3 review at 02:32:23Z; occurrence appended to issue-7d3dcfd7433b and issue-84c584957e65; related_build/related_run set.

# Measured evidence

- Wait elapsed 60.738s vs 60.0s budget; issue created 01:36:07.933Z; terminal success 01:37:06.235Z (generation 664, total 116.234s).
- Slowest/only changed TU 75.902s; 2 compiled / 643 skipped; changed_source_preflight 75.984s.
- Resource: 92 samples/120.625s, mean CPU 98.738%, peak 100%, min free 1,227,251,712 B, partial sampler stop timeout.
- Wake: one queued at 01:36:10.215Z; codex pid 10284 launched 01:36:11.938Z; opencode pid 20656 final launch 02:26:50.742Z; 3040.5s queue-to-final-launch; attempts=2.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget/dedupe policy (issue-7d3dcfd7433b); reliable <=5 min fallback wake drain (issue-84c584957e65); matched pre-build resource baseline and sampler clean shutdown (issue-2a5c5da53565); wait-record in-flight attempt/run identity.

# Agent handoff context

- Wait policy/duplicate/identity owner: issue-7d3dcfd7433b (owner launch-latency-investigation); recurrence appended, no new proposal.
- Wake-delivery owner: issue-84c584957e65 (unowned; occurrence appended).
- Resource sampling: gap_key 8ca897 already on issue-2a5c5da53565; not duplicated.
- Next exact step: let the owners confirm the smallest wait-budget/dedupe and sampler/wake-delivery checks; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
