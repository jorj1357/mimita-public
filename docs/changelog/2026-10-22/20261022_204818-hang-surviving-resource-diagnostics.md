# Hang-surviving resource diagnostics issue intake

- Task: Record the requested resource-diagnostics gaps and desired coverage.
- Result: PASS_WITH_FOLLOW_UP
- Time: 2026-10-22T20:48:18Z (America/New_York display: 2026-10-22 16:48:18 EDT)
- Branch: `20261011zt`
- HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a` (no commit created)

## Exact change

### `docs/specs/open-gameplay-issues.md`

- Line 120: added a new intake row summarizing the three current instrumentation paths and their gaps. Before, there was no issue covering journal loss on hang, allocation-caller and outstanding-memory attribution, or continuous external process/system-resource monitoring.
- Exact addition: `User wants broader diagnostics so a hang can still be explained after the game stops writing, memory use can be attributed to allocation owners and tracked while retained, and resource use can be monitored externally.`
- The row keeps per-function instrumentation as a design question and requires low-overhead measurement, consistent with the low-resource goal. It links the issue to the existing Juggernaut hang/high-memory report.
- Line 137: added the matching issue-index row.

## Review and validation

- Read `docs/ROUTER.md`, relevant performance rules in `docs/specs/performance/performance.md`, canonical journal/storage guidance in `docs/specs/debug-logging/debug-logging.md`, `docs/specs/dev-loop/dev-loop.md`, and the efficiency, logging, documentation, and time-format focused guidance.
- The current performance spec calls for measured per-thread CPU and critical-path costs before multi-core redesign, and prefers low allocation counts and memory use. The intake preserves this constraint without selecting an implementation.
- Confirmed linked performance, debug-logging, dev-loop, and accessibility documents exist.
- Whole-docs TODO scan found the directly relevant existing note at `docs/specs/debug-logging/debug-logging.md:1`: `2026 10 10 1800 jorj todo - need to make size limit bc we keephaving std alloc memory error`. Suggested expansion: define diagnostic allocation bounds, retained-memory behavior, and the response when the bound is reached. Left unchanged.
- `git diff --check -- docs/specs/open-gameplay-issues.md` passed (Git prints its configured LF-to-CRLF conversion notice).
- Documentation-only issue intake; no code review, build, runtime measurement, or tests were performed.

## Pre-existing work preserved

At the start of this turn, `config/analytics.json`, `docs/specs/dev-loop/dev-loop.md`, `src/engine/engine-tick.cpp`, and `docs/specs/open-gameplay-issues.md` were already modified. Existing untracked changelogs included `20261022_195219-gameplay-issues-juggernaut-diagnostics.md`, `20261022_200038-juggernaut-performance-debug.md`, `20261022_200528-juggernaut-live-fps-diagnosis.md`, `20261022_200838-juggernaut-freeze-connection-report.md`, `20261022_203330-juggernaut-build2846-live-journal.md`, `20261022_203546-juggernaut-hang-memory-networking.md`, and `20261022_204438-build-output-slow-display.md`. They were preserved.
