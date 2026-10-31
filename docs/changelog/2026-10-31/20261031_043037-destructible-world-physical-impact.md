# Destructible world physical impact and all-map TDM/Juggernaut slice

Date: `2026-10-31T04:30:37Z`

## Summary

Imported-map destruction now runs through one persistent world-impact entry
point for hitscan, projectile, melee, and explosion sources. Closed authored map
solids become shared destructible physical entities on first impact, all map
rotation entries use the same rules, and TDM/Juggernaut restore the authored map
at their match/round boundary.

The legacy weapon-facing `cut_energy_scale` and `cut_radius_scale` arcade
fields were deleted. New tuning uses physical mass/density/size/speed/
penetration values resolved from shared weapon master profiles.

## Specifications updated

- `docs/specs/destructible-world/destructible-world.md`
  - appended section 71: imported map destruction, universal damage sources,
    permanent single entry point, accuracy-first physical model, weapon master
    profiles, explosion radial impacts, materials/assignment, replication,
    reset, 240 FPS/logging rules, acceptance, actor-destruction hook.
- `docs/specs/gamemodes/gamemodes.md`
  - added `features.destructible_world_reset` and global/mode inheritance.
- `docs/specs/gamemodes/teamdeathmatch.md`
  - added destructible map lifecycle and match-boundary reset.
- `docs/specs/gamemodes/juggernaut.md`
  - added destructible map lifecycle and round-boundary reset.
- `docs/specs/gamemodes/survive-the-disasters.md`
  - recorded match-boundary reset with damage preserved across its ten rounds.
- `docs/specs/weapons/weapons.md`
  - added shared world-impact/explosion profiles and physical tuning examples.
  - replaced the explosion TODO with a pointer to the shared owners.
- `docs/architecture/json-configuration/json-configuration.md`
  - documented weapon master world-impact defaults/profiles and inheritance.
- `docs/specs/networking/networking.md`
  - added world destruction authority, ordered causes, latency, reset.
- `docs/specs/networking/contact-authority.md`
  - added actor-damage vs world-destruction scope boundary.
- `docs/specs/entity-editor/entity-editor.md`
  - documented GLB material assignment, overrides, and bulk rules.
- `docs/specs/visuals/blood-bulletholes-viscera.md`
  - recorded future actor body holes/organs as a separate slice.
- `docs/ROUTER.md`
  - replaced the missing `docs/doc-review-09-03-2026.md` references with the
    real documentation checker route and added a destructible-world route.

## Implementation

- Added exact map-solid identity to `ImpactEvent` (`worldSolidId`) and
  `serverRaycastWorld`; hitscan/melee/projectile adapters now pass the exact
  imported solid instead of relying on a chunk-cell guess.
- Hitscan now raycasts each pellet independently for its own world hit/angle.
- Added `HeadlessGLBObject` AABB/material data and a bounded radial explosion
  owner with distance falloff, occlusion, and `explosionMaxNodesPerEvent`.
- Added `world.enabled` global config and gamemode overrides
  `features.destructible_world` / `features.destructible_world_reset`.
- Generalized match reset to the mode reset policy; map changes clear active
  map destruction.
- Added per-node `material` overrides in `config/maps/<mapId>.json`
  (`glb_objects[].material`); default remains concrete with horizontal ground
  slabs as foundation.
- Added diagnostics:
  - `world.destructible-impact`
  - `world.destructible-cut`
  - `world.destructible-activation`
  - `world.destructible-rejected`
  - `world.destructible-explosion`
  - `world.destructible-reset`
  - `world.destructible-budget`
- Cached rejected open solids so repeated hits do not spam the journal.
- Bumped NPC collision render revision so navigation rebuilds after cuts.

## Deleted / migrated

Deleted legacy fields and their runtime paths:

- `cut_energy_scale`
- `cut_radius_scale`
- `WeaponDefinition::cutEnergyScale`
- `WeaponDefinition::cutRadiusScale`
- `ImpactEvent::cutScale`
- `ImpactEvent::sizeScale`

Migration:

- `config/weapons/masterlist/weapon-masterlist.json` now defines
  `world_impact_defaults`, `world_impact_profiles`, `world_impact`, and
  `world_impact_profile`.
- All 34 weapon configs were synchronized with the masterlist.
- Key profiles were assigned: pistol/rifle bullets, buckshot, projectile rifle,
  heavy rocket, light melee, heavy melee (mace/force punch/HAFS).
- `weapon_json_sync.py --check`: `masterlist=1 definitions=34 errors=0`.
- `gamemode_json_sync.py --check`: PASS.
- Baseline before deletion: commit `6dc0814c`
  (`baseline before physical-impact destruction migration`).

## Evidence

### Source and ownership

- Destruction entry point: `ImpactSystem::submit`.
- Map owner: `src/network/server-world-destruction.cpp`.
- Map material overrides: `config/maps/<mapId>.json` `glb_objects[].material`.
- Mode policy: gamemode masterlist + `GamemodeFeatures`.

