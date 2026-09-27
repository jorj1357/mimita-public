# Force Punch NPC damage: confirmed gold behavior

- Status: CONFIRMED GOLD BEHAVIOR
- Weapon: `force_punch` / native slot 13
- Attack shape: one-tick sphere, configured radius `3.5`
- Damage: `250` per accepted contact
- Targets: authoritative NPCs and players through the shared physical-contact
  server path

## What was fixed

The local QuickHit animation used to return an empty fire result. That made
the multiplayer command path stop before sending the shared `AttackRequest`.
The server therefore never started the authoritative Force Punch contact
shape. QuickHit now returns a fired marker and aim data after starting, so the
existing generic attack request reaches the server.

## Runtime proof

The dev-loop build `0013` server log records repeated successful NPC contacts:

`logs/09-26-2026/Server_log_215258.txt:37179`

records `NPC DAMAGE BROADCAST ... damage=250 ... killed=1 weapon=10`.

The same event sequence records the weapon identity and authoritative kill:

`logs/09-26-2026/Server_log_215258.txt:37197-37207`

records `weapon="Force Punch"`, `weaponId="force_punch"`, and the gamemode
kill event. The later repeated entries at lines 51215, 53663, 64959, and
112504 show the behavior is repeatable, not a one-off startup artifact.

## Architecture lesson

The visual sphere is only presentation. The server receives the shared attack
request, starts the one-tick physical shape, queries actor targets, and owns
damage/knockback. Weapon-set selection can permit or deny the request, but it
cannot make damage happen when the client never sends the request.

## Human-facing behavior

When Force Punch is equipped and fired, an NPC inside the sphere can receive
250 damage on that tick and can be killed. The same authoritative contact
owner is prepared for player targets.
