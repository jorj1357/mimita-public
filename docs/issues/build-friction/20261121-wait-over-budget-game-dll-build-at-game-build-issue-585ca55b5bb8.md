# issue-585ca55b5bb8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-21T22:27:16.988Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded reviewer activation for wait game-dll-build:2026-11-21T22:14:37.136Z.

Issue completeness: verified. The record carries wait_id, kind=game-dll-build, stage=game_build, reason, elapsed_seconds=60.245, started_utc=2026-11-21T22:14:37.136Z, source_generation=13, generation=433, build_id=2026-11-21T02:26:50.306117Z, request_ids=[auto-13], and a reviewer wake. No fields were missing.

Was it a stall? No. .dev/build-timings/game-dll-20261121T221437598388Z-a59f5e.json shows the same attempt started 2026-11-21T22:14:37.598Z and completed successfully at 22:21:44.414Z: total_seconds=426.708, generation=435, source_generation=13, request auto-13, success=true. Dominant phases: changed_source_preflight=103.109s, compile=124.441s, link=46.324s, publish=38.560s, object_cache_scan=5.871s, worker_total_seconds=282.846, compiled=24/skipped=618 after src/mapeditor/map-editor.h changed. The 60.245s alert fired inside a normal long game-DLL build, not a hang.

Acceptance: met. Dev-loop events.jsonl shows dev-loop.wait.reviewer-wake queued=true adapter=codex wake=wake-608af8e4d069, and dev-loop.wait.over_budget created=true for this wait_id. It was recorded and woken exactly once for this occurrence.

Noise finding: 46 open game-dll-build wait issues now exist (17 new, 29 in_review), all carrying the same title Wait over budget: game-dll-build at game_build. The fixed WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:108) is far below the normal game-DLL build time on this tree (worker_total_seconds 146-283s; total wall 360-427s observed today), so nearly every build creates one issue. Per docs/specs/dev-loop/change-to-evidence-latency.md (lines 122-127) wait_id is the intended dedupe key, so per-occurrence issues are by design and acceptance is still met; the cost is a growing same-titled backlog. The precedent review on issue-76f19e7598c4 already flagged the identical concern. Recommend one consolidated, human-approved follow-up (class-aware/measured budget for game-dll-build and grouping repeated wait occurrences, cross-linked to issue-136d4553ff92) instead of more per-build issues. No code changed here; budget policy changes are human-approved per the spec and were not made.

No source edits, no file leases taken.
