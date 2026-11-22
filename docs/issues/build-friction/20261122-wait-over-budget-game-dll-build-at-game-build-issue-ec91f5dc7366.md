# issue-ec91f5dc7366: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-expected-wait-with-reason-defect)

### 2026-11-22T06:51:17.113Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait game-dll-build:2026-11-22T06:29:47.707Z (issue-ec91f5dc7366).

FINDING: The wait is accurate and benign, but the issue's `reason` field is corrupt.

Evidence (matching attempt .dev/build-timings/game-dll-20261122T062947781426Z-8dc1de.json):
- started_utc 2026-11-22T06:29:47.781Z, completed_utc 2026-11-22T06:31:01.919Z, total_seconds 74.127, success true, generation 466, changed src/npc/npc-navigator.cpp.
- Phase durations: changed_source_preflight 24.86s, compile 24.58s (npc-navigator.cpp), link 13.39s, object_cache_scan 5.34s, publish 0.51s.
- So a normal single-source incremental build legitimately runs ~74s, above the 60.0s budget. The build succeeded; it was not stuck or wedged.
- Current generation 468 also took 74.218s, confirming ~74s is the normal cost, so single-source builds can routinely exceed the 60s budget.

Issue-quality defect: evidence.reason is a 240-char tail of a raw worker progress JSON line:
`58788.743,build_attempt_id:game-dll-20261122T062947781426Z-8dc1de,source_generation:0,generation:466,revision:cc46ebef...,progress_write_ms:5.458,record_overhead_ms:6.206}`
Cause: devscripts/dev-loop.py _record_game_build_output (~line 2830) sets game_build_detail = line.rstrip()[-240:] for every non-empty stdout line, including raw `translation_unit.completed` progress JSON; check_wait_budget uses game_build_detail as `reason`. This violates the acceptance requirement that the issue carry a meaningful reason.

Recommendation (code fix to be claimed separately, not done here):
1. Raise WAIT_ISSUE_SECONDS above normal incremental build time (>=90s) or make the budget stage-aware, so expected ~74s builds do not auto-file friction and wake reviewers.
2. Only set game_build_detail from recognized `[GAME_*]`/human-readable lines, or summarize raw progress, so `reason` is meaningful.

No source edits made and no code claimed. This wait does not indicate a failed or wedged build.
