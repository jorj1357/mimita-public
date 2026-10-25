# Flat-floor dash response and playback position snap

- Time UTC: `2026-10-25T14:26:00Z`
- Display: `2026-10-25 10:26:00 EDT`
- Branch: `20261011zt`
- HEAD: `0472121b` plus the existing uncommitted worktree
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Ran the supplied coolplace recording
`devscripts/scenarios/user/scenario-20261025_135108-coolplace1.jsonl` through
the scenario playback path, added per-tick weapon replay and a 5 m position
snap, and traced the reported "dash on flat ground catches on nothing" to a
back-faced flat floor triangle in the actor-triangle response.

## Playback changes

`src/devtools/scenario-player.{h,cpp}`:

- Each recorded tick now carries `equipped_weapon` and `equipped_slot`.
- Playback re-applies recorded weapon changes through `equipweapon` when the
  result changes, and zeros the raw `slotHeldMask` so combat cannot re-equip a
  different slot.
- Playback compares the live actor position to the next recorded position. If
  drift is `>= 5 m`, it snaps to the next recorded position, restores the
  recorded velocity, updates model transforms, and emits
  `scenario.playback.snap`.
- `scenario.playback.completed` includes `snap_count`.
- `scenario.playback.stopped` is emitted for non-complete stops.

`src/devtools/scenario-commands.cpp`:

- An empty recorded start weapon now equips `nothing` explicitly.

## Collision fix

`src/physics/movement/actor-triangle-solver.cpp`:

- Map walkability now uses `fabs(surfaceNormal.z) > MAX_WALKABLE_SLOPE_DOT`
  and requires the contact normal to point up. A flat floor wound with a
  back-facing authored normal is still support; a ceiling is not.
- Near-feet contacts on walkable map faces always use the oriented authored
  face normal for response before flattening numerically flat floors.
- Limb-only rollback tolerance raised from `0.35 m` to `0.7 m` (2x) so shallow
  limb/wedge overlaps do not roll the whole actor back.

## Root-cause evidence

Before the fix, `logs/2026-10-25/20261025_140919/events.jsonl` tick `12087`
showed a flat floor contact with `surface_normal = [0,0,-1]` and:

```text
response_normal = (-0.8909, 0.3690, -0.2649)
velocity_before = ( 38.10, -40.17, -0.67)
velocity_after  = (-20.34, -15.97, -18.05)
```

The floor was misclassified as non-walkable and the rounded feature normal
reflected the dash sideways/backward. This is a local collision-response bug,
not networking.

## Validation

- Final build: `3181`, published current.
- Full recording playback: run `20261025_142116`, mode `16`, coolplace.
- `scenario.playback.completed`: `frames_played=4728`, `start_restored=true`,
  `max_position_error_m=5.77`, first divergence frame `408` (`1.48 m`).
- `collision.last_valid.rollback = 0`.
- `scenario.playback.snap = 32`.
- `scenario.playback.equipment = 9`.
- The supplied recording itself shows the original catch at frame `405` →
  `406`: velocity drops from about `(28.2, -41.3)` to `(7.47, -23.94)`.
- Collision trace was temporarily enabled for a trimmed 101-frame dash capture
  and restored to `important` afterward.
- No human visual/feel acceptance was performed.

## Regression records

- `docs/regressions/2026-10-25/flat-floor-backed-face-dash-REG.md`
- Updated `docs/regressions/2026-10-25/coolplace-limb-rollback-REG.md`

Related older records: `docs/regressions/2026-09-29/slope-edge-snag-REG.md`
and `docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md`.

## Pre-existing work preserved

Existing uncommitted configuration, collision, networking, GUI, scenario, and
documentation work was preserved. No commit was created.
