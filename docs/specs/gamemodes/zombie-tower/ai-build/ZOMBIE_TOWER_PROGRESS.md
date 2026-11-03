# Zombie Tower Progress

Updated: 2026-11-02

This is the project status record. Each item is labeled as implemented, verified, broken, or unfinished; implementation and verification are separate states.

## Implemented (source/content exists; not proof of behavior)

- [x] A detailed first-playable mode contract exists in `docs/specs/gamemodes/zombie-tower/zombie-tower.md`.
- [x] Zombie Tower mode configuration, run logic, actor presets/roles, monster pools, team selection, checkpoints, encounter entities, boss state, and presentation paths exist in source/config.
- [x] The native map format supports authored box blocks and gameplay entities. `MapConfigRegistry` parses/saves `config/maps/<mapId>.json`; `applyAuthoredMapGeometry` adds block render/collision geometry to client and headless server worlds.
- [x] The viewport Map Editor documents selection, dragging, copy/paste, duplicate, undo/redo, and map JSON persistence.
- [x] Six Zombie Tower GLBs exist (`zombietower1` through `zombietower6`). GLB6 contains named floor geometry for floors 20–98. It does not contain named floors 1–19, 99, or 100.
- [x] Added `devscripts/generate_zombie_tower_slice.py`, which deterministically authors a 10-floor map-entity slice from GLB6 floor anchors 20–29.
- [x] Added `config/maps/zombietower6.json` with lobby/intro spawnpoints, checkpoints 20–29, required encounters 21–29, a floor-20 Chainsaw Boss placeholder trigger, and a slice end marker.
- [x] Updated the Zombie Tower mode map list and `zombie_tower_1` server preset to use `zombietower6` for this slice.

## Verified in this session

- [x] Router, current Zombie Tower spec, entity-editor spec, movement spec, collision architecture, NPC navigation architecture, runtime-scenario workflow, quality gates, build procedure, task completion procedure, and focused behavior/documentation review instructions were inspected.
- [x] Working tree contains many pre-existing modifications in server gameplay, NPC/navigation, collision documentation, configs, and other systems. They are unrelated user work and must be preserved.
- [x] `config/maps/zombietower4.json` has no authored blocks and contains 18 monster-zone entities; it is not a generated 10-floor map.
- [x] GLB6 contains floor anchors 20–29 at 100 m Z intervals. Floors 26–29 have explicitly named stair nodes; floors 20–25 do not. These names alone do not prove that the earlier transitions are disconnected, because other authored geometry may provide a route.
- [x] The map catalog discovers only `.glb`/`.gltf` assets. Native authored blocks augment a loaded base map; there is no config-only map path in the inspected code.
- [x] `python devscripts/generate_zombie_tower_slice.py --seed 20261101 --check` passed: ten anchors, 23 unique entities, repeatable same-seed output, JSON round-trip, and 100 m floor-anchor spacing.
- [x] Static configuration checks passed for the generated map entity counts, server preset map, and Zombie Tower mode map.
- [x] Canonical dev-loop build request succeeded as build 175. It is a compile/link result only; the executable has not been launched for this tower slice.
- [x] Build 175 `--versioninfo` identified `.dev/builds/0175/mimita.exe` and printed `logs/11-01-2026/20261101_165634/events.jsonl`. That invocation only printed version information; the journal is not runtime proof of Zombie Tower.

## Broken

- None confirmed during this session. No Zombie Tower runtime scenario or human playtest was performed, so gameplay failures are not classified here.

## Unfinished / unverified

- [ ] Confirm the selected GLB geometry and map coordinate transforms in the running client and server.
- [ ] Prove the 20–29 route is connected, including the floor-20-to-26 transitions and whether the 100 m floor spacing is compatible with traversal constraints.
- [ ] The current generated slice authors gameplay entities only; it does not generate floor geometry or demonstrate the 75% non-standard layout target.
- [ ] Verify actual fast player movement, jumping, dashing, projectile use, collision, and NPC navigation across the slice.
- [ ] Implement reusable seeded native floor geometry generation and scale it from ten floors toward 100. Existing GLB6 does not cover the required beginning and rooftop.
- [ ] Add/verify bosses on floors 20, 40, 60, 80, and 100, the open-air rooftop, and final-boss victory.
- [ ] Run relevant regressions and real runtime scenarios; record exact launched executable identity and client/server event journals.
- [ ] Obtain human visual/gameplay acceptance.

