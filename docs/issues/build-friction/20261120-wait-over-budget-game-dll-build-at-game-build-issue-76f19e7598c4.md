# issue-76f19e7598c4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-20T21:29:07.443Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded reviewer activation for wait game-dll-build:2026-11-20T20:47:34.055Z.

Issue completeness: verified. The record carries wait_id, kind, stage, reason, elapsed_seconds=60.702, started_utc, source_generation=40, generation=413, build_id=2026-11-20T20:20:51.788061Z, request_ids=[auto-40], and a reviewer wake.

Was it a stall? No. .dev/build-timings/game-dll-20261120T204734114577Z-82e79e.json shows the same build started 2026-11-20T20:47:34.114Z and completed successfully at 20:56:59.129Z: total_seconds=565.001, generation=414, source_generation=40, request auto-40, success=true. Dominant phase object_scan_seconds=318.344 (scan_parse_ms=307016.39), then compile_seconds=114.931, link_seconds=48.722, publish_seconds=28.107, jobs=12, source_count=642. The 60.702s wait therefore fired inside a normal full game-DLL build, not a hang.

Noise finding: the 60s budget is far below the normal build time on this tree, so each cold-ish build creates a same-titled issue. Four near-identical issues exist today: issue-76f19e7598c4, issue-0322fa45b0e8, issue-41a872c08642, issue-f66e6e82656b. build_friction.py keys uniqueness on wait_id, so per-occurrence issues are intentional and the requirement is met; the cost is repeated identical titles. Suggested follow-up (no code changed here): make the wait budget stage/build-aware or group repeated over-budget waits by stage so readers can distinguish a real stall from the expected cost of a 565s cold build.

Wake acceptance: the wake was queued exactly once (wake-f54a24f3d0fa, queued 20:48:36). The codex adapter was claimed first and exited, then agent-wake retried opencode at 20:59:54 and launched at 21:18:39, so delivery took about 30 minutes. The delivered opener was only "You were woken by MiMITA agent-wake. This is a bounded test activation." while the request/prompt files contained this review task; that truncation is tracked separately as issue-62ab3daa615d.

No source edits and no file leases were taken. Reproduction, expected behavior, acceptance, paths, and proof expectation were already present.

## Agent suggestion

### 2026-11-20T21:32:02.131Z — build-wait-reviewer

Structured attribution for this wait points at the previously published artifact, not the in-flight attempt: issue evidence build_id=2026-11-20T20:20:51.788061Z and generation=413, while .dev/build-timings/game-dll-20261120T204734114577Z-82e79e.json shows the waited-on attempt is generation=414 (source_generation=40, request auto-40, started 20:47:34.114Z, completed 20:56:59.129Z). The correct attempt identity appears only in the free-text reason. Also, .dev/state.json points the dev-loop journal at logs/2026-11-20/20261120_170259/dev-loop/events.jsonl, but that file and directory are absent (run dir contains only run.json, server-01, client-01), so the wait.over_budget event could not be read. Suggested owner fix: have the wait-issue writer read active_build_* from .dev/state.json (or pass the active attempt id) so build_id/generation identify the in-flight build.
