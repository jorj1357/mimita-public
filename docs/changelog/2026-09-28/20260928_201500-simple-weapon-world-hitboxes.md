# Simple weapon world hitboxes restored

- Date: 2026-09-28
- Scope: active player weapon-to-world collision
- Status: source and build verified; live weapon playtest remains pending

## Change

- Reconnected the existing `source: "capsule"` weapon collision mode to the
  active actor-triangle movement solver.
- Configured capsules are sampled into the existing simple weapon sphere
  contact path, matching the pre-triangle hitbox behavior.
- Weapon render triangles remain fallback-only and are not used as the normal
  weapon world collider.
- All existing weapon definitions in `config/weaponcollisions.json` therefore
  use their configured simple capsule hitboxes against world geometry.

## Validation

- `python build_agent.py`: SUCCESS; canonical `mimita.exe` relinked.
- `mimita.exe --actor-triangle-solve-selftest`: PASS.
- `mimita.exe --collision-selftest`: PASS.
- `git diff --check` passed for the changed source file.
- Human weapon-to-wall playtest: pending.
