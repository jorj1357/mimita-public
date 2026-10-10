# Hybrid weapon-collision wall probe logging

Date: 2026-10-09
Scope: bounded runtime diagnostics and a 60-fixed-tick walk-forward scenario for the hybrid aim-body collision investigation.

## Source changes

- Added a bounded `walkforward_ticks [ticks]` terminal scenario command. It schedules the existing walk-forward action once per fixed simulation tick, with a 1-600 tick limit; the requested probe uses 60 ticks.
- Added a collision-trace marker for each scheduled tick, including fixed tick, actor position/velocity, input, and `aimbody_mode`.
- Extended actor-triangle contact trace records with sweep velocity, actor part label, weapon-contact flag, weapon bounce mode/scale, equipped weapon, aim-body mode, and before/after root velocity.
- Collision trace was enabled only for the probe and restored to `off` afterward in `config/debuglogger.json`.

## Build evidence

- The first rebuild reported an existing workspace ambiguity in `src/network/server-gamemode.cpp` after the dirty map-config changes were visible to the compiler. No unrelated networking source was edited for this probe.
- A subsequent canonical build completed successfully at 09:22:54: 1 translation unit compiled, 548 skipped, executable relinked.
- Fresh executable version identity: `C:\mimita-v9\mimita.exe`, build time `09:22:44`.
- Version-info journal: `logs/10-09-2026/20261009_092457/events.jsonl`.

## Runtime evidence

The corrected probe used the same `funworld3` map for server and client and requested Juggernaut mode.

- Server journal: `logs/10-09-2026/20261009_092316/events.jsonl`.
- Client journal: `logs/10-09-2026/20261009_092319/events.jsonl`.
- The server loaded `funworld3`, accepted player 1, emitted `SERVER MAP READY`, and sent snapshot chunks.
- The client loaded the same map and connected, but remained at `waiting for snapshot id=1`; the startup command therefore never executed.
- Search found zero `scenario.walkforward_tick` records and zero player `collision.contact.*` records in this probe. The run is not valid evidence of hybrid wall physics.
- The server journal did expose a separate runtime issue: its loop ran well below 60 Hz under this Juggernaut setup and logged repeated NPC legacy collision fallbacks. That is a harness/performance limitation for this run, not proof of the weapon-fling cause.

## Current conclusion

The source path still supports the original hypothesis: hybrid physically moves body parts and weapons, and collision response can use the swept part velocity to push root velocity. The new trace will distinguish weapon contacts from body contacts and show the exact velocity transfer once the client snapshot/readiness gate is repaired. This session did not reach that evidence boundary, so no collision fix is claimed and no human gameplay acceptance is claimed.

## Validation still needed

Run the same 60-tick scenario after fixing the client snapshot-readiness gate or using the repository’s supported local multiplayer launch path. Compare `aimbody_mode=hybrid` against `default`, and inspect `sweep_velocity`, `actor_velocity_before`, `actor_velocity_after`, `weapon_contact`, and `weapon_bounce_mode` at the first wall contact.
