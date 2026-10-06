# Ragdoll per-death presentation regression — Attempt 1

Date: 2026-10-06  
Status: Attempt 1 implemented; live gameplay confirmation pending.

## Bug

Ragdolls do not appear reliably for every actor death. The failure is most
visible in Counter-Strike rounds and can affect teammates, enemies, and the
local player. Blood and other death-adjacent effects may still appear. Dead
actors can also continue producing the walking effect at their death position.

## Ideal behavior

For every authoritative actor life:

1. The server applies lethal damage and records one alive-to-dead transition.
2. Each client receives or observes that transition, with actor identity,
   life/spawn generation, health, position, and tick available for diagnosis.
3. Exactly one corpse presentation is requested for that actor life.
4. Prediction, a reliable death packet, and a snapshot transition may compete,
   but they must deduplicate rather than suppress the only presentation.
5. When the actor respawns, every death-presentation latch is reset before the
   new life can die.
6. The local player follows the same corpse-presentation rule as remote
   players and NPCs.
7. A corpse request is separately traceable through attempt, spawn, renderer
   submission, updates, rejection, and eviction. A missing visual must not be
   described as a renderer problem when no request was made.
8. Walking/idle presentation must stop while the actor is dead.

## Live evidence used

Source journal:

`C:\mimita-v9\logs\10-06-2026\20261006_145207\events.jsonl`

The journal was approximately 14.3 MB and remained readable as JSONL. It
contains multiple client/server processes and restarted runs, so process ID,
executable path, and run ID must be used when correlating a single match.

Important counts from the file:

- `server.damage.applied`: 74
- `server.death.transition`: 12
- `client.npc.death.received`: 13
- `client.npc.death.presentation_skipped`: 3
- `ragdoll.corpse.spawn.attempt`: 14
- `ragdoll.corpse.spawned`: 14
- `ragdoll.corpse.rejected`: 0
- `ragdoll.corpse.evicted`: 0

The decisive sequence was a reliable NPC death that spawned normally on its
first life, followed by a later death for the same client-side replica where
`network_death_presented_before=true`. That later event was skipped with
reason `network_death_presented_already_true`, and no NPC presentation reset
was recorded between the two lives. Examples included later deaths around
server ticks 3691, 3820, and 3974. This rules against a global ragdoll
disable, corpse-cap eviction, or corpse-request rejection for this occurrence.

The file also shows a separate local-player gap: the authoritative local
damage-confirmation path updated health and `dead`, but did not request a
corpse. This explains why the local player's own ragdoll could be absent even
when remote corpse creation was working.

## Attempt 1 change

### Remote/NPC life reset

`src/network/multiplayer-interpolation.cpp` now clears
`player.networkDeathPresented` in the fallback `dead -> alive` health-reset
path. It emits:

- `client.death.presentation.health_reset`
- `client.death.presentation.health_reset.complete`

This closes the exact stale-latch path evidenced in the journal when the
primary epoch reset is missed.

### Local player death

`src/network/multiplayer-projectiles.cpp` now handles a lethal authoritative
local-player damage confirmation by logging `client.local.death.applied` and,
when the per-life presentation latch is clear, requesting one corpse through
`RagdollModeSystem::spawnCorpse`. Duplicate local death confirmations emit
`client.local.death.presentation_skipped` instead.

## Validation evidence

- Source/build: `C:\mimita-v9\.dev\builds\1561\mimita.exe`
- Build: SUCCESS; the edited sources compiled and linked.
- Identity probe: `PROCESS_ROLE=client`, run ID `20261006_150036`.
- Fresh journal path: `C:\mimita-v9\logs\10-06-2026\20261006_150036\events.jsonl`
- Tests: none added or run, by request; this fix requires live gameplay
  evidence.
- Human acceptance: pending. The next run must include at least one death,
  one respawn, and another death for the same actor, with the fresh journal
  checked for reset-before-second-death and one corpse spawn per life.

## If Attempt 1 still fails

The next escalation is to give every actor life an explicit presentation key
such as `(actor_id, spawn_generation)` and make the corpse ledger own the
deduplication. That would remove reliance on a boolean latch entirely, but it
is intentionally deferred until this smaller fix is tested against a fresh
live executable.
