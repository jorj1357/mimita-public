# Shallow bevel bounce response

- Date: 2026-09-28
- Scope: shallow-angle bevel contacts and low-normal-speed body rebounds
- Status: implemented and deterministic tests pass; live map acceptance remains open

## Change

- Rounded feature normals now retain a portion of the exact world-face normal.
  This prevents a radial bevel normal from becoming nearly tangent and losing
  the impact component after another contact has projected velocity.
- Embedded body contacts at any positive penetration now use the configured
  minimum rebound instead of silently taking the no-bounce projection branch.
- Exact triangle depenetration remains unchanged.

## Validation

- `python build_agent.py`: SUCCESS.
- `mimita.exe --actor-triangle-solve-selftest`: PASS.
- `mimita.exe --moving-crate-selftest`: PASS.
- `mimita.exe --collision-selftest`: PASS.
- Sizing7 large-bevel shallow-angle acceptance remains open.
