# Force Punch actor targeting

Date: 2026-09-26
Status: PASS_WITH_HUMAN_REVIEW

## Finding

Force Punch was not an NPC damage path. Its generic `AttackRequestPacket` could
start the server QuickHit state, but `tickServerPhysicalContactWeapons` iterated
only `ServerPlayer` targets. The old local QuickHit NPC check was client-side and
was not authoritative for replicated NPCs. Spy Knife appeared to work because it
has a separate client contact-claim packet and explicit server NPC handling.

## Fix

The generic physical-contact owner now evaluates both player and NPC targets.
NPCs use the same sphere/capsule test, configured radius, force damage, force
knockback, damage interval, and contact episode state. NPC health and knockback
are changed server-side; the existing reliable `broadcastNpcDamageEvent` path
and `serverGamemodeRecordKill` path are used for presentation and scoring.
`PhysicalContactEpisode` now records whether its target is an NPC so player
confirmation flushing cannot treat an NPC episode as a player.

This keeps `QuickHit` temporarily as the behavior owner. Deleting it or merging
it into a broader attack component remains a follow-up after player and NPC live
acceptance; deleting it now would remove the only existing local sphere state
and make diagnosis harder.

## Validation

- `config/weapons.json` parsed successfully with `force_punch.hitboxRadius` at
  `3.5`.
- `python build_agent.py` completed `Status: SUCCESS`; two changed translation
  units were compiled and the executable was linked.
- `git diff --check` passed for the touched implementation/config files.
- `--snapshot-chunk-selftest` returned exit code 1 without output, so it is not
  counted as passing evidence.
- Live two-client/NPC acceptance remains required: confirm a Force Punch attack
  produces `[ATTACK REQUEST SEND]`, `[QUICK HIT SERVER]`, and
  `[PHYSICAL CONTACT NPC DAMAGE]`, then verify NPC health, knockback, respawn,
  killfeed, and no duplicate client-only damage.
