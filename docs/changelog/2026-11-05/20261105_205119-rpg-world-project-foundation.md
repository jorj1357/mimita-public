# RPG world-project foundation — 2026-11-05

## Session record

- Timestamp: 2026-11-05 15:51:19 EST (2026-11-05T20:51:19Z).
- Branch: `20261011zt`.
- Commit: not created.
- Pre-existing worktree: heavily modified before this task, including source,
  config, assets, docs, and regression-file moves/deletions. Those changes were
  preserved. In particular, pre-existing edits in `docs/ROUTER.md`, the RPG,
  entity-editor, networking, and vehicle-arena documentation remain mixed with
  this task's additions; no unrelated source modifications were made.

## Implemented

- Added `devscripts/world-project.py`, a local offline map-project store.
  It imports map JSON, saves canonical SHA-256-addressed immutable versions,
  forks a selected version with parent lineage, lists/shows versions, and
  verifies version bytes before printing the map path. Data is stored below
  the per-user `Mimita/WorldProjects` folder. Licensing metadata marks creator
  confirmation as required and leaves dependency license terms explicit.
- Added `docs/specs/world-projects/world-projects.md`, including project/version
  semantics, typed licensing, privacy and recovery constraints, data ownership,
  collaboration ordering/conflicts/replay, and the distributed-storage
  comparison and first-slice recommendation. Its initial implementation
  section states the local CLI's actual limits.
- Extended the RPG, entity-editor, networking, data-saving, procedural-world,
  asset-management, and project-layer documents with the accepted design and
  implementation sequence. Added the project routes in `docs/ROUTER.md` and
  removed duplicate copies of those two rows while preserving the user's other
  router changes.
- Corrected `docs/architecture/live-development/project-layer.md` to identify
  `src/project/` and `project-selftest` as proposed, not present in this tree.

## Validation evidence

- Source: `python -m py_compile devscripts/world-project.py` passed.
- CLI behavior: manually created a project from `config/maps/arena1.json`,
  opened and verified its immutable version, forked it, modified the fork's
  working map, saved a child version, and confirmed project listing and
  parent/head metadata. The exercise used a temporary per-user root.
- Documentation: reviewed the changed requirements and links; `git diff
  --check` found the existing/pre-existing trailing whitespace at
  `docs/specs/gamemodes/rpg.md:65` within the dirty RPG text. This was left
  untouched to avoid altering the user's pre-existing note.
- Build: not run; the implementation is Python-only.
- Runtime/multiplayer: not run. The local CLI is not connected to `MapEditor`
  or network packets and cannot prove the plan's multiplayer scenarios.
- Human acceptance: not performed.

## Scope still open

The plan is not complete. In-game project open/fork/version browsing, live
host-authoritative edits, named capabilities, conflicts/undo/replay/snapshot
recovery, dependency and content distribution, encrypted profile recovery,
world-scoped progression, chunk and zone authoring, NPC presets, and map-authored
F tap/hold behavior still need implementation and runtime evidence. No new
network packet or interaction behavior was added. The F path audit found a
press-edge handler and target callback in `InteractionSystem`; no reproducible
broken-key evidence was available to justify changing that path.

## Review references and unresolved note

- Read `AGENTS.md`, `docs/ROUTER.md`, the world-project, RPG, data-saving,
  entity-editor, networking, procedural-world, asset-management, live-project,
  live-authored-world, animation, GUI, build/dev-loop, and multi-client routes.
- Applied `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/documentation-checker-v1.md`.
- The current procedural-world document begins at
  `docs/specs/procedural-infinite-world/procedural-infinite-world.md:1` with
  “todo - explain the inf dng slr infinite world gneration, and like”. Suggested
  expansion: define generation as seeded rules over reusable rooms/chunks,
  explain the Minecraft comparison, and link the RPG zone, authority, and
  determinism requirements. It was preserved as requested by the router's TODO
  rule.
