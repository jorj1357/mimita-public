# Agent lane import, integration, and build-speed policy

- Date: 2026-11-09T18:18:52Z (13:18:52 EST)
- Branch: `checkpoint/pre-live-kernel-20261108`
- Commits:
  - `4f24af8c` checkpoint: preserve pre-integration workspace edits
  - `5af24993` build: enforce repository-local lane and build paths
  - `b7516a78` build: use all logical processors at high priority
  - `ed8787fe` build: lower memory guard to 16 MB and pin high priority
  - `2fa7178c` merge: integrate reviewed agent lanes into pre-live kernel
  - `349a03ec` fix: use server gamemode state in duel map load
- Result: the seven intended lane changes are imported/reviewed inside the
  repository, the integration branch is merged into the active branch, and a
  testable executable was built by the main dev-loop. Runtime validation is
  partial; mode-start and map-sync failures remain open.

## Repository containment

Added `devscripts/path_containment.py` as the single containment owner and
hardened:

- `devscripts/agent-lane.py`: lane worktree, metadata, registry, snapshot,
  promotion, status, run, check, cleanup, list, and stable paths resolve inside
  the repository. Escaping paths, metadata, symlinks, junctions, and mismatched
  worktrees are rejected before use.
- `devscripts/build_scheduler.py`: default scheduler root is
  `<repo>/.dev/build-scheduler`; `MIMITA_BUILD_SCHEDULER_DIR` outside the repo
  fails clearly.
- `build.py`, `build_game_dll.py`, `devscripts/dev_loop/build.py`: shared ccache
  is `<coordinator>/.dev/ccache`; temporary files are under `.dev/tmp/`;
  `MIMITA_COORDINATOR_ROOT` is validated.
- `devscripts/dev-loop.py`: room files use `.dev/tmp/<lane>/`; scenario and
  input paths must resolve inside the repository.
- `devscripts/world-project.py`: default store is `.dev/world-projects`;
  `MIMITA_WORLD_PROJECTS_DIR` outside the repo is rejected.
- `src/pch.h.d` is no longer tracked, so generated dependency output cannot
  trigger rebuild loops.

The focused checks are in `devscripts/quality/test_path_containment.py`.

## Imported and reviewed lanes

The human supplied the seven legacy lane copies under
`.dev/2026-11-09/`; `.dev/agent-lane-imports` is an in-repo junction to that
archive. No agent read or wrote the old `C:\mimita-v9-agent-lanes` folder.

- `build-boundary-20261108`: imported the link ledger, atomic link publish,
  no-op artifact reuse, module-private headers, lane `check` observation, and
  map-geometry/server-authored rebuild source.
- `npc-collision-hot-20261108`: imported the actor-world collision slice,
  event `406`, module ABI changes, and the lane changelog.
- `world-streaming-nav-reconnect-20261108`: imported the gamemode-gated
  streaming, tiled navigation, map-sync/session-health policy, reconnect
  changes, navigation policy event `800`, and the lane changelog.
- `network-session-diagnostics-20261105`: its source fix was already present;
  imported its missing evidence changelog.
- `grenade-collision-20261105`, `vehicle-arena-20261105`,
  `world-projects-20261105`: reviewed; their source and changelogs were already
  present at the preservation base. No duplicate source was applied.
- `src/pch.h.d` build artifacts were deliberately excluded from imports.

Integration conflict decisions:

- Kept the world-streaming tiled navmesh query path and removed the older
  `navmesh_stale` stub from the build-boundary lane.
- Assigned map-geometry event `810`; navigation-policy keeps `800`.
- Merged module event IDs `406` and `706`/`707`/`708`.
- Preserved the repository-local lane/build policy in the build and dev-loop
  docs.

## Build-speed policy

Restored the intended policy in code and specs:

- `build.py`: all available logical processors, no fixed four-job cap; the build
  process sets itself to `HIGH_PRIORITY_CLASS`, so compiler children inherit it.
- `devscripts/dev_loop/build.py`: `HIGH_PRIORITY_CLASS`; removed the per-lane
  four-job clamp.
- `devscripts/build_scheduler.py`: `DEFAULT_MIN_FREE_MB = 16`.
- `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/specs/dev-loop/dev-loop.md`, and
  `docs/operations/build-and-exe/build-and-exe.md` now say one job per logical
  processor, high priority, and the 16 MB memory guard.

The earlier uncommitted build-priority work and its changelog
(`20261109_123644-build-priority-jobs-headless.md`) were preserved and
committed as `b7516a78`; the 16 MB guard and self-priority pin are `ed8787fe`.

## Merge and compile fix

`2fa7178c` merges the integration branch. A single compile error then appeared:

- `src/network/server-gamemode.cpp:2877`, inside `tryLoadDuelMap`, used
  `d.matchMode` although `d` is not in scope.
