# issue-3385f308560f Build-Wait Review

# Task

- Task ID: review build/test wait issue-3385f308560f (wake-14d62006319a)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.773s, wait_id `game-dll-build:2026-11-26T06:17:59.474Z`
- Summary: Verified the wait against attempt `game-dll-20261126T061759578653Z-67ec90`, its build-timings/resource sidecar, the run journal, and the wake queue. Appended one review (verdict `reviewed-false-positive-progressing-build-record-defects`), set related_build/related_run, appended occurrences to issue-7d3dcfd7433b, issue-2a5c5da53565, and issue-84c584957e65, and sent one bounded question to launch-latency-investigation. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T15:04Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted edits (config, devscripts, src, docs, shaders). This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through `CoordStore.issue_review` / `issue_context` / `issue_suggest` plus one coordination question.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- The wait was real work, not a stall. Attempt `game-dll-20261126T061759578653Z-67ec90` succeeded: generation 688, revision `eee512f0058f4458ee4d80b6e1448740fea5b093c5d2ee4ffd764faea2cf33c2`, changed `src/combat/explosion-fx.cpp` + `src/combat/weapon-viewmodel.cpp`, total 107.875s, completed 06:19:47.463Z; 3 compiled / 642 skipped of 644, jobs 10.
- The flat 60s budget fired mid-build. Crossing ~06:18:59.47Z was inside `changed_source_preflight` (06:18:28.240Z→06:19:14.806Z) while both heavy TUs were still compiling: `weapon-viewmodel.cpp` 46.407s, `explosion-fx.cpp` 41.993s. Then object_cache_scan, a 3.263s module-identity compile, `link.lld_linker` 06:19:18.292Z→06:19:45.544Z (27.203s), and publish. Work advanced continuously; the 60.773s figure is the first-crossing sample, actual wall time 107.875s.
- Cause of crossing: the flat 60s `WAIT_ISSUE_SECONDS` is below the natural 2-TU + 644-object link cost on this host (preflight 46.563s + compile 3.39s + link 27.266s). This is a wait-policy false positive, not a stuck worker.
- Auto-intake held: exactly one `dev-loop.wait.over_budget` (created=true) and one `dev-loop.wait.reviewer-wake` (wake-14d62006319a, adapter codex) for this wait_id, both stamped 06:19:05.640Z in `logs/2026-11-26/20261126_061338/dev-loop/events.jsonl`. Wake log `2026-11-26T06-19-02.266Z-build-wait-reviewer.log`; queue-to-launch ~3s this time, no duplicate launch observed.
- Record defects reproduced: (1) build_id `2026-11-26T06:10:41.962780Z` is the prior EXE artifact, not the in-flight attempt (dev-loop.py:~3896 `latest_build`); (2) generation=null vs in-flight 688 (:~3897); (3) `fields.wake.launch_result` still contains the full reviewer prompt text instead of a bounded launch result (correctly attributed this time, not the prior misattribution). Positive data point: `reason` was readable (`[GAME BUILD] revision=... sources=644 jobs=10 ... seeded=0`); the `[GAME_FILE]` JSON-tail corruption did NOT reproduce here.
- Resource sidecar `game-dll-20261126T061759578653Z-67ec90.resource.json`: status captured, 75 samples @1s over 107.86s, mean host CPU 98.63%, peak 100.0%, minimum available memory 378,179,584 B (~360 MiB), sampler stopped cleanly; runtime_alignment run 20261126_061412 (server bloodtest pid 10544 gen 687; client pid 28860 ~199.8 fps); unavailable fields matched_prebuild_resource_baseline and host_commit_charge_and_limit. Pressure recorded, not proven as the wait cause.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the exact wait is recorded, the class recurrence was appended to the umbrella tracker, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records, one coordination question, and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/issue context/issue review/issue suggest/ask`, and read-only inspection of `logs/2026-11-26/20261126_061338/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T061759578653Z-67ec90.{json,jsonl,resource.json}`, and the issue docs.
- Review artifact: issue-3385f308560f review id 476 at 15:02:39.180Z; occurrences appended to issue-7d3dcfd7433b (15:03:00.714Z), issue-2a5c5da53565, issue-84c584957e65 (15:03:32.085Z); related_build/related_run set.

# Measured evidence

- Wait elapsed 60.773s vs 60.0s budget; build succeeded at 107.875s (generation 688); preflight 46.563s, compile 3.39s, link 27.266s; 3 compiled / 642 skipped; jobs 10.
- Resource: 75 samples/107.86s, mean host CPU 98.63%, peak 100%, min free 378,179,584 B; unmatched baseline/commit-charge unavailable.
- Wake: one queued and one reviewer-wake for this wait_id; queue-to-launch ~3s.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget/dedupe (issue-7d3dcfd7433b); launch_result pollution and fallback drain (issue-84c584957e65); matched pre-build resource baseline and host commit charge/limit (issue-2a5c5da53565); wait-record in-flight attempt/generation identity.

# Agent handoff context

- Wait policy/duplicate/identity owner: issue-7d3dcfd7433b (owner launch-latency-investigation); occurrence appended and one bounded question sent (thread-13d816330bb2).
- Wake-delivery owner: issue-84c584957e65 (occurrence appended; correct attribution noted).
- Resource sampling: gap_key `build-resource-sampling:game-dll-20261126T061759578653Z-67ec90` appended to issue-2a5c5da53565.
- Next exact step: owner/human confirms the smallest wait-budget/dedup/identity and wake-record checks; shared dev-loop changes need human approval.
- Do not redo: this attempt's evidence collection. Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
