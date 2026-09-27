# SpyKnife impact damage curve

## Request

Make SpyKnife damage respond to impact speed, force, and hit directness: weak
hits should start near 5 damage, strong hits should grow toward a 999 cap, and
the server should decide the authoritative result each 60 Hz contact tick.

## Specification review

- User-visible behavior: a slow, weak, or sideways contact starts at
  `minDamage`; faster, stronger, more direct contacts increase damage; the
  configured `maxDamage` prevents unbounded health changes.
- Authority: the client measures and reports impact ingredients for prediction;
  the server validates the report, re-runs the shared formula, and owns health.
- One owner: `src/combat/spyknife-damage.cpp` now owns the formula used by both
  client prediction and server application.
- Protocol: the contact packet carries speed, force, and directness, and the
  protocol version was incremented from 34 to 35.
- Preserved behavior: backstab damage and knockback keep their existing
  configured override; ordinary hit knockback continues to use the shared
  impact measurements.

## Changes

- Added `src/combat/spyknife-damage.h/.cpp`.
- Added hot configuration values to `config/weapons.json` and matching built-in
  defaults in `src/combat/weapon-data.cpp`.
- Added bounded impact validation and impact details to authoritative server
  logs.

## Evidence

- `config/weapons.json` parsed successfully.
- `python build_agent.py` with the local compiler and GLFW paths: SUCCESS;
  final incremental build compiled 1 file and linked `mimita.exe`.
- `mimita.exe --snapshot-chunk-selftest`: PASS.
- Real two-client knife-hit acceptance was not performed in this session.
