# Zombie Tower Implementation Plan

## Purpose and scope

This document tracks phased, testable implementation milestones for `ZOMBIE_TOWER_SPEC.md`. It does not replace the gameplay specification or treat source/config presence as runtime proof.

## Phase 0 — ownership and baseline audit

- [x] Read the Zombie Tower contract, map/entity editor contract, movement and collision rules, NPC navigation rules, runtime evidence workflow, build authority, and task completion requirements.
- [x] Inspect working-tree changes and preserve all pre-existing edits.
- [x] Locate current map assets, map JSON, mode configuration, map editor, authored-block geometry path, and server-owned Zombie Tower phase/encounter code.
- [ ] Establish a baseline Zombie Tower runtime journal; the existing live dev-loop run was an unrelated mode and was preserved.
- [ ] Confirm map asset transforms, route connectivity, and exact supported movement capabilities.

## Phase 1 — connected 10-floor vertical slice

- [x] Reuse GLB6 named geometry for floors 20–29 as an initial existing-asset content slice.
- [x] Add a deterministic map-entity authoring script and checkpoints/encounters/boss/end markers for those ten floors.
- [ ] Implement reusable deterministic native floor-geometry generation with an explicit seed and floor index.
- [ ] Generate/verify ten connected floors with 100 m × 100 m bounds, approximately 50 m walls, and at least eight distinct/non-standard layouts (meeting the 75% slice target).
- [ ] Prove a continuous legal route from the slice entry through every floor to the top; verify floor-to-floor geometry and traversal diagnostics.
- [ ] Validate map data, IDs, bounds, repeatable generation, and map-editor save/reload.
- [ ] Build through the dev loop, launch the exact artifact, capture `--versioninfo` and `EVENTS_JSONL_PATH`, and exercise actual player movement and NPC navigation through the slice.
- [ ] Record first divergence and fix the owning step; do not claim playability from build or data checks alone.

## Phase 2 — traversal and encounter hardening

- [ ] Validate jumping, dashing, projectiles, collision seams, narrow paths, moving between vertical regions, and 5–50 m gaps only where the configured role can cross them.
- [ ] Validate NPC route projection, traversal-link choice, shared ActorIntent/movement execution, recovery, and unreachable-goal reporting on the same routes.
- [ ] Run relevant collision, movement, map-editor, NPC, and Zombie Tower regressions; preserve unrelated dirty work.
- [ ] Add regression records only for confirmed behavior breaks.

## Phase 3 — scale the generator to 100 floors

- [ ] Extend the ten-floor module through floor 100 without hand-authoring each floor.
- [ ] Place major bosses on 20, 40, 60, 80, and 100; make floor 100 an open-air rooftop and make final-boss defeat trigger the existing win owner.
- [ ] Verify connections, bounds, encounter IDs, boss ordering, deterministic seeds, and 75% layout diversity across all 100 floors.
- [ ] Profile map loading, collision construction, navigation generation/query, NPC count, memory, and fixed-tick performance before claiming scale.
- [ ] Run a real end-to-end run to the rooftop and verify final-boss victory and results behavior.

## Phase 4 — acceptance and release readiness

- [ ] Complete regression and multiplayer checks relevant to changed paths.
- [ ] Report build, runtime journal, player observation, NPC observation, visual review, and human acceptance as distinct evidence.
- [ ] Update this plan and `ZOMBIE_TOWER_PROGRESS.md` with remaining issues and attach the final session changelog.

## Current limits

The existing mode logic has an extensive first-playable contract, and GLB6 contains many floors, but neither the current map nor this first content slice proves a connected 10-floor route. GLB6 lacks named floors 1–19, 99, and 100. It has explicit stair nodes on floors 26–97; floors 20–25 need route inspection. The slice currently adds gameplay markers only, not procedural floor geometry.
