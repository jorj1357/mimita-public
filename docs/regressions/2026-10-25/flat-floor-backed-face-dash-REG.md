# Flat-floor back-face made a straight dash reflect

- Recorded: `2026-10-25T14:25:00Z`
- Time last updated: `2026-10-31T11:50:24Z`
- Status: FIXED IN SOURCE; LIVE PLAYBACK HAS ZERO ROLLBACKS; HUMAN FEEL REVIEW PENDING
- Occurrence 2: SUSPECTED RECURRENCE (2026-10-31); cause unconfirmed

## Observed

Dashing straight on a flat coolplace surface looked like catching on nothing.
The actor lost most of its forward speed and was pushed sideways/backward even
though the contact was the flat floor.

## Cause

The contact trace showed a flat floor triangle (`3455`) whose authored surface
normal was back-facing (`surface_normal = [0, 0, -1]`) while the actual support
contact normal was up (`normal = [0, 0, 1]`).

The response path classified map walkability with
`surfaceNormal.z > MAX_WALKABLE_SLOPE_DOT`, so the back-faced flat floor was
treated as **not walkable**. It then used the rounded feature response normal:

```text
response_normal = (-0.8909, 0.3690, -0.2649)
velocity_before = ( 38.10, -40.17, -0.67)
velocity_after  = (-20.34, -15.97, -18.05)
```

That reflected the dash's horizontal velocity. This was a local actor-triangle
response bug, not networking.

## Fix

- Map walkability now uses `fabs(surfaceNormal.z)` and requires the contact
  normal to point up (`normal.z > 0`), so a back-faced flat floor is still
  support while a ceiling is not.
- Near-feet contacts on walkable map faces always use the oriented authored
  face normal for response, then flatten only numerically flat floors to
  world-up.
- The limb-only rollback tolerance was raised from `0.35 m` to `0.7 m` (2x) so
  a shallow limb/wedge overlap does not roll the whole actor back.

## Evidence

- Before fix: `logs/2026-10-25/20261025_140919/events.jsonl`, tick `12087`,
  `collision.solve.summary` with the response normal and velocity reversal
  above.
- After fix: full coolplace recording `scenario-20261025_135108-coolplace1.jsonl`
  in run `20261025_142116`, build `3181`:
  - `collision.last_valid.rollback = 0`
  - `scenario.playback.snap = 32`
  - `scenario.playback.equipment = 9`
  - `scenario.playback.completed` with `frames_played=4728`,
    `start_restored=true`, `max_position_error_m=5.77`
- The recording itself shows the original catch: frame `405` dash, then frame
  `406` velocity drops from about `(28.2, -41.3)` to `(7.47, -23.94)`.

## Related regression records

- `docs/regressions/2026-09-29/slope-edge-snag-REG.md`
- `docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md`
- `docs/regressions/2026-10-25/coolplace-limb-rollback-REG.md`

---

## Regression Occurrence 2 — 2026-10-31

Time: `2026-10-31T11:50:24Z` (human report)

Status: SUSPECTED RECURRENCE; cause not yet confirmed.

The player reports that dashing on a flat surface can still send movement off to
the side instead of continuing straight. This is the same observed symptom as
Occurrence 1, but the current report does not yet include a complete recording
with `client_collision`, so it is not confirmed to share the same response-normal
defect.

The current implementation still contains two different walkable-face rules:
some paths use `surfaceNormal.z`, while the near-feet response override uses
`fabs(surfaceNormal.z)`. A rounded feature normal can still win if the selected
contact is not recognized as a near-feet walkable face. This is recorded as a
candidate, not a confirmed cause.

Next evidence: a short dash recording on the exact flat seam, with the selected
response normal, surface normal, triangle id, contact kind, and before/after
velocity. Do not mark this occurrence fixed from a deterministic self-test alone.
