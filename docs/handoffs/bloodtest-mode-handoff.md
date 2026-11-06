# Bloodtest mode handoff

Snapshot: 2026-11-06 01:38 UTC (2026-11-05 20:38 EST)  
Repository: `C:\mimita-v9`  
Branch: `20261011zt`  
HEAD at handoff: `a992f9a4`

## Goal

Finish Bloodtest as a multiplayer close-quarters sandbox using shared match,
movement, weapon, actor-damage, and destructible-world owners. The user wants
an independent `movement-bloodtest` Source-kernel preset based on Juggernaut
control tuning, physical hybrid body aiming, physically simulated normal-scale
revolver and shotgun, idle sword blade contact, one-tick NPC respawns, one
authoritative weapon set for all actors, and nested skin/meat/bone damage layers
with part detachment and replicated presentation. Traversal across connected
ground blocks must not snag; use the shared collision fix and do not add a
Bloodtest-only exception.

The user confirmed these values and interpretations:

- Movement copies Juggernaut control tuning into an independent preset, with
  both `ground_speed` and `max_speed` capped at `12.0`.
- Each body part has 100 health. Show meat below two-thirds health, bone below
  one-third, and detach a destroyed part only while its actor remains alive.
- NPCs should respawn at the existing one-fixed-tick minimum.
- Players and NPCs must receive the same mode weapon set.

## Implemented so far

- Added `config/gamemodes/bloodtest.json`, online mode entry, Bloodtest weapon
  set 33, `movement-bloodtest.json`, and Bloodtest revolver/shotgun definitions.
- Bloodtest is FFA, enables friendly fire, uses set 33, has a 0.01-second
  respawn delay, enables persistent destructible geometry with reset policy
  `never`, and selects `mimita-duels-map-v3`.
- Bloodtest opts into physical aiming and hybrid body movement, and enables
  blood and ragdoll presentation.
- Movement is a separate Juggernaut-derived Source-kernel config with the
  requested 12-unit speed caps.
- The revolver is configured as one projectile (mass 0.008 kg, speed 260,
  radius 0.004); the shotgun as 15 pellets (mass 0.002 kg each, speed 320,
  radius 0.003). Both viewmodels are 1x and set `uses_physics_projectile` true.
  Network mapping and the server projectile path use the shared projectile
  rifle family; the volley uses deterministic per-pellet directions and sends
  authoritative pellets from the requested muzzle origin.
- The mode weapon IDs use native slots 28 and 29. Keep these slots; the
  previous runtime showed the original colliding slot assignment was wrong.
- Added generic `force_weapon_set_for_all_actors` mode setting, default false,
  and enabled it for Bloodtest. The actor resolver applies the mode set over
  role/preset loadouts and explicit host set choices when this flag is true.
  Other mode configs have the field set false so the mode masterlist stays
  complete and ordered.
- Added Bloodtest mode documentation at
  `docs/specs/gamemodes/bloodtest.md`. It explicitly says biological part
  health/layers/detachment are not implemented.
- Existing Swordsword shared physical contact behavior remains the mode’s
  melee weapon. Confirm its contact config and server path before changing it.
- The shared block-seam fix already exists in
  `src/physics/movement/actor-triangle-solver.cpp`; the regression record says
  source and scenario runtime passed, but human movement feel remains open.
  No Bloodtest-specific seam handling was added.

Relevant changed source/config paths include:

- `src/gamemode/gamemode.h`, `src/gamemode/gamemode.cpp`,
  `src/network/server-gamemode.cpp`
- `src/network/network-weapons.cpp`,
  `src/network/multiplayer-projectiles.cpp`,
  `src/network/server-projectiles.cpp`, `src/combat/weapon-system.cpp`
- `config/gamemodes/bloodtest.json`, `config/gamemodes/masterlist/`,
  `config/gamemodes/*.json`, `config/onlinemodes.json`,
  `config/weaponsets.json`, `config/movement/movement-bloodtest.json`,
  `config/weapons/bloodtest_{revolver,shotgun}/config.json`,
  `devscripts/dev-launch-modes.json`
- `docs/specs/gamemodes/bloodtest.md`

The working tree contains extensive unrelated dirty and untracked user work.
Review `git status --short` before editing. Do not reset, clean, or restore
whole directories. The files listed above were already dirty in several cases;
preserve unrelated hunks within them.

## Evidence and current blocker

Configuration checks after the mode setting migration:

- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `python devscripts/weapon_json_sync.py --check`: PASS.
- `git diff --check`: reported existing trailing whitespace throughout other
  dirty documentation; it did not identify a Bloodtest-specific whitespace
  issue. Re-run scoped checks after changes.
- `config/gamemodes/bloodtest.json` begins with the mode masterlist comment, so
  plain `json.loads` is not the right parser for that file. The gamemode sync
  checker accepted it.
- `config_selftest.py` previously failed with a large list of unrelated avatar
  schema omissions; do not attribute that baseline failure to Bloodtest.

Earlier runtime observation (run directory
`logs/2026-11-06/20261106_004057`) showed mode startup as Bloodtest FFA, weapon
set 33, and a player inventory resolving to Swordsword, Bloodtest Shotgun,
`nothing`, and Bloodtest Revolver. The server accepted revolver attacks and a
shotgun volley; the volley emitted 15 projectile IDs. These observations came
from an older executable and do not prove the latest mode-wide override source
change, close-range hits, body damage, visual effects, sword contact, reset, or
seam feel.

Latest dev-loop snapshot at handoff:

- Run `20261106_012938`, launch profile `[2 CLIENTS] Bloodtest - Free for all`;
  the run is stopped and the dev-loop reports `build_status=failed`.
- Latest successful artifact is source generation 185, build ID
  `2026-11-06T01:13:09.288969Z:4818d3ef83479f49f3d9d90c9456340836a3ed7bc8e6b353636c900f9fc08f7d`.
  It predates the current mode-wide weapon-set precedence change and must not
  be treated as verification of that change.
- The repository-root `mimita.exe` is present but zero bytes. The loop recorded
  `LINK FAILED` on one attempt, then an incremental `Nothing changed` followed
  by publish rejection: `source executable invalid (invalid DOS header or file
  too small)`.
- `source_generation=0` was reported by the loop during failure recovery; inspect
  the current status and daemon/build identity rather than relying on that
  value as a source hash.
- No current server/client run or successful build proves the current source.

Use the existing dev-loop authority only. First run
`python devscripts/dev-loop.py --status --json` and inspect daemon/build/run
state. Reuse or recover its existing daemon through its control path; do not
start a competing build loop, run the root executable, or manually use a stale
artifact as feature evidence. The build step output exposed no actual linker
diagnostic, so identify why the canonical build created a zero-byte output
before retrying. Preserve all unrelated running processes.

## Work still required

1. Finish the shared actor damage owner: authoritative 100-HP-per-part state,
   body-part hit-location routing, ordinary actor-health damage alongside part
   damage, 66/33 layer thresholds, nested progressively smaller skin/meat/bone
   geometry/material presentation, fragment effects, and part detachment at
   zero only when the actor survives.
2. Replicate part health, exposed layer, and detachment for players and NPCs
   through shared actor damage/replication. Reuse one authoritative path; avoid
   a Bloodtest-only damage subsystem. The current `server.h` actor structs and
   `applyActorDamage` are relevant starting owners; packet and client render
   owners still need inspection before choosing the replication shape.
3. Confirm actual weapon `uses_physics_projectile` consumers. The server maps
   Bloodtest guns into the shared projectile family, but a repository search
   found no use beyond the weapon field and JSON parser; verify physical
   simulation directly rather than assuming that flag changes behavior.
4. Once a valid current-source artifact exists, launch mode 51 through the
   dev-loop and inspect that run’s journals. Verify player/NPC identical loadout
   despite role/preset/host overrides, FFA damage, one-tick NPC respawn, actual
   muzzle origin including point-blank contact, 15-pellet count, impact scaling,
   idle sword blade contact, arena persistence/manual reset, and edge traversal.
5. Human visual/feel review remains needed for physical hybrid aiming, gun
   scale, movement handling, blood/wounds/fragments/layers, surviving-part
   detachment, and block-edge traversal.
6. Update `docs/specs/gamemodes/bloodtest.md` and record exactly one final
   changelog when implementation work is complete. Separate source/config,
   build, runtime, and human-acceptance claims.

## Useful existing routes

- `docs/ROUTER.md` and the mode/movement/weapons/destructible-world specs
- `docs/specs/dev-loop/dev-loop.md` for canonical runtime/build evidence
- `docs/specs/visuals/blood-bulletholes-viscera.md` for body-layer intent
- `docs/specs/networking/` and shared contact/damage owner docs for authority
- `docs/regressions/collisions/block-seam-speed-REG-2026-10-31.md` for seam
  evidence and remaining human review

