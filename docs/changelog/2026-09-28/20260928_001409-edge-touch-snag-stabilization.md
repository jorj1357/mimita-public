# Edge-touch snag stabilization

- Date: 2026-09-28
- Scope: actor triangle contacts at block seams, edges, and vertices
- Status: implemented and self-tested; live badhouse acceptance remains open

## Root cause

- The triangle collector treated any current triangle intersection as a penetrating contact.
- It then replaced zero or near-zero penetration with the full collision skin, so a triangle merely touching a block edge became a blocking depenetration and velocity response.

## Fix

- Added hot-reloadable `edgeTouchTolerance` to `config/collision.json`.
- Current static contacts at or below that real penetration depth are ignored as seam/edge touches.
- Swept crossings and contacts with real penetration still produce normal collision response.
- Kept `collisionSkin` at `0.05` as the separate recovery margin for real contacts.

## Validation

- `mimita-20260928T003000.exe --actor-triangle-solve-selftest`: PASS
- `mimita-20260928T003000.exe --moving-crate-selftest`: PASS
- `mimita-20260928T003000.exe --collision-selftest`: PASS
- Unique executable build/link: PASS

## Runtime tuning

```json
"collisionSkin": 0.05,
"edgeTouchTolerance": 0.002
```

`edgeTouchTolerance` is intentionally small. Increasing it too far could allow real shallow penetration, so it should be tuned only from a captured seam trace.
