# NPC rocket server context fix

Date: 2026-09-24 16:45:47 EST
Branch: current working branch

## Finding

The supplied run `logs/2026-09-24/20260924_203822/events.jsonl` shows the
authoritative server targeting NPC 1001, reaching `rocket.world_hit`, and
entering `rocket.explosion.before` at server tick 2076. No later server event
appears. The client then reports `server_generation=0`, `server_hash=0`, and
`server=(none)`. The later NPC weapon-selection lines are therefore local
client behavior after the authoritative server is gone; they cannot damage the
player.

## Fix

- `src/network/server-damage-outcome.cpp`: clear the global active server
  context after the synchronous hot damage consequence pass. The function had
  left the global pointer referencing its stack-local `ServerContextV1`, making
  the next hot projectile/lifecycle/network callback use dangling memory.
- `src/network/server-projectiles.cpp`: preserve NPC ownership when constructing
  the generic projectile impact owner entity. NPC rockets were previously
  represented as player-domain entities at this boundary.

## Validation and remaining work

`git diff --check` passed. The exact JSONL evidence was inspected. These are
cold EXE/server bridge files, so the fix cannot activate in the already-running
process through the DLL-only live build; it requires the next canonical
timestamped EXE build/install. The running process was not restarted or
replaced. After installation, acceptance is to spawn an NPC, let its rocket
explode, confirm the server continues ticking, then spawn a second NPC and
confirm it fires revolver/shotgun/rocket behavior and authoritative damage.

No new debug file or unmanaged logging was added; existing structured JSONL
events supplied the diagnosis.

Pre-existing edits were preserved and are not attributed to this change.
