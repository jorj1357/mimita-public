# Lightweight test tiers and runtime scenario queue

## Changes

- Linked the user's follow-up to plan issue `issue-dc2f873290a7` and clarified the process-reuse approach in its discussion and `reduce-test-process-churn` task method.
- Proposed a sequential queue built on existing `--scenario-file` definitions, grouped by pinned build/map/mode/weapon-set, reusing a matching server/client runtime and keeping each scenario's event assertions and canonical journals.
- Separated evidence tiers: source review for documentation-only changes; static/config checks and narrow pure tests for isolated logic; focused scenario journals for narrow behavior; unscripted live play for general gameplay; and multi-client matrices only for network behaviors that need them.

## Evidence

- `devscripts/dev-loop.py --scenario-file` already runs a stable-ID scenario on a matching live runtime, or requests a launch if none matches. It records `scenario.started`, `scenario.step`, `scenario.assert`, and `scenario.finished` in the dev-loop journal, while server and client events remain in their canonical journals.
- `devscripts/dev-loop-matrix.py` provides a one-server/multiple-client matrix, but each client is a separate process. This is reserved for cases that need multiplayer evidence.
- No game or runtime processes were started or stopped, and no test suite was run. The queue controller itself is a proposed follow-up, not an existing feature.
