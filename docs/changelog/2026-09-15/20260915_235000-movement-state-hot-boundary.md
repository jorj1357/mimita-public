# Hot movement state boundary

Date: 2026-09-15
Status: implemented phase 1

## Change

Completed the first safe migration seam for hot movement state. The existing
versioned `GameMovementRuntimeStateComponentV1` projection is now readable and
writable through the generic component bridge, and movement intent setup creates
the ECS-owned state once without resetting it on later input updates.

## Why

Movement ability state must survive hot DLL replacement. It cannot live in DLL
globals such as grounded state, dash cooldowns, or input edge latches.

## Evidence

- `python devscripts/run-movement-tests.py`: PASS (18/18, 21/21, 33/33).
- `git diff --check`: PASS.
- No cold movement path was removed in this phase.
- Full hot player/NPC/server movement migration remains pending.
