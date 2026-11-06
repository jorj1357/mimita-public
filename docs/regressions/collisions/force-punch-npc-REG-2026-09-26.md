# Force Punch NPC contact damage regression

Time created: 2026-09-26T22:00:00Z
Time last updated: 2026-09-26T22:00:00Z
Status: ATTEMPTED FIX (1)

## Observed behavior

Force Punch displays its sphere but does not damage NPCs. Increasing
`config/weapons.json` `force_punch.custom_params.hitboxRadius` to `3.5` does
not change that result.

## Expected behavior

Force Punch is a one-tick sphere attack. On that fixed simulation tick, every
actor intersecting the sphere—player or NPC—takes the configured damage once.
The default Force Punch damage is 250. The knockback direction is the vector
from the sphere center toward the nearest/intersected part of the target. The
sphere radius is the configured bubble radius around the right-hand attack
center.

## Exact causes found

1. The active community weapon set did not contain `force_punch`. The client
   could equip/render it locally, but `serverCommunityWeaponAllowed()` rejected
   the generic attack request before the server started `QuickHitState`.
2. The generic server physical-contact loop originally iterated players only.
   The client-only QuickHit NPC check was not authoritative for replicated NPCs.
3. Spy Knife works through a separate contact-claim packet with explicit NPC
   validation and NPC damage broadcasting, so it did not prove that generic
   physical contact already targeted all actors.

## Attempted fix (1)

- Added `force_punch` to community weapon set 1 so the server accepts the
  centralized attack request and maps it to logical slot 5/native slot 13.
- Set `activeHitboxTicks` to `1.0` so one attack creates one authoritative
  sphere tick.
- Extended `tickServerPhysicalContactWeapons` to evaluate both players and
  `ServerNpc` entries with the same physical sphere test, JSON radius, damage,
  knockback, and contact interval.
- NPC damage now changes authoritative NPC health/knockback and uses the
  existing reliable NPC damage broadcast and kill-recording paths.
- Kept QuickHit as the temporary behavior owner. A broader server melee/contact
  unification should follow after live acceptance instead of deleting the only
  currently wired local state prematurely.

## Unification direction

The desired ownership is one server physical-contact system with an actor
adapter:

```text
AttackRequest
  -> WeaponDefinition / active shape
  -> actor query: players + NPCs
  -> shared contact result: center, nearest point, normal, distance
  -> shared damage/knockback model
  -> actor-specific authoritative apply + shared event vocabulary
```

Spy Knife's client claim packet should remain only where client-visible swept
geometry is needed; it should eventually call the same actor query and damage
application boundary rather than maintain a second NPC/player damage system.

## Proof boundary

`config/weapons.json` parses, the canonical build linked successfully, and the
source now contains the player/NPC loop. Live acceptance is still required:
the server log must show `ATTACK REQUEST RX`, `QUICK HIT SERVER`, and
`PHYSICAL CONTACT NPC DAMAGE`, followed by NPC health reduction, knockback,
respawn, and killfeed behavior. No human live run has yet proven those events.
