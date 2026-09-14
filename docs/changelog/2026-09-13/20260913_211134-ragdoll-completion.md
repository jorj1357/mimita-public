# Ragdoll completion: remote interpolation, generic constraints, dead-config removal

- EST timestamp: 2026-09-13 17:11:34 EDT (UTC 2026-09-13T21:11:34Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); two-client runtime proof pending

## What was implemented

### Remote ragdoll presentation (perfectPose skeleton)
- `src/ragdoll/ragdoll-body.h/.cpp` (new): extracted the shared body builder and
  skeleton applier (`Ragdoll::buildBody`, `Ragdoll::applyBodyToPlayer`,
  `Ragdoll::rigidWorld`) from `RagdollModeSystem::initParts`/`syncToPlayer`; both
  are now thin wrappers. One builder/applier for local, corpse, and remote.
- `src/ragdoll/ragdoll-presentation.h/.cpp` (new): per-owner ordered frame
  buffer (`pushFrame`), tick/wall-clock interpolation with configurable delay,
  capped extrapolation, hold, staleness fallback, and `perfectPoseSkeleton`
  write via `applyBodyToPlayer`. Derived-only: never writes components, never
  feeds the solver.
- `multiplayer-tick.cpp`: received `PACKET_RAGDOLL_STATE` frames are pushed to
  the presentation buffer in addition to `applySnapshot`.
- `engine-tick-render.cpp`: remote player/NPC render calls
  `RagdollPresentation::present(...)` before `renderNetworkPlayer`, so remote
  ragdolls render their skin from the interpolated buffered limbs.
- `multiplayer-packets.cpp`: presentation buffers cleared on context reset.

### Generic constraint primitive
- `src/physics/constraints/constraint-components.h` (new): `ConstraintType`
  (Point/Distance/Grab), value `Constraint`, ECS `ConstraintComponent`
  (serial/owner/created/release), `kWorldBody`.
- `src/physics/constraints/constraint-solver.h/.cpp` (new): `SolveBodyTable`
  interface and `solveConstraints` (world, body-body, distance) built on the
  existing point-joint math. One path for ragdoll/object/vehicle constraints.
- `ragdoll-solver.cpp`: `solveGrabs` now maps hand state to `Physics::Constraint`
  and delegates to `Physics::solveConstraints` through a `RagdollBodyTable`
  (no grab special case). `ragdoll-entities.cpp` writes a `ConstraintComponent`
  alongside every `GrabComponent` (set and snapshot-apply paths).
- `engine-tick-render.cpp`/inspector: `CreationMode::describe` prints
  constraint fields.

### Dead config/state removal
- Removed `RagdollModePart::configIndex`, `RagdollModeConfigData::jointStiffness`
  and `jointDamping` (plus loader + `config/ragdoll.json` keys),
  `RagdollModeAttachmentConfig::coneLimitDeg` (plus loader + JSON keys), and
  `RagdollModeSystem::mNextCorpseSerial`. Verified each had no readers.

### Telemetry
- `RagdollPresentationInterp` scope + per-owner receive counters;
  `ConstraintSolve` scope; existing `RagdollSolverSubstep` scope retained.

## Evidence

- `python build_agent.py` -> `BUILD SUCCESS`, mimita.exe relinked
  (`Compiled: 125, Skipped: 406`), then a second 1-TU incremental relink after a
  self-test fix (`Compiled: 1`).
- `mimita.exe --ragdoll-slice-selftest` -> **PASS** (all 24 checks, including the
  generic constraint component and presentation ordering/replacement/clear).
- `--live-code-selftest`, `--hot-authoritative-selftest`,
  `--entity-slice-selftest`, `--project-selftest`, `--phase456-selftest`,
  `--telemetry-selftest`, `--creation-selftest` -> all **PASS** (exit 0).
- `python build_game_dll.py --output build/verify-ragdoll2-game.dll` -> success
  (4 hot sources; no hot module otherwise changed).
- `-fsyntax-only` clean (mingw64 g++, `-std=c++17`, pch): `ragdoll-body.cpp`,
  `ragdoll-mode.cpp`, `ragdoll-mode-config.cpp`, `ragdoll-entities.cpp`,
  `ragdoll-solver.cpp`, `ragdoll-presentation.cpp`, `ragdoll-slice-selftest.cpp`,
  `constraint-solver.cpp`, `multiplayer-tick.cpp`, `multiplayer-packets.cpp`,
  `server-packets.cpp`, `server.cpp`, `engine-tick-render.cpp`,
  `simulate-tick.cpp`, `creation-mode.cpp`, `community-match-client.cpp`,
  `ragdoll-death-config.cpp`.
- `config/ragdoll.json` parses (python `json.load`) after dead-key removal.

## Bootstrap cold build (installed)

The remote presentation, constraint primitive call sites, and inspector fields
are EXE-owned mechanisms. The one bootstrap cold build was run after the prior
`mimita.exe` had exited (no game was running; no process was killed):
`python build_agent.py` -> success, then all self-tests -> pass. After this
bootstrap, ragdoll policy/behavior edits in `src/hot-reload/modules/` remain hot.

## Runtime proof (pending, not claimed)

- Headless `mimita.exe --ragdoll-slice-selftest` run and result: **done, PASS**
  (above).
- Two-client proof (not yet run): remote ragdoll interpolates (alpha in (0,1),
  no snap), loss hold/extrapolate, cross-limb/broadcast grab, deterministic
  corpse, hot policy swap with stable ids. Procedure in §8 of the plan.

## Known limitations

- The body table currently resolves one `RagdollBody` (local alive/corpse);
  cross-actor two-body solve requires remote ragdolls to be locally simulated,
  which is out of scope. Cross-actor grabs still travel as the replicated moving
  anchor, now expressed as a generic constraint.
- Persistent-object/NPC/vehicle adapters are not registered in the body table
  yet; the `SolveBodyTable` interface is the plug point.
- Constraint create/release replication events and late-join constraint
  snapshot are not implemented in this pass (local constraint entity + codec
  fields exist).

## Files

New: `src/ragdoll/ragdoll-body.h`, `ragdoll-body.cpp`,
`src/ragdoll/ragdoll-presentation.h`, `ragdoll-presentation.cpp`,
`src/physics/constraints/constraint-components.h`, `constraint-solver.h`,
`constraint-solver.cpp`.

Changed: `src/ragdoll/ragdoll-components.h`, `ragdoll-entities.h/.cpp`,
`ragdoll-mode.h/.cpp`, `ragdoll-mode-config.h/.cpp`, `ragdoll-solver.cpp`,
`ragdoll-slice-selftest.cpp`, `src/network/multiplayer-tick.cpp`,
`multiplayer-packets.cpp`, `src/engine/engine-tick-render.cpp`,
`src/editor/creation-mode.cpp`, `config/ragdoll.json`,
`docs/architecture/live-development/ragdoll-live-network.md`.

## Pre-existing edits preserved

All unrelated working-tree changes were preserved; only the files above edited.
