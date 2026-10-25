# NPC Waves kept the player dead at the death position

- Recorded: `2026-10-25T00:14:05Z`
- Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED

## Observed

In NPC Waves, the player died, appeared to float at the death position while
NPCs kept shooting, and only spawned later.

## Cause

The authoritative spawn packet did not clear client death state. Clearance
depended on `mpReconcileLocalPlayer` seeing a positive health and an advanced
spawn generation, or a health 0-to-positive transition. The 0.01 s respawn often
skipped the health-0 snapshot. Separately, `PendingVictimHealth` applied a
lethal result with no life generation guard, so a delayed hit could re-kill a
freshly spawned life. Local `DeathSystem` also respawned in connected Sandbox.

## Fix

- The authoritative spawn apply clears death, presentation, respawn timer, and
  pending victim damage.
- Pending victim damage carries its spawn generation and is dropped when it is
  stale.
- Local `DeathSystem` respawn is disabled while a networked context is active.
- `serverConsumeNpcWaveLife` is idempotent for an actor already marked Dead.

## Evidence

Run `logs/2026-10-24/20261024_234355/events.jsonl` recorded
`client.local.spawn_applied` with `dead_before=true`, then a grounded, alive
snapshot. Multi-life human acceptance is still recommended.
