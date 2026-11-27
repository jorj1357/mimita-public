# issue-dea21a9c722d Build-Wait Review

# Task

- Task ID: review build/test wait issue-dea21a9c722d (wake-c6fb7ac32356)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 62.229s, wait_id `game-dll-build:2026-11-27T01:33:03.139Z`
- Summary: Verified the wait against in-flight attempt `game-dll-20261127T013303208483Z-a405dd` (gen 749), its build-timings/resource sidecar, the pinned run 20261127_011757, and the wake queue. Appended review (verdict `confirmed-long-build-not-stall; flat-budget-policy-noise; wait-identity-stale; wake-queued-once-dispatch-late; journal-proof-missing(2of3)`) and set `related_build`/`related_run` on the issue. Appended resource-sampling gaps for a405dd plus brain_check gaps 893a11/96489e/0c4b9e to issue-2a5c5da53565 and asked build-coordinator to confirm the next check. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-27T02:05Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7176d28a

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted config, devscript, source, and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog. Append-only issue records were written through `agent-coord.py issue context/review/suggest`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code or commit without human approval.

# Findings

- The wait was real work, not a stall. Wait `game-dll-build:2026-11-27T01:33:03.139Z` (62.229s vs 60.0s budget, source_generation 5, request `2026-11-27T01:33:02.926Z-418dceaf:mode3:game`) maps to attempt `game-dll-20261127T013303208483Z-a405dd` (gen 749, revision `11e6d92f...`). It started 01:33:03.208Z and SUCCEEDED at 01:37:56.090Z; `total_seconds` 292.891, `worker_total_seconds` 261.797, 5 compiled / 641 skipped of 645 sources.
- Phases: changed_source_preflight 148.140s, compile 82.860s, link 24.031s, object_cache_scan 3.329s, publish 1.063s; jobs=10; compile_sum 281.601s. The 60s crossing (~01:34:05) was inside the 148s map-editor.cpp preflight with ~230s of work left. Slowest TUs: map-editor.cpp 148.043s, server-gamemode.cpp 79.926s, map-authored-geometry.cpp 40.207s, authored-block-rebuild.cpp 10.612s. Only map-editor.cpp was directly changed; the other three recompiled because their dependency files were missing/invalid (object-cache/depfile freshness class, issue-70b27f66e00e / issue-51b425637ea9 / issue-a6890271d414).
- Wait identity is stale (known issue-7d3dcfd7433b): evidence build_id `2026-11-27T00:44:04.497616Z` is the pinned EXE artifact of the active run and generation is null; the true attempt a405dd/gen 749 was recoverable only from the reason revision. The reason field was clean this occurrence, so the reason-truncation defect (issue-6e08d89b55e3) did NOT apply.
- Exactly one reviewer wake: `wake-c6fb7ac32356` queued 01:34:14.395Z (.dev/agent-wake/events.jsonl line 3718). codex claimed/launched pid 4940 01:34:36.479/36.634Z then exited; opencode attempts=2 claimed/launched 01:46:15.829/16.123Z (~12m02s queue-to-working-launch), reproducing issue-84c584957e65. One issue, one wake for this wait_id; no duplicate issue.
- Proof expectation is 2 of 3: the issue record and one queued wake exist, but NO `dev-loop.wait.over_budget`/`dev-loop.wait.reviewer-wake` event is retained for this wait_id in any logs/2026-11-27 dev-loop journal (run 20261127_011757 journal ends 01:20:24Z; daemon restarted 01:49:35Z). This is the known root-dev-loop journal loss (issue-6e2d7b91e7d9 / duplicate issue-0498cbef4ef3).
- Resource sidecar `game-dll-20261127T013303208483Z-a405dd.resource.json`: partial; 214 samples @1s over 292.563s, mean host CPU 99.969%, peak 100%, minimum available memory 644,521,984 B (~615 MiB); sampler thread did not stop before timeout. Same run hosted server team_deathmatch pid 5900 (9 NPCs, ai 568.04ms/30 ticks) and client pid 34092 (~21.69 fps, 22/22 frames over 4.17ms). Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation — pressure observed, NOT proven as cause.
- Recurrence, not a duplicate: 267 same-titled "Wait over budget" issues exist all-time (247 in_review, 19 new, 1 assigned). The flat 60s budget sits below normal heavy-TU game-DLL wall time, so healthy builds mint a new friction issue each time (policy owners issue-7d3dcfd7433b, issue-136d4553ff92, issue-e044a931c54b).

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the wait ID is the dedupe key, the exact-wait issue was reused, later observations were appended as occurrences/gaps on the owning defect issues, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/activity set/issue context/issue review/issue suggest/ask`, and read-only inspection of `.dev/build-timings/game-dll-20261127T013303208483Z-a405dd.{json,resource.json}`, `.dev/build-timings/game-dll-20261127T01{4456,4348,4742}*.resource.json`, `logs/2026-11-27/20261127_011757/dev-loop/events.jsonl`, `.dev/agent-wake/events.jsonl`, and `.dev/state.json`.
- Review artifact: issue-dea21a9c722d review (state in_review, reviewer build-wait-reviewer); `related_build`/`related_run` set to the attempt and run. Resource-gap suggestion appended to issue-2a5c5da53565; one issue-linked question sent to build-coordinator.

# Measured evidence

- Wait elapsed 62.229s vs 60.0s budget; attempt started 01:33:03.208Z; terminal success 01:37:56.090Z (generation 749, total 292.891s).
- Phase timings: changed_source_preflight 148.140s, compile 82.860s, link 24.031s; slowest TU map-editor.cpp 148.043s.
- Resource: 214 samples/292.563s, mean host CPU 99.969%, peak 100%, min free 644,521,984 B; live pair server 5900 / client 34092.
- Wake: one queued 01:34:14.395Z; codex launch 01:34:36.634Z then exit; opencode final launch 01:46:16.123Z; ~12m02s queue-to-working-launch; attempts=2.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: owner decision on flat vs per-class budget / per-wait_id consolidation; <=5 min fallback wake drain; wait-record in-flight attempt/run identity; durable dev-loop wait-event journal under the root daemon; whether map-editor.cpp TU cost or host saturation dominates; whether the three depfile-invalidated recompiles were avoidable.

# Agent handoff context

- Wait policy/budget/duplicate owner: issue-7d3dcfd7433b, issue-136d4553ff92, issue-e044a931c54b.
- Wait-record stale identity owner: issue-7d3dcfd7433b.
- Wake-delivery owner: issue-84c584957e65.
- Journal-retention owner: issue-6e2d7b91e7d9.
- Resource-pressure owner: gap_key `build-resource-sampling:game-dll-20261127T013303208483Z-a405dd` and 893a11/96489e/0c4b9e appended once to issue-2a5c5da53565; build-coordinator asked to confirm the matched pre-build baseline check.
- Reason-truncation owner: issue-6e08d89b55e3 (not triggered — reason was clean).
- Next exact step: owners confirm the smallest budget/dedupe/identity/journal checks; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