- Fixed in `349a03ec` to use `serverGamemodeState().matchMode`, the existing
  state accessor declared at `server-gamemode.cpp:150`.

## Validation

Source/static:

- `python -m py_compile`: PASS for changed Python/build entry points.
- `python devscripts/module_ownership.py .`: PASS, 11 module sources, 581 EXE
  sources, unique events through `810`.
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `python devscripts/weapon_json_sync.py --check`: definitions=36 errors=0.
- JSONC parse of 24 changed config files: PASS.
- `python devscripts/quality/test_path_containment.py`: 6 tests PASS, including
  an escaping junction rejection.
- `src/pch.h.d` is untracked; `git diff --check` is clean.

Build:

- Main dev-loop build artifact:
  `.dev/builds/2026-11-09T18-11-10.953010Z/mimita.exe`
- Build ID:
  `2026-11-09T18:11:10.953010Z:f538b96de6225c213ac8bbe8dd672e9479074157eb8c0f1048b33ad7ad73a6c6`
- SHA-256: `f538b96de6225c213ac8bbe8dd672e9479074157eb8c0f1048b33ad7ad73a6c6`
- Build output: `Compiled: 1`, `Skipped: 622`, `Time: 33.66 sec`.
- The build started from revision `2fa7178c` with `dirty=true` because the
  one-line fix was still uncommitted. The fix content was present and compiled;
  the fix was committed afterwards as `349a03ec`. A clean-source rebuild was not
  requested during this session.

Runtime journals (main dev-loop; build above):

- Mode 56: `logs/2026-11-09/20261109_181141/`; server/client online,
  `funworld3`, 23 NPCs, 20 `performance.server-tick-stall`, 5
  `network.snapshot.timeout`, 1 `network.ice-reconnect.start`.
- Mode 57: `logs/2026-11-09/20261109_181330/`; 19 NPCs,
  `npc.navmesh.bake-success build_ms=3853.4`, 24 `npc.slow-update`, 10
  `performance.server-tick-stall`; client remained `active_map_mismatch` with
  `map_ready=false` and server heartbeat `players=0`. Mode start timed out.
- Mode 52: `logs/2026-11-09/20261109_181515/`; `infinite_cramped`,
  `world-stream.server.cells cells=81`, client reached `map_ready=true` and
  applied an authoritative spawn, one initial
  `world-stream.collision.sync base_broadphase_rebuilt=true`. Mode start timed
  out.
- Mode 54: `logs/2026-11-09/20261109_181625/`; `infinite_mixed`, 8 spawn
  cycles, 1 `network.snapshot.timeout`, world-stream identity/cell events. Mode
  start timed out.
- Mode 55: `logs/2026-11-09/20261109_181722/`; `infinite_cramped`, 6 spawn
  cycles, 5 `performance.server-tick-stall`, 1 `network.snapshot.timeout`,
  world-stream identity/cell events. Mode start timed out.

Containment acceptance:

- `python devscripts/agent-lane.py list` skips the seven old sibling lanes as
  `outside_repo` and shows in-repo lanes only.
- Fresh lane `verify-repo-local-20261109` was created under
  `.dev/agent-lanes/verify-repo-local-20261109` and reports ready.
- The old `C:\mimita-v9-agent-lanes` archive was not modified.
- `.dev/agent-lane-imports` is an in-repo junction to `.dev/2026-11-09`.

## Documents and skills

Read/used: `AGENTS.md`, `docs/ROUTER.md`,
`docs/specs/dev-loop/agent-lanes-build-authority.md`,
`docs/specs/dev-loop/dev-loop.md`,
`docs/operations/build-and-exe/agent-lane-integration-and-pending-validation.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/specs/debug-logging/debug-logging.md`,
`docs/specs/networking/networking.md`,
`docs/specs/networking/contact-authority.md`,
`docs/specs/npc-behavior/npc-behavior.md`,
`docs/architecture/collision/collision.md`,
`docs/specs/performance/performance.md`,
`docs/skills/spec-behavior-review-v1.md`, and the lane changelogs under the
import archive.

## Remaining and human review

- Mode start timed out for modes 52, 54, 55, and 57 even though servers and
  clients started and journals show spawn/world-stream activity. Investigate
  the launch `mode_start` readiness contract separately.
- Mode 57 still fails map synchronization: client `active_map_mismatch`,
  `map_ready=false`, server `players=0`.
- Server tick stalls remain in modes 56, 57, and 55; NPC update cost is the
  dominant sampled contributor.
- A clean-source rebuild after `349a03ec` would remove the `dirty=true` build
  identity qualification.
- Human gameplay/visual review was not performed. Do not claim visual,
  multiplayer-feel, or stable promotion acceptance from these journals.
