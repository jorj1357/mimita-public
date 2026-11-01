# Client-first hitscan claims and shotgun aggregate authority

Time created: 2026-11-01T23:04:26Z
Time last updated: 2026-11-01T23:04:26Z

Status: IMPLEMENTED; RUNTIME EXACT-MATCH EVIDENCE CAPTURED

## Goal

Stop the shotgun from showing a predicted damage value and then applying a
different server total. Keep predicted damage feedback, make all hitscan share
one path, and make the server a referee that rejects only impossible outcomes.

## Policy

Confirmed in `docs/specs/networking/contact-authority.md`:

- MiMITA is intentionally client-trusting in the current phase.
- The shooter owns what it saw, including per-target hitscan damage and pellet
  count.
- The server validates target identity, life, spawn generation, reach, world
  occlusion, hit volume, and a theoretical maximum.
- A valid claim is adopted exactly, including when the server re-trace already
  found the same target with a different pellet result.
- Anti-cheat can tighten validation later without changing the ownership model.

## Changed

- `PROTOCOL_VERSION` is now `49`.
- `AttackRequestPacket` carries up to eight `ClaimedHitPacket` entries:
  target id, target spawn generation, claimed damage, claimed pellet hits,
  body part, and hit position.
- Added `ShotClaim`/`MAX_SHOT_CLAIMS` to `RevolverShotResult`.
- Client local hitscan paths record per-target claims:
  - single-ray remote player;
  - single-ray remote NPC;
  - multi-pellet remote player;
  - multi-pellet remote NPC.
- `mpSendAttackRequest` copies claims into the request, pending attack retries,
  and pending hit claims.
- Pending-claim correlation now recognizes every claimed target, not only the
  primary/last target, and uses that target's claimed damage for prediction
  telemetry.
- Server `handleAttackRequest` now:
  - validates every claimed target;
  - accepts existing re-traced targets without replacing their aggregate with
    the old one-pellet fallback;
  - updates existing aggregates with the validated claimed total;
  - creates an aggregate when the server misses but the claim is valid;
  - clamps claimed damage to the configured maximum for the weapon/body
    part/pellet count;
  - clamps claimed pellet hits to the configured pellet count.
- Re-enabled `prediction.predict_damage = true` so predicted numbers remain
  visible while being corrected to the authoritative claimed aggregate.
- Updated the AttackRequest packet size bound to `384` bytes for the bounded
  multi-target claim array.
- Updated `docs/specs/weapons/weapons.md` to allow the bounded hitscan claim
  while keeping projectile and melee attacks intent-only.

## Validation

- Build `191`: success.
- `--actor-final-pose-selftest`: PASS.
- `config/networkingconfig.json` and `config/hitfx.json`: JSON parse OK.
- Live journal `logs/2026-11-01/20261101_230248/events.jsonl` (build `191`):
  - shotgun predicted `75` -> server applied `75`;
  - shotgun predicted `44` -> server applied `44`;
  - revolver predicted `15` -> server applied `15`;
  - no shotgun `net.prediction.damage.mismatch`.
- The same run contained one unrelated rocket-explosion mismatch
  (`150` predicted, `146` actual); hitscan claims were exact.
- `devscripts/quality/check_damage_authority.py` still reports its existing
  broad allowlist findings; this change introduces no new health writer.

## Remaining

- Human visual review of multi-target shotgun feedback is still needed.
- A human should verify the exact claimed number matches the displayed
  aggregate and blood feedback with multiple moving targets.
- Anti-cheat remains deferred by the current client-first policy.
- Rocket explosion prediction still uses its separate path and is not covered
  by this hitscan claim implementation.

## Follow-up build

- Build `192`: success.
- `--actor-final-pose-selftest` on build `192`: PASS.
- The functional runtime evidence remains the build `191` journal above; build
  `192` contains the same code plus documentation/changelog changes.
