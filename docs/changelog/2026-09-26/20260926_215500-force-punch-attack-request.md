# Force Punch attack request and dev-loop room routing

## Outcome

Force Punch now reaches the shared multiplayer `AttackRequest` path. The
client QuickHit start previously returned an empty `RevolverShotResult`, so
the command handler stopped before sending the request to the server. The
server therefore never started its authoritative one-tick physical-contact
shape.

## Change

- `src/combat/weapon-system.cpp` returns a fired marker and aim data after
  starting QuickHit, allowing the existing generic attack sender to run.
- No separate Force Punch damage path was added; the server continues to own
  contact testing, NPC/player damage, and knockback.

## Runtime evidence

- Dev-loop published build `0013`.
- `Server_log_215258.txt` records an NPC damage broadcast with `damage=250`
  and `weapon=10`, followed by the Force Punch kill event.
- The same run used the dev-loop room `KVYPKEQ` and server weapon set 1.

## Room/config note

The dev loop launches its own server and client from the selected profile. A
GUI-created room is a separate server process. Leaving the dev-loop room and
creating a GUI room can leave two MiMITA servers competing for the same UDP
port, making it appear that a code or weapon-set change did not apply.

## Remaining follow-up

The dev loop should eventually track and close server descendants created by
its client so a GUI-created child room cannot remain orphaned across rebuild
restarts. This was observed but not changed in this focused Force Punch fix.
