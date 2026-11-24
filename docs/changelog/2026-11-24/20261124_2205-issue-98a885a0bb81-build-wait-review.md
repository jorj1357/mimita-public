# issue-98a885a0bb81 Build-Wait Review

# Task

- Task ID: review build/test wait issue-98a885a0bb81 (wake-a61c8e5d2cf2)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 61.186s, wait_id game-dll-build:2026-11-24T20:55:04.680Z
- Summary: Verified the wait against attempt `game-dll-20261124T205504793823Z-0c153b`, its build-timings and resource summaries, and the dev-loop/agent-wake journals. Appended one review (verdict `reviewed-proof-complete-followups-routed`) to issue-98a885a0bb81, an evidence-backed suggestion to issue-9a4503f6ed68 (wait-evidence fields), an evidence note to issue-136d4553ff92 (single-TU resource pressure), and an occurrence to issue-84c584957e65 (late reviewer wake). No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-24T22:05:00Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through the coordination API.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was real, not a stall. Attempt `game-dll-20261124T205504793823Z-0c153b` (request `2026-11-24T20:55:02.711Z-af52fb88:mode3:game`; changed `src/main-systems.cpp`) succeeded: generation 620, revision `c75a32f3648a7054da36750dc0bbf92e23f59c945244d7e82ad2c290474b341b`, total 285.82s.
- Phase split: `changed_source_preflight` 202.55s (slowest TU `src/main-systems.cpp` 202.42s), compile 5.47s (module identity provider), link 34.28s, publish ~0.73s. The 60s threshold was crossed during the genuine single-TU compile, not a hang.
- Host was saturated: 144 samples @1s over 285.8s, mean host CPU 99.977%, peak 100.0%, minimum available memory 32,190,464 B (~30.7 MiB).
- Same-TU contrast: attempt `game-dll-20261124T201013477657Z-77e222` compiled in 8.35s with 4,234 MB free. This matches the gen618 149.6s / 166 MiB pattern already reported by hot-reload-latency-root (thread-0e4585258fe7, msg-199a0d906a19). Classification: resource-pressure / slow single-TU compile, not a wait-policy false positive.
- Issue contract: wait_id, stage, reason, elapsed, budget, started_utc, request_ids, generation/source_generation are present; readiness ready with no missing fields. Exactly one `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` exist for this wait_id, so one issue per wait occurrence held.
- PROOF MET 3/3 (unlike the 20:24 occurrence): both events are retained in `logs/2026-11-24/20261124_205541_01/dev-loop/events.jsonl` (lines 12-13, at 2026-11-24T20:56:34.129Z/20:56:34.144Z), alongside the issue record (20:56:06.063Z) and the wake request (20:56:07.426Z).
- Evidence nuance (routed): issue `evidence.build_id` is the latest EXE artifact `2026-11-24T19:27:02.742428Z`, and `generation` is 618 (last published; `dev-loop.py:3259` advances it only on completion) while the in-flight attempt produced generation 620. run_id (20261124_205541_01), the game-DLL attempt id, and the journal path are absent from the issue. Suggested smallest fix appended to issue-9a4503f6ed68.
- Wake queue contract held; dispatch did not: one `wake.queued` (20:56:07.432Z); codex claimed/launched 20:56:33-34Z; `wake.retry` adapter=opencode 21:13:39.208Z; opencode claimed/launched 21:14:31.937Z; attempts=2; queue-to-final-launch ~1104.5s (~18m24s). Recorded as an occurrence on issue-84c584957e65.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; every record is append-only.

# Exact implementation changes

- None. Append-only issue records only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: issue observe/brief, `.dev/build-timings/game-dll-20261124T205504793823Z-0c153b.json` and `.resource.json`, sibling build-timings generations 609-623, `logs/2026-11-24/20261124_205541_01/dev-loop/events.jsonl`, `.dev/agent-wake/events.jsonl` and `requests/wake-a61c8e5d2cf2.json`, and wait-policy source (`devscripts/dev-loop.py:3839-3931`, `devscripts/build_friction.py`).
- Review artifact: review on issue-98a885a0bb81 (state `in_review`).

# Measured evidence

- Wait elapsed 61.186s at budget 60.0s; attempt total 285.82s (preflight 202.55s, link 34.28s).
- Host pressure: mean CPU 99.977%, peak 100.0%, minimum available memory 32,190,464 B (~30.7 MiB).
- Dev-loop wait event: present (run 20261124_205541_01), exactly one over_budget + one reviewer-wake.
- Wake queued exactly once at 20:56:07.432Z; final opencode launch ~1104.5s after queue; attempts=2.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether owners will add run_id/attempt id/journal path to wait evidence, restore bounded wake dispatch, and reduce single-TU build latency under host memory pressure.

# Agent handoff context

- Wait-evidence field owner: issue-9a4503f6ed68 (suggestion added).
- Build-latency/resource root: issue-136d4553ff92 (owner build-coordinator; evidence added).
- Wake-delivery owner: issue-84c584957e65 (occurrence added).
- Next exact step: for one-file builds, compare a matched no-build host baseline against a build with exactly one verified game pair (compiler CPU-seconds, hard faults/pagefile, wall time) before changing jobs or the 60s budget. Shared wait/build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
