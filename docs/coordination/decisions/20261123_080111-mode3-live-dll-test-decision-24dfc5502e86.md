# Coordination decision: mode3-live-dll-test

- Decision: `decision-24dfc5502e86`
- Made by: `mode3-live-dll-test`
- Created UTC: `2026-11-23T08:01:11.655Z`
- Conflict: `conflict-50cf42d8a954`
- Inputs: `-`

## Chosen approach

Keep the restored NPC query source as-is and resolve the test-only hash conflict

## Rationale

This was a controlled measurement probe: the comparison order was changed for generation 547 and restored before generation 548. Current working-tree text normalizes exactly to HEAD and git diff is empty; no other agent modified the source. The edit-commit conflict records the intentional edit/revert across the existing lease, not an unresolved concurrent source change.

## Tradeoffs

Preserves exact committed source semantics and keeps the source tree clean; raw line endings may differ because the worktree uses CRLF conversion.

## User directives considered

- none recorded

## Rejected alternatives

- none

## Affected paths

- `src/npc/npc-world-query.cpp`

## Dissent

- none

## User override

Add a new user directive for the affected task. The decision becomes stale
automatically and the task owner records a replacement decision.

