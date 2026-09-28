# Actor-triangle response Phase 1

Date: 2026-09-27 19:53:39 -04:00 (EST)
Branch: afad20a-rebuild
Commit: 92cb30e7
Status: PASS_WITH_HUMAN_REVIEW

## Scope

Implemented Phase 1 from `docs/specs/20260927plan.md`: retain exact actor
triangle collision while restoring the old non-bounce movement response.

## Source change

`src/physics/movement/actor-triangle-solver.cpp`

`solveActorTriangleCollision` now uses `projectVelocityAgainstNormal` for actor
triangle contacts. Arm, leg, head, and weapon sweep motion no longer becomes a
root-player bounce impulse. Triangle penetration correction, contact detection,
corner handling, support-entity carry, explosions, and weapon forces remain on
their existing boundaries.

The high-speed wall self-test was updated to require removal of normal velocity,
preservation of tangential velocity, and no outward bounce launch.

## Validation

- `python build_agent.py`: `Status: SUCCESS`, compiled 1 file, linked.
- `--actor-triangle-solve-selftest`: PASS.
- `--collision-selftest`: PASS.
- `--moving-crate-selftest`: PASS.
- `git diff --check`: passed; only line-ending warnings were reported.
- Human gameplay acceptance remains: push the arms into a block corner, slide
  along a wall, and confirm no snag or excessive launch.

## Preserved edits

The working tree also contains existing edits in `config/accounts/default.json`,
`config/analytics.json`, `config/collision.json`, and the user-provided
`docs/specs/20260927plan.md`. They were preserved and not reverted.
