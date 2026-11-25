# issue-a2de003a1900 Build-Wait Review

# Task

- Task ID: review build/test wait issue-a2de003a1900 (wake-3145a03d66ba)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.382s, wait_id game-dll-build:2026-11-24T21:58:06.724Z
- Summary: Verified the wait against attempt `game-dll-20261124T215807388728Z-84be9e`, its build-timings and resource summaries, the dev-loop journal, and the wake queue. Appended one review (verdict `pass-with-open-defects`) to issue-a2de003a1900, set its related_build/related_run, and appended occurrences to issue-9a4503f6ed68 (stale identity), issue-84c584957e65 (late/duplicate dispatch and launch_result misattribution), issue-136d4553ff92 (resource gap), and issue-2a5c5da53565 (build-stress sampling). No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-24T22:55:00Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through `agent-coord.py` / `CoordStore.issue_occurrence`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was real and the build succeeded; it was not a stall. Attempt `game-dll-20261124T215807388728Z-84be9e` (Mode 3 request `2026-11-24T21:58:06.362Z-a390fc9e:mode3:game`, revision `d3113668...`, source_generation 27, changed `src/main-systems.cpp` + `src/physics/physical-entity.cpp`) published generation 625 with `status: ok`, `no_op: false`, `total_seconds: 182.618` (compile 67.398s, link 89.348s, object scan 1.73s, publish 0.652s).
- The flat 60s budget fired ~0.4s after the first changed TU completed (`src/main-systems.cpp` 60.486s, `src/physics/physical-entity.cpp` 60.480s) and ~122s before publication. The over-budget signal is expected for this workload, not evidence of a hung build.
- Auto-intake and queue contract held: dev-loop run `20261124_215719_01` journal wrote `dev-loop.wait.over_budget` (L49, created:true) and `dev-loop.wait.reviewer-wake` (L48, queued:true); exactly one wake request `wake-3145a03d66ba` was created at 21:59:08.153Z for this issue.
- Stale-identity defect (known, issue-9a4503f6ed68): wait evidence carries generation=null and build_id `2026-11-24T19:27:02.742428Z` (last completed EXE build), not the in-flight attempt 84be9e / generation 625; related_build/related_run were empty.
- Dispatch did not hold (known, issue-84c584957e65): codex claimed 21:59:09.006Z and launched pid 26300 at 21:59:09.145Z; `wake.retry` "stale launched request recovered by scheduler" 22:01:27.596Z; opencode claimed/launched pid 35280 at 22:07:28.050Z/087Z. attempts=2; queue-to-final-launch 499.9s (~8m20s); two launches for one wait_id (exactly-once queue held, one-launch target did not).
- Launch attribution defect (known): the `dev-loop.wait.reviewer-wake` row at 21:59:11.373Z is stamped with this wait_id/issue but its `wake.launch_result` (2000-char tail of `agent-wake.py run --once`, `build_friction.py:250`) embeds wake-59dc989b28d0 / issue-ee3e906c8273 / log `2026-11-24T21-45-00.685Z` / pid 18976, not this wake.
- Duplicate issue volume (known, issue-9a4503f6ed68): each per-attempt wait_id mints another near-identical "Wait over budget: game-dll-build at game_build" issue; this review added no duplicate.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; every record is append-only.

# Exact implementation changes

- None. Append-only issue records only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: issue observe/show, `.dev/build-timings/game-dll-20261124T215807388728Z-84be9e.json` and `.resource.json`, `logs/2026-11-24/20261124_215719_01/dev-loop/events.jsonl`, `.dev/agent-wake/events.jsonl`, and `requests/wake-3145a03d66ba.json`.
- Review artifact: review id 400 on issue-a2de003a1900 (state `in_review`).

# Measured evidence

- Wait elapsed 60.382s at budget 60.0s; attempt total 182.618s (compile 67.398s, link 89.348s).
- Resource sidecar: 142 samples @1s over 182.614s, mean host CPU 99.984%, peak 100.0%, minimum available memory 12,595,200 B (~12.0 MiB), no sampler errors; `runtime_alignment` unavailable (`fresh_server_and_client_journal_evidence_not_matched`).
- Wake queued exactly once at 21:59:08.153Z; final opencode launch 499.9s after queue; attempts=2.
- Dev-loop wait events: present (over_budget + reviewer-wake).

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether owners will fix stale wait identity, the duplicate-issue policy, launch_result attribution, and the missing matched pre-build resource baseline.

# Agent handoff context

- Wait-policy / identity / duplicate owner: issue-9a4503f6ed68 (owner wake-proof-wait).
- Wake-delivery owner: issue-84c584957e65 (late/duplicate dispatch and launch_result misattribution).
- Resource gap: issue-136d4553ff92; gap_key `build-resource-sampling:game-dll-20261124T215807388728Z-84be9e`; question sent to build-coordinator.
- Build-stress sampling: issue-2a5c5da53565.
- Next exact step: capture a matched pre-build resource baseline plus host commit charge/limit, and choose one duplicate-issue policy; shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
