# Collision cleanup passes 1 and 2

Time: `2026-09-29T16:21:00Z`

## Requested scope

Implemented items 1 and 2 from `docs/specs/20260929plan.md` Part 5 only.

## Changes

- Removed the uncalled, allocating `gatherGLBTrianglesForSphere` helper and
  its stale shared-header and translation-unit declarations.
- Removed the dead safety cluster: `applyPostSnapCorrection`, `doGroundSnap`,
  `doRotationSafetyPass`, and `doFinalSafetyPass`.
- Kept `doFloorRecovery` and the remaining legacy GLB pipeline because they
  still have live toggle-off/NPC/fallback consumers. The later migration items
  in Part 5 were not performed.

## Evidence

- Source reference search has no remaining matches for the removed symbols.
- `python build_agent.py`: `Status: SUCCESS`, return code 0.
- Build 0409 `mimita.exe --collision-selftest`: `COLLISION SELFTEST PASS`.
- This is source/build/self-test evidence only; no live gameplay acceptance was
  claimed, and the slope/corner behavior remains a separate runtime issue.
