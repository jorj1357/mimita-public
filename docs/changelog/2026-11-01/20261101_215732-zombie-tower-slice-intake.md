# Zombie Tower 10-Floor Content Slice

- Result: `PASS_WITH_HUMAN_REVIEW` for project documents, deterministic entity authoring, and config/build checks; connected gameplay remains unverified.
- Timestamp: `2026-11-01T21:57:32Z` (2026-11-01 16:57:32 EST, America/New_York).
- Branch: `20261011zt`.
- Commit: no commit created.

## Pre-existing work preserved

The tree already contained unrelated modifications across account/audio/analytics/configuration, NPC and server gameplay, movement, collision, effects, weapons, documentation, plus an untracked regression pair. Those edits were left untouched. The dev loop was already running build 174 on map `dust2cyberiav4` in Fun Counter Strike/NPC-navigation mode; that run was left running.

## Documents and review

Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/gamemodes/zombie-tower/zombie-tower.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/quality-gates.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`.

- Behavior review: the existing mode specification covers the first playable phase/encounter/checkpoint contract, but does not supply the requested 100-floor layout or prove geometry connectivity. The new request's 100-floor requirement supersedes the older tentative 50-floor target.
- Documentation review: the three new root project files state their purpose and limits. The existing applicable spec TODO below was left unchanged per router instructions.
- Whole-docs TODO scan found the direct Zombie Tower note `docs/specs/gamemodes/zombie-tower/zombie-tower.md:5861`: `2026 10 06 1603 jorj todo - explain zombie tower`. Suggested expansion: replace the raw note with a concise statement of the tower's co-op survival premise, floor-by-floor progression, and relationship to the mode contract; retain unrelated informal historical notes as history.
- Related routed NPC document TODO: `docs/architecture/player-npc-systems/npc-movement.md:22`: `9 8 2026 1019 est jorj todo - explain`. Suggested expansion: summarize the current goal-to-route-to-shared-movement ownership and point to its multi-floor navigation acceptance evidence.

## Changes made

- Added `ZOMBIE_TOWER_SPEC.md` with permanent 100-floor, floor-size, wall-height, layout-diversity, traversal, boss, rooftop, and evidence requirements. It explicitly supersedes the previous 50-floor target without replacing the existing mode contract.
- Added `ZOMBIE_TOWER_PLAN.md` with audit, connected ten-floor slice, traversal/encounter hardening, 100-floor scale-up, and acceptance phases.
- Added `ZOMBIE_TOWER_PROGRESS.md` to separate content/source presence from static verification, build evidence, runtime evidence, and outstanding work.
- Added `devscripts/generate_zombie_tower_slice.py`. It reads the named floor anchors from `assets/maps/zombietower6.glb` and deterministically writes entity data for floors 20–29, using a seed for encounter placement. `--check` validates repeatability, unique IDs, JSON round-trip, anchors, and 100 m Z spacing.
- Added generated `config/maps/zombietower6.json`: two spawnpoints, ten checkpoints, one floor-20 Chainsaw Boss placeholder trigger, nine required monster zones, and a slice end marker.
- Updated `config/gamemodes/zombie_tower.json` and `config/server-presets.json` so the tower slice uses `zombietower6`.

Before: the Zombie Tower mode and its server preset selected `zombietower4`, whose map JSON has no authored blocks and 18 monster zones; no map config existed for `zombietower6`.
After: the mode/preset select GLB6 and its newly generated entity config describes the floor-20–29 content slice. GLB6 already contains named floor geometry for 20–98. The new script adds gameplay entities only; it does not generate or repair floor geometry.

## Validation and evidence

- `python devscripts/generate_zombie_tower_slice.py --seed 20261101 --check`: passed; ten anchors, 23 unique entities, repeatable output, JSON round-trip, and 100 m spacing.
- Focused static configuration checks: passed for expected entity counts, unique IDs, mode map, and server preset map.
- `git diff --check` over changed project/config/generator files: passed; Git emitted only the existing line-ending conversion warning for the mode config.
- Canonical build request `python devscripts/dev-loop.py --request build --wait-for-build`: returned success, build 175.
- Exact build 175 `--versioninfo`: executable `.dev/builds/0175/mimita.exe`; output `EVENTS_JSONL_PATH=logs/11-01-2026/20261101_165634/events.jsonl`. This was a version-info invocation, not a gameplay run.
- The dev-loop status later showed build 175 stale while another source generation/build was in progress; the active build 174 run remained on unrelated `dust2cyberiav4`. Build 175 was not launched into that run.
- No Zombie Tower runtime, movement, NPC-navigation, multiplayer, visual, or human acceptance evidence was collected.

## Remaining work and risks

- GLB6 lacks named floors 1–19 and 99–100. Its floor anchors 20–29 are 100 m apart; stair nodes are explicitly named for floors 26–29 but not 20–25. Neither names nor entity checks prove those transitions are connected or traversable.
- The current ten-floor data does not meet the requested geometry-generation or 75% non-standard-layout acceptance.
- Next: launch the configured slice through the dev loop after coordinating with the active unrelated run, inspect client/server map-load and navigation journals, test actual player movement and NPC traversal, then repair the first observed route owner. Continue with a reusable native geometry generator rather than hand-editing each floor.
