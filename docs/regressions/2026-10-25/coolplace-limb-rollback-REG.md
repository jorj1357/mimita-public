# Coolplace limb overlap rolled the whole actor back

- Recorded: `2026-10-25T00:14:05Z`
- Status: SOURCE FIX BUILT AND LAUNCHED; LIVE FEEL REVIEW PENDING

## Observed

Dashing or walking into certain coolplace geometry looked like catching on
invisible nothingness.

## Cause

The actor triangle solver restored `lastValidWorldCollisionPosition` whenever
residual penetration exceeded `0.1 m`. During a dash, an arm or leg can swing
ahead of the root and overlap a nearby wall while the head/torso remain clear.
The residual was usually `0.10–0.23 m`, so the whole actor was rolled back even
though only a limb was clipping.

The coolplace trace also showed opposing wall/seam contacts on separate
triangles farther apart than the old `0.015 m` seam epsilon.

## Fix

- Opposing face seams and connected edges use a wider contact distance.
- Rollback keeps `0.1 m` when head or torso penetrates, but allows `0.35 m`
  when only limbs overlap.
- Rollback diagnostics record `core_penetration_m` and the applied threshold.

## Evidence

- `logs/2026-10-24/20261024_235013/events.jsonl`: nine
  `collision.last_valid.rollback` events near `(138, -419, 74)`, residual
  `0.10–0.23 m` on `leftArm`/`leftLeg`.
- `logs/2026-10-24/20261024_235447/events.jsonl`: eight rollbacks after the
  seam-distance change.
- Build `3119` compiled and launched in run `20261025_001329`; no post-threshold
  collision capture or human feel acceptance yet.

## Update 2026-10-25T14:25:00Z — full playback and 2x limb tolerance

The limb-only rollback tolerance was raised from `0.35 m` to `0.7 m` after a
full coolplace recording still produced two rollbacks: a `0.39 m` limb/wedge
contact and a `0.52 m` floor/wall corner contact.

Full playback of `scenario-20261025_135108-coolplace1.jsonl` on build `3181`
(run `20261025_142116`) now reports:

- `collision.last_valid.rollback = 0`
- `scenario.playback.snap = 32`
- `scenario.playback.equipment = 9`
- `frames_played = 4728`, `max_position_error_m = 5.77`

The flat-floor back-face response that reflected the dash is tracked separately
in `docs/regressions/2026-10-25/flat-floor-backed-face-dash-REG.md`.