## Current blocker / next action

The current dev loop is running an unrelated NPC-navigation/Fun Counter Strike session on build 174. It was preserved and not replaced, so this session did not launch build 175 into a Zombie Tower run. The next validation step is to use the dev loop for a Zombie Tower launch on `zombietower6`, then inspect client and server journals while traversing floors 20–29. Fix the first actual route or navigation divergence before describing the slice as playable.

## Blender blockout — 2026-11-02

### Implemented

- [x] Saved first version into `C:\important\quiet\blender-projects\mimita zomb tower ai 2.blend`.
- [x] Foundation bottom is Z=20 m and top is Z=30 m, with dimensions 500 m × 500 m × 10 m.
- [x] Tower floors 1–100 each rise 50 m and fit a 100 m × 100 m footprint. Floor 1 begins at Z=30 m; floor 100 ends at Z=5030 m.
- [x] One continuous 32 m-wide square-spiral ramp loop per floor joins each loop to the next; 90 of 100 floors have distinct non-standard blockout treatments. Floor 100 is an open-air rooftop.
- [x] Added 100 solid 100 m × 100 m, 2 m-thick decks with L-shaped ramp passages, plus 32 m-wide ramp landings that bridge those passages.
- [x] Enclosed all 100 floor perimeters with four-sided walls; the first floor has a framed entrance.
- [x] Blender scene checks confirmed 100 floor markers, 100 ramp objects, 100 labels, 100 wall/support structures within 100 m × 100 m, and exact foundation dimensions.
- [x] Blender mesh-bound checks confirmed all 100 solid decks are exactly 100 m × 100 m, their top surfaces are 50 m apart, and the ramp passages keep 3.8 m or more vertical clearance.

### Unverified / unfinished

- [ ] This is a Blender design/blockout scene. It has not been exported or loaded as MiMITA map geometry.
- [ ] Runtime fall-stop behavior on each deck and traversal across every ramp-to-walkway seam have not been exercised in MiMITA.
- [ ] Smooth-looking ramp continuity is not proof of MiMITA collision continuity. Player walk/run/jump/dash/rocket-jump and NPC navigation have not been exercised.
- [ ] The 200/300/500 m/s values are design references only; game movement configuration and role abilities still need runtime inspection and validation.
- [ ] Review per-floor spacing and routes at game scale, including the 5–50 m optional platform gaps, with the actual MiMITA actors.

## Enclosed boss arena — 2026-11-02

### Implemented

- [x] In `C:\important\quiet\blender-projects\mimita zombie tower only with ai.blend`, replaced the selected solid 1,008-unit cube with an enclosed tiled boss room built from the tower wall materials.
- [x] Removed the intermediate floor modules inside the room and made one broad arena ground from 100 tiles.
- [x] Built 360 wall blocks, 99 ceiling blocks, and left one 100 × 100 tile opening aligned with the retained upper stair.
- [x] Added 30 deterministic surf obstacles (stretched cylinders, spheres, and cones), repositioned two existing props, and added five ascending launch ramps.
- [x] Blender checks found no remaining floor 43–51 objects, confirmed the 360/99/100 tile counts, verified the generated meshes are closed, and checked all tiled pieces use existing tower wall materials.

### Unverified / unfinished

- [ ] Player collision, surf movement, jumping, fall recovery, and exit traversal have not been tested in MiMITA runtime.
- [ ] The 1.8 m character plus 2 m headroom target is expressed by a 100 × 100 ceiling opening, but a MiMITA actor has not been driven through it.
- [ ] The arena is a Blender blockout and has not been exported to the runtime map format.
