# issue-a114980aee90 Build-Wait Review

# Task

- Task ID: review build/test wait issue-a114980aee90 (wake-fc2eaee53560)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.623s, wait_id `game-dll-build:2026-11-26T06:07:55.877Z`
- Summary: Verified the wait against attempt `game-dll-20261126T060755948198Z-ddc45d`, its build-timings/resource sidecar, the run journal, and the wake queue. Appended one review (verdict `reviewed-false-positive-wait-record-defects-persist`), set related_build/related_run, appended occurrences to issue-7d3dcfd7433b, issue-84c584957e65, and issue-2a5c5da53565, and sent one bounded review message to launch-latency-investigation. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-26T06:24Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through `CoordStore.issue_review` / `issue_context` / `issue_suggest` plus one coordination message.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- The wait was real work, not a stall. Attempt `game-dll-20261126T060755948198Z-ddc45d` (run `20261126_055157`) succeeded: generation 687, source_generation 32, total 88.921s (worker 82.328s), compile phase 74438ms, object_scan 3203ms, link 3281ms, publish 782ms, 36 compiled / 609 skipped of 644, jobs 10.
- The flat 60s budget fired mid-compile. At the crossing (~06:08:56.5Z) translation units were completing: physical-entity-replication 06:08:51.273Z, map-authored-geometry 06:08:53.895Z, constraint-debug 06:08:55.908Z, multiplayer-reconcile 06:08:56.713Z, constraint-system 06:08:57.091Z, main-systems 06:08:57.472Z, community-match-client 06:08:57.831Z.
- Cause of crossing: changed headers `src/physics/physical-entity.h` (fanout 31), `src/vehicles/vehicle-system.h` (15), `src/vehicles/vehicle-types.h` (17) legitimately rebuilt 36 TUs; the compile phase alone was 74.4s (slowest TU `src/mapeditor/map-editor.cpp` 41.415s, compile_sum_ms 678991.967). This is a wait-policy false positive.
- Record defects reproduced (same classes as the issue-75e49f67b48c review): (1) reason is a raw `[GAME_FILE]` JSON tail `59641.0,"build_attempt_id":...` from dev-loop.py:3112/3142 feeding :3892; (2) build_id is the last EXE artifact `2026-11-26T06:00:42.933273Z` (:3896 `self.latest_build`), not the in-flight attempt; (3) generation=686 (previous published) vs in-flight 687 because `self.game_build_generation` is only assigned at completion (:3259, :3897).
- Auto-intake held once but delivery did not: exactly one issue was created and exactly one `wake.queued` exists (wake-fc2eaee53560, 06:08:58.473Z). codex primary claimed/launched 06:09:01.728/01.753Z (pid 4732) and produced no review; `wake.retry` "stale launched request recovered by scheduler" 06:13:17.754Z; opencode claimed/launched 06:13:18.758/18.819Z (pid 18876). Queue-to-final-launch 260.3s (~4m20.3s), attempts=2.
- Duplicate cluster: the same run also created the immediately preceding wait `game-dll-build:2026-11-26T06:00:44.737Z` -> issue-70a739edc6ae (attempt `game-dll-20261126T060044815851Z-9b7ffb`, 419.093s). Per-timestamp wait_id keeps spawning near-duplicate issues.
- Resource sidecar `game-dll-20261126T060755948198Z-ddc45d.resource.json`: status captured, 67 samples @1s over 88.906s, mean host CPU 96.442%, peak 100.0%, minimum available memory 34,197,504 B (~32.6 MiB), jobs 10; `runtime_alignment` unavailable/mode_unverified; unavailable fields matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics. Gap_key recorded on issue-2a5c5da53565; not treated as the wait cause.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the exact wait is recorded, the class recurrence was appended to the umbrella tracker, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records, one coordination message, and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/issue context/issue review/issue suggest/send/activity set`, and read-only inspection of `logs/2026-11-26/20261126_055157/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261126T060755948198Z-ddc45d.{json,jsonl,resource.json}`, and `.dev/agent-wake/events.jsonl`.
- Review artifact: issue-a114980aee90 review id 473 at 06:23:15.734Z; occurrences appended to issue-7d3dcfd7433b, issue-84c584957e65, issue-2a5c5da53565; related_build/related_run set.

# Measured evidence

- Wait elapsed 60.623s vs 60.0s budget; build succeeded at 88.921s (generation 687); compile phase 74.438s; 36 compiled / 609 skipped; jobs 10.
- Resource: 67 samples/88.906s, mean host CPU 96.442%, peak 100%, min free 34,197,504 B; unmatched baseline/commit-charge unavailable.
- Wake: one queued at 06:08:58.473Z; codex pid 4732 launched 06:09:01.753Z; opencode pid 18876 final launch 06:13:18.819Z; 260.3s queue-to-final-launch; attempts=2.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on the flat 60s budget/dedupe (issue-7d3dcfd7433b); reliable primary-wake completion and <=5 min fallback drain (issue-84c584957e65); matched pre-build resource baseline and host commit charge/limit (issue-2a5c5da53565); wait-record in-flight attempt/generation identity.

# Agent handoff context

- Wait policy/duplicate/identity owner: issue-7d3dcfd7433b (owner launch-latency-investigation); recurrence appended, one message sent, no new proposal beyond prior findings.
- Wake-delivery owner: issue-84c584957e65 (unowned; occurrence appended).
- Resource sampling: gap_key `build-resource-sampling:game-dll-20261126T060755948198Z-ddc45d` (plus 9b7ffb, ee15db) appended to issue-2a5c5da53565; not duplicated.
- Next exact step: owner/human confirms the smallest wait-budget/dedup/identity and wake-delivery checks; shared dev-loop changes need human approval.
- Do not redo: this attempt's evidence collection. Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
