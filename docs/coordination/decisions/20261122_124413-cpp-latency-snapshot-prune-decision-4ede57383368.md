# Coordination decision: cpp-latency-snapshot-prune

- Decision: `decision-4ede57383368`
- Made by: `cpp-latency-digest-cache`
- Created UTC: `2026-11-22T12:44:13.030Z`
- Conflict: `conflict-28c39cc357c3`
- Inputs: `-`

## Chosen approach

Preserve the currently checked-out game_build.py content as the authoritative base; reconcile conflict-28c39cc357c3 and apply only the leased snapshot-pruning patch.

## Rationale

Cold review of the current working tree and full diff shows the conflict arose from a file hash changing during an earlier edit. The current tree contains needed codegen-cache, worker-metric, and snapshot-timing changes; there is no active lease holder. Preserve every existing hunk and patch against the current file hash acquired by lease-f749a4202004. No alternative historical file copy is available or should overwrite this shared state.

## Tradeoffs

This resolves only the build-driver file-change conflict. The independent game_build_worker.py conflict remains open and untouched. The snapshot cleanup change will be small and validated through the root dev-loop.

## User directives considered

- none recorded

## Rejected alternatives

- none

## Affected paths

- `devscripts/dev_loop/game_build.py`

## Dissent

- none

## User override

Add a new user directive for the affected task. The decision becomes stale
automatically and the task owner records a replacement decision.

