# issue-266ae06d18d0: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (confirmed-expected-build-duration)

### 2026-11-22T20:28:04.597Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence exe-build:4:2026-11-22T19:51:16.116Z.

FINDING (high confidence): False-positive build-friction. The wait was a normal exe build still compiling, not a stall or failure.

EVIDENCE
- Issue evidence (dev-loop): kind=exe-build, stage=compiling, elapsed=60.778s at budget 60.0s, reason 'compiling 0/45 [CXX] src/network/server-ice.cpp', source_generation=4, started_utc=2026-11-22T19:51:16.116Z.
- The build that owned this wait succeeded: .dev/state.json latest_successful_build=2026-11-22T19:53:22.116562Z, source_generation=4, build_status=published_current, last_build_result=success.
- Duration evidence (same build): last_build_duration_seconds=127.559s; setup=0.443s; build_script=105.84s; post_build_processing=21.26s; artifact_publish=1.63s. Build output tail: Compiled 1, Skipped 86, Time 101.14 sec.
- dev-loop --status: lifecycle=build_succeeded, latest_build_id=2026-11-22T19:53:22.116562Z:131d88c3..., active_build_elapsed_seconds=127.559s.
- Coordination causality: coord.issue.opened seq 8335 by dev-loop-wait at 2026-11-22T19:52:16.958Z; reviewer wake delivered to build-wait-reviewer.

ROOT CAUSE: WAIT_ISSUE_SECONDS is a flat 60s budget (devscripts/dev-loop.py:107-110) applied to every wait kind/stage, including exe-build. A normal exe build here runs ~101-128s, so crossing 60s mid-compile is expected and recurs on nearly every exe build. check_wait_budget (devscripts/dev-loop.py:3486-3577) files one issue per wait_id without checking whether the build subsequently succeeds, which is why many near-identical exe-build and game-dll-build wait issues exist.

ASSESSMENT: The wait itself is correctly reported (one issue, one wake), but the policy design causes avoidable noise. No build defect is implied.

SUGGESTION (separate fix, not applied): make the budget stage/kind-aware, or defer/suppress the issue when the build completes successfully within a grace window, so one slow-but-successful build does not spawn an issue per occurrence. Requires human approval and its own claimed task before any code change.