### Build

- Runtime evidence executable: build `3878`, sha256
  `e0357483865eb390cefa9df58950b6610d77777c5b8b0b602c30bd3397d3daeb`,
  `.dev/builds/3878/mimita.exe`.
- Later auto-builds (3882/3885) also compiled successfully but contain
  unrelated concurrent edits outside this slice; the runtime evidence below is
  from the exact 3878 executable.

### CLI tests

- `mimita.exe --destructible-selftest`: PASS (all checks).
- `mimita.exe --destruction-stress-selftest`: PASS (all checks).
- `python devscripts/config_selftest.py` reports overall FAIL from pre-existing
  avatar JSON validation errors; weapon and gamemode checks passed.

### Runtime journal

`C:\mimita-v9\logs\2026-10-31\20261031_041931\events.jsonl`

Confirmed on the real executable:

- TDM loaded Dust2 (`collision_triangles=5469`, `spawn_points=2`).
- TDM activated many map solids, for example `node:139` foundation and
  `node:152` concrete; client created mirrored physical entities.
- Concrete cuts were accepted with real physical energies, for example
  `energy_j=2632.3`, `radius_m=0.355`, `material=concrete`.
- Rocket radial explosion:
  `radius=12.0`, `energy_j=5062.5`, `candidates=7`, `submitted=7`,
  `capped=false`, `occluded=1`; a later blast logged `capped=true` at the
  8-node cap.
- Explosion-sourced cuts were accepted with `source=3`, `energy_j=2278`,
  `bore_m=0.8`.
- Foundation nodes were activated but no foundation cuts were accepted under
  the observed 5062 J rocket blast; concrete cuts were accepted. This matches
  the intended extreme foundation resistance.
- TDM match boundary reset:
  `world.destructible-reset` with `removed_nodes=63`,
  `removed_fragments=0`.
- Juggernaut started on Dust2 and reset with `removed_nodes=7`.
- Juggernaut loaded `atdm` and activated atdm solids, for example
  `node:35` concrete with `mesh_tris=960`; radial explosion submitted 3 solids.

### Performance follow-up (same session)

The first performance pass replaced the expensive per-activation rebuilds with
incremental work and moved client cut geometry off the render thread:

- GLB documents are parsed once and cached (`preloadDestructibleGLB`); map
  activation load dropped from ~100-750 ms to ~0.16-0.30 ms.
- Client map-node hide/restore no longer rebuilds the map mesh or collision
  broadphase. Render batches skip hidden node ranges; collision queries skip a
  parallel hidden flag. Measured hide time was ~0.003-0.05 ms.
- Server activation no longer rebuilds the headless collision grid or the NPC
  snapshot per cut. Flush total dropped from ~1.0-1.2 s to ~0.28-0.55 ms.
  NPC snapshot rebuild on world cut is now explicit config
  (`npcCollisionRebuildOnWorldCut`, default false).
- First-time activations are budgeted (`maxActivationsPerFlush`, default 4) and
  deferred FIFO to the next tick.
- Replicated client cuts are applied by an off-thread Boolean worker. Main-thread
  install measured ~0.008-0.18 ms; worker time (2-129 ms) no longer blocks the
  receive frame. The worker runs at below-normal priority.
- Network receive dispatch is budgeted (0.8 ms / 128 packets per frame) with a
  FIFO backlog (`network.rx-budget`); slow single packets are logged
  (`network.packet-slow`).
- Client destruction flush budget reduced to 0.75 ms per fixed tick.

Measured TDM/Dust2 debug run (10 actors, verbose diagnostics, build 3971):

- average frame ~2.6-3.3 ms (~300+ FPS), p95 ~4.5 ms;
- occasional maxima 4.5-9.8 ms remain, so the strict "no frame below 240 FPS"
  target is still **not met**;
- the remaining tail is outside the async cut install: engine tick, rendering,
  config polling, replay, and worker CPU contention;
- a release-build measurement was attempted, but the release build failed in the
  current mixed working tree (unrelated concurrent edits); debug evidence only.

Therefore build success, gameplay activation/cut/reset, and the 240 FPS target
are separate claims. The first two have runtime evidence; the strict zero-dip
FPS target does not yet.

## Open work

- Finish the 240 FPS zero-dip goal: profile and budget the remaining engine
  tick/render/config-poll/replay tail, and run a release-build reference
  scenario on the target machine.
- Add local shooter prediction for map solids; current map cuts are
  server-authoritative causes applied by clients.
- Client async cut results do not yet reproduce fracture children; server
  fragment spawns currently own that case.
- Finish the Map Editor material assignment UI (the JSON/per-node data layer is
  implemented).
- Verify the Survive the Disasters reset boundary under its actual ten-round
  lifecycle.
- Add actor body holes/organs/viscera as the separate actor-part slice.
- Human visual/multiplayer acceptance is still required for doorway/corner
  cuts, physical chunks, floor resistance, and reset readability.
