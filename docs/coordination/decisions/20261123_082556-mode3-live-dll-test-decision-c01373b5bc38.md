# Coordination decision: mode3-live-dll-test

- Decision: `decision-c01373b5bc38`
- Made by: `mode3-live-dll-test`
- Created UTC: `2026-11-23T08:25:56.729Z`
- Conflict: `conflict-83478d747769`
- Inputs: `-`

## Chosen approach

Resolve the 15-file probe's intentional edit-and-restore hash conflict after confirming all normalized source content matches HEAD

## Rationale

The test added only a unique comment marker to src/worldgen/infinite-worldgen-module.cpp, the DLL attempt completed, and git status/diff show no net source change. The coordinator compares raw bytes and recorded the expected CRLF-versus-LF normalization difference on edit commit; there is no concurrent source edit to preserve or merge. The remaining 14 source leases were safely aborted after restore.

## Tradeoffs

Records the test-only lease hash conflict explicitly; source/runtime alignment still requires the queued root-dev-loop restore generation.

## User directives considered

- none recorded

## Rejected alternatives

- none

## Affected paths

- `src/worldgen/infinite-worldgen-module.cpp`

## Dissent

- none

## User override

Add a new user directive for the affected task. The decision becomes stale
automatically and the task owner records a replacement decision.

