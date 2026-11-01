# Shotgun aggregate feedback, client-first prediction, and damage-scaled numbers

Time created: 2026-11-01T18:44:53Z
Time last updated: 2026-11-01T18:44:53Z

Status: IMPLEMENTED; RUNTIME COMBAT EVIDENCE PENDING

## Goal

1. Remove the shotgun’s many zero damage numbers.
2. Show one aggregate shotgun damage number for the pellets that actually hit.
3. Make client prediction the immediate feedback path and let the server accept
   plausible predicted hits with wide tolerances.
4. Scale damage numbers from small at 1 damage to largest at 200+ damage.
5. Record the 99.99% predicted-damage-versus-actual-damage target.

## Changed

### Shotgun presentation

- `applyPelletBlastEventPacket` no longer spawns a damage number for each
  per-pellet entity hit. Per-pellet blood/impact effects remain.
- The authoritative aggregate `DamageConfirmed`/NPC damage event owns the one
  damage number for each target.
- Shotgun damage remains the sum of pellets that actually hit that target.
- Local shooter uses the existing predicted aggregate path; the server
  confirmation replaces/corrects it.

### Prediction and acceptance

- `config/networkingconfig.json`:
  - `server_authoritative_hits.enabled = false`;
  - `prediction.predict_damage = true`;
  - `prediction.predict_deaths = true`;
  - rewind tolerance `4.0`;
  - claim lag allowance `4.0`;
  - maximum rewind `120` ticks.
- Hitscan contact volume tolerance now uses the wide configured claim-lag
  allowance instead of a hardcoded `0.35` m.
- NPC trace/claim fallback uses the authoritative actor capsule when no
  replicated body samples exist. This keeps random avatars and monsters
  plausible for server acceptance while the client keeps its precise trace.
- Added predicted-vs-actual damage telemetry:
  - `net.prediction.damage.mismatch`;
  - `net.prediction.accuracy` every 60 samples;
  - predicted damage and pellet-hit count are stored with the pending hit
    claim.

### Damage-number scaling

- Extended `DamageNumberConfig` and `config/hitfx.json`:
  - `scaleByDamage`;
  - low/mid/high damage anchors;
  - low/mid/high size multipliers.
- Defaults: `1 -> 0.65`, `100 -> 1.35`, `200+ -> 2.0`.
- Damage-number rendering now uses the per-effect damage scale instead of only
  the fixed start/end scale.
- Healing and percent displays keep their existing presentation.

### Specs

- `docs/specs/networking/contact-authority.md`: 100% ideal, 99.99% required
  predicted-versus-actual damage equality; shotgun aggregate contact rule; wide
  plausible-hit acceptance.
- `docs/specs/weapons/weapons.md`: one aggregate shotgun result per target per
  trigger; one damage number, per-pellet effects allowed.
- `docs/specs/effects/effects.md`: damage-number size tiers and no per-pellet
  zero numbers.

## Validation

- Build `119` succeeded.
- `--actor-final-pose-selftest`: PASS.
- `config/networkingconfig.json` and `config/hitfx.json` parse successfully
  with comments stripped.
- Build 119 launches successfully in dev-loop mode 1.
- `devscripts/config_selftest.py` still fails on pre-existing avatar JSON
  completeness issues unrelated to these changes.

## Remaining

- Live shotgun tests are still required:
  - player shotgun hit on NPC/monster;
  - NPC shotgun hit on player;
  - exactly one aggregate number;
  - no zero numbers;
  - predicted total equals confirmed total.
- Record the observed predicted-versus-actual match rate; the 99.99% value is a
  target, not yet a measured result.
- Human visual review of the 1 / 100 / 200+ damage-number sizes is still needed.

## Follow-up validation — 2026-11-01T18:47:00Z

- Forced rebuild `120` succeeded.
- `--actor-final-pose-selftest`: PASS.
- Build 120 launches successfully.
- `hitfx_reload` succeeded; `hitfx_test` at damage `1`, `100`, and `200`
  completed without a runtime error.
- Final code path now accumulates predicted shotgun NPC damage within a 50 ms
  pellet window so the prediction telemetry compares one aggregate total rather
  than the last pellet only.
