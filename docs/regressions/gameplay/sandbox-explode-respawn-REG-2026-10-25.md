# Sandbox RESET repeated the explode death forever

- Recorded: `2026-10-25T00:14:05Z`
- Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED

## Observed

Pause-menu RESET in Sandbox caused continuous resetting instead of one death
and one respawn.

## Cause

`explode` set `ctx.awaitingExplodeDeath`, which was cleared only by a local
snapshot with `health <= 0`. The server processed the request before its tick,
respawned in the same tick (`respawn_seconds` 0.01 with `SERVER_DT` 1/60), and
sent the snapshot after the respawn. No health-0 snapshot existed, so the client
retried explode every 200 ms and killed each new life.

Sandbox also allowed the local `DeathSystem` to respawn while connected, giving
the client a second life owner.

## Fix

- Clear `awaitingExplodeDeath` when an authoritative spawn generation advances.
- Clear it when the authoritative spawn packet is applied.
- Stop local `DeathSystem` respawn while `MP_CONTEXT.active`.

## Evidence

Run `logs/2026-10-24/20261024_234208/events.jsonl` recorded two spawn
publications (initial + one reset), one `client.local.spawn_applied` with
`dead_before=true`, and zero explode retries.
