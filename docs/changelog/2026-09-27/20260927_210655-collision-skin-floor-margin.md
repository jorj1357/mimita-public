# Collision skin and floor recovery margin

- Date: 2026-09-27
- Scope: active actor triangle collision and weapon approximation contacts
- Status: implemented and self-tested; live corner/seam acceptance remains open

## Changes

- Increased the shared actor/world collision skin from `0.02` to `0.05` in `src/physics/config.h`.
- Aligned the slope skin constant with the shared collision skin so these values cannot drift apart.
- The larger margin is used by actor triangle contact generation and broadphase gathering, giving floor contacts more recovery room at the fixed tick.
- Kept weapon-specific skin JSON-controlled; the revolver remains at `0.02` to avoid excessive wall penetration before its bounce response.

## Validation

- `mimita-20260927T204000.exe --actor-triangle-solve-selftest`: PASS
- `mimita-20260927T204000.exe --moving-crate-selftest`: PASS
- `mimita-20260927T204000.exe --collision-selftest`: PASS
- Unique executable build/link: PASS

## Follow-up

- Reproduce the one-time floor fall-through and corner snag in badhouse with collision trace enabled. If the floor miss returns, capture the actor candidate count and final penetration so the missing sweep/recovery step can be fixed directly rather than increasing skin again.
