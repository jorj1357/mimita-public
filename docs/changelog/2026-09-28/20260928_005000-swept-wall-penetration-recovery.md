# Swept wall penetration recovery

- Date: 2026-09-28
- Scope: high-speed actor movement, thin walls, and multi-contact recovery
- Status: implemented and deterministic tests pass; live acceptance remains open

## Root cause

- Sweep detection could find a triangle crossing, but recovery used only final
  plane penetration. A fast actor could therefore be detected after crossing a
  wall without being moved back far enough.
- The shared correction solver also capped each axis at `0.5`, and the actor
  solver capped the total correction at `2.0`, regardless of tick travel.

## Fix

- Swept contacts now include the normal travel that occurred after their time
  of impact.
- Correction limits scale with the intended movement for that fixed tick while
  retaining the old small-move floor.
- Exact triangle depenetration and rounded feature response remain separate.

## Validation

- `python build_agent.py`: SUCCESS; `Compiled: 3`, `Skipped: 485`.
- `mimita.exe --actor-triangle-solve-selftest`: PASS.
- `mimita.exe --moving-crate-selftest`: PASS.
- `mimita.exe --collision-selftest`: PASS.
- Live extra-fast wall and multi-contact acceptance remains open.
