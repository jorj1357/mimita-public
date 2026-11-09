# Lane build ledger, no-op publish reuse, private module headers

Lane: `build-boundary-20261108`  
Worktree: `C:\mimita-v9-agent-lanes\build-boundary-20261108`  
Branch: `codex/agent-build-boundary-20261108`

## Purpose

Implement Phase 0 of the lane-native build plan: make the lane dev-loop the
only builder, make stale/missed relinks self-repairing, stop duplicate no-op
artifacts, and support module-private headers without EXE rebuilds.

No agent build command was run. The lane dev-loop built and published the
artifacts cited below.

## Changes

### Link ledger and atomic output

`build.py` now keeps:

- `build/link.pending.json` before every link;
- `build/link-state-<mode>.json` only after a validated successful link.

The link decision now checks:

- pending or failed prior link;
- missing/zero-byte objects;
- object set, size, and mtime signature;
- objects newer than the executable;
- executable changes since the successful link;
- compiler/linker flag fingerprint.

A failed link writes a failed pending ledger so the next lane dev-loop build
relinks automatically. The linker writes to `build/out/mimita-candidate-<pid>.exe`,
then the validated output is atomically moved to `mimita.exe`.

No agent sets `MIMITA_FORCE_LINK`; the ledger replaces the manual workaround.

### No-op artifact reuse

`devscripts/dev_loop/build.py` now reuses the latest immutable artifact when
the newly built root `mimita.exe` has the same SHA-256. It does not create a new
`.dev/builds/<id>` folder, and it reuses the verified identity instead of
copying and hashing the 238 MB executable again.

### Optional presentation DLL

`build_game_dll.py` is no longer an unconditional prebuild and its failure no
longer aborts the EXE build. It runs only when one of its own inputs changed.

### Module-private headers

`devscripts/module_ownership.py` now understands `private_headers`. The dev-loop
watch exclusion includes module-private headers, and the module worker snapshots
and hashes them. Public ABI headers remain cold-watched; private module headers
can change without an EXE rebuild.

### Lane workflow

- `agent-lane.py check` no longer calls the refused build path for C++ changes.
  It reports lane dev-loop build status instead, so `check` remains an
  observation command.
- `agent-workflow.py` defaults to mode 2; mode 1 remains an explicit
  coordinator fallback.
- Removed the dev-loop spec's "Mode 2 experimental; do not use" contradiction.
- Removed the build-and-exe doc's `agent-lane.py build` command and stale-object
  deletion advice.
- Aligned `AGENTS.md` gate 5 with the isolated lane dev-loop.

## Evidence

Lane build: `2026-11-08T20:16:08.092639Z:5b2f466251aa4b871efd180b6d615e072db9df0e131f833beb30b52b25e4a760`  
Artifact: `.dev/builds/2026-11-08T20-16-08.092639Z`  
Latest published artifact after no-op test: `.dev/builds/2026-11-08T20-20-14.893679Z` (same SHA-256)

- `build/link-state-debug.json` exists with status `linked`, object signatures,
  flags, executable signature, and SHA-256.
- `build/link.pending.json` is absent after success.
- A build.py-only source edit caused the lane dev-loop to run a no-op build:
  `Nothing changed. (current)`.
- The no-op did not create a new artifact: artifact count stayed 3, latest
  build ID remained the same.
- `agent-lane.py check` passed for the Python/build changes without invoking a
  build.

## Not implemented

The remaining plan phases are not implemented in this lane:

- async candidate rebuild + detached Recast + atomic swap;
- `map-geometry` hot module;
- full map-editor extraction and module-owned undo/redo/clipboard;
- GLB schema v2 rotation/scale/tombstones/clones/imports;
- one ABI-first boundary build.

The next lane should start from this Phase 0 work and perform the single ABI
boundary build.

## Repository-local workspace and documentation — later same session

Implemented the workspace migration described by the 2026-11-08
`open-gameplay-issues.md` note.

- Lane worktrees and registry: `.dev/agent-lanes/<lane>` via
  `devscripts/agent-lane.py`.
- Build scheduler default: `.dev/build-scheduler`.
- Shared lane ccache default/env: `.dev/shared-agent-ccache`.
- Lane temp env: `.dev/tmp`; dev-loop room files: `.dev/runtime/rooms`.
- World-project default store: `.dev/world-projects`.
- All paths remain under the gitignored `.dev/` root; no generated state is
  tracked.
- `agent-lane.py` creates lane metadata and worktrees in the same repo-local
  root and sets scheduler/cache/temp env for lane children.
- Verified a nested Git worktree under `.dev/agent-lanes` works and can be
  removed cleanly.
- Verified path helpers resolve to:
  - `C:\mimita-v9\.dev\agent-lanes\demo`
  - `C:\mimita-v9\.dev\build-scheduler`
  - `C:\mimita-v9\.dev\world-projects`

Documentation added/updated:

- New canonical workspace rule:
  `docs/architecture/live-development/self-contained-workspace.md`.
- New continuation plan:
  `docs/architecture/live-development/module-migration-plan.md`.
- Cold-build policy and “effectively never” wording added to
  `docs/specs/hotreload/hotreload.md`.
- Cross-links added to `AGENTS.md`, `docs/ROUTER.md`,
  `docs/architecture/live-development/live-development.md`,
  `docs/architecture/live-development/hot-kernel.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/operations/build-and-exe/build-and-exe.md`, and
  `docs/specs/open-gameplay-issues.md`.

The policy is: cold builds only for process/device/transport/packet-schema/
document-storage/loader/public-ABI changes; gameplay/tool behavior belongs in
modules; the long-term “effectively never” path is a generic data-driven
event/behavior IR.

## Async authored rebuild + detached Recast — later same session

Implemented the first ABI/host slice:

- `RecastNavMeshCandidate` builds an unpublished Detour mesh/query off-thread.
- `RecastNavigationBackend::adopt()` atomically publishes a candidate and bumps
  the navmesh version.
- Query no longer bakes on the fixed tick; a stale navmesh returns
  `navmesh_stale` until a candidate is adopted.
- `buildNpcWorldCollisionDetached()` builds a candidate NPC collision world
  from supplied destructible surface triangles without touching
  `PhysicalEntitySystem`, `BehaviorProfileRegistry`, the infinite-collision
  global, the logger, or the Recast singleton.
- New `server-authored-rebuild` worker builds a candidate authored world, a
  pristine cache copy, a destructible-applied world, candidate NPC collision,
  and candidate navmesh off the fixed tick.
- `ServerGamemodeState` captures shared immutable base/cache worlds; queueing no
  longer copies the entire base world on the tick.
- Drain validates map/signature, swaps `world`/`npcWorld` contents at the tick
  boundary, adopts navmesh, updates caches/revision, broadcasts, and retires old
  worlds back to the worker for freeing.

Runtime evidence (`run 20261108_211101`, funworld3, index 0 moved +5):

- `map.geometry.rebuild.queued` at tick 1818.
- No `performance.server-tick-stall` at the queue tick.
- `map.geometry.reload.applied` at tick 2342:
  - `worker_ms=9051.01`
  - `authored_ms=2610.28`
  - `npc_collision_ms=5743.92`
  - `new_navmesh_version=2`
- No `performance.server-tick-stall` at the swap tick; the heavy work ran on
  the worker and old worlds were retired off-tick.
- Remaining stalls in the run are pre-existing NPC contact/weapon-contact
  baseline stalls, not the geometry rebuild.

## map-geometry hot module — later same session

Added the first `map-geometry` module slice:

- New `src/modules/api/map-geometry-api.h` with a plain-data signature request.
- New `src/map/map-geometry-module.cpp`; hashes the host-canonicalized authored
  document with FNV-1a.
- Added `EVENT_MAP_GEOMETRY_SIGNATURE=800`.
- Added `ModuleHost::mapGeometrySignature()`.
- Added `authoredGeometrySignatureLive()` with revision + module-generation
  caching and host fallback.
- Server/client signature call sites now use the module when active.
- Catalog entry: `map-geometry`, `process_scope: "all"`.

Runtime evidence (`run 20261108_212140`):

- Server and client both activated `map-geometry` generation 1.
- Live funworld3 edit produced `map.geometry.signature`
  `18138557554130175710 -> 9942891213889549383`.
- `map.geometry.rebuild.queued` and async swap followed with no tick stall at
  queue or swap; `worker_ms=8783.01`.
- A source save in `map-geometry-module.cpp` produced
  `module.build.started` and `module.activated` generation 2 in the same server
  PID (`previous_generation=1`, code hash
  `97fe831bd5605cc8c98d3e5ee7e1de2e103ef54f7fc19b55263f28875511ef4d`).

## Build exclusivity, jobs, Build-time tracker, GLB v2 sidecar — later same session

- Added `devscripts/build_exclusive.py`: one EXE build enforced by the OS named
  mutex `Local\\MiMITA-EXE-Build`; `build.py` acquires it before compilers.
- `build_scheduler.py` resolves the coordinator root from
  `.dev/agent-lanes` or `MIMITA_COORDINATOR_ROOT`, so lanes share
  `<coordinator>/.dev/build-scheduler`.
- `agent-lane.py` propagates `MIMITA_COORDINATOR_ROOT`, raises lane jobs to 8,
  and restarts a lane dev-loop when its scheduler-root marker is stale.
- Dev-loop records its scheduler root under `.dev/build-scheduler-root.txt`.
- EXE build jobs default to 8, maximum 12; still one build process, never one
  compiler per source file.
- Added build telemetry:
  - `build/build-metrics.jsonl`
  - `build/build-timing.json`
  - elapsed, compiled/skipped, cold-build flag, jobs, lane, scheduler root.
- Moved authored GLB v2 runtime state and pristine per-object bases into the
  narrow `src/map/map-glb-authored.h` sidecar. `map_common.h` no longer carries
  `baseTransform`, base vertex/normal arrays, or authored-hidden flags.
- Renderer and collision read authored visibility/collision from the sidecar.
- Added `docs/regressions/performance/build-time-REG-2026-11-08.md` and indexed
  it in the performance regression README.
- Observed build time before this fix: roughly 20 minutes; target 2–3 minutes;
  ideal effectively 0 for normal module-owned work.

## GLB clone runtime — later same session

- Client `applyGLBObjectClones(World&)` duplicates source base vertices,
  normals, and batch material into the authored tail with a new stable clone id.
- Server `applyGLBObjectClones(HeadlessWorld&)` duplicates source raw triangles
  into a new collision solid with a new stable clone id.
- Source references use `source`; instance identity uses `instance_id`.
- Clone visibility/collision/deleted state is stored in the GLB sidecar.
- Live test (`run 20261108_232735_01`, funworld3, clone of `node:90` at +8 X):
  - server `map.glb-object.clone.created`: `clone=clone:node90:1`,
    `solid_id=157`, `triangle_count=12`, `triangle_first=42694`
  - client `map.glb-object.clone.created`: `clone=clone:node90:1`,
    `vertex_count=36`, `vertex_first=128088`
  - server async reload applied signature `17110681755022707211`.

## Verification claims

- Build evidence: lane dev-loop published the artifact above.
- Build metrics (`build/build-metrics.jsonl`):
  - cold rebuild: `compiled=236`, `skipped=387`, `jobs=8`,
    `elapsed_s=1070.877` (about 17.8 minutes).
  - no-op after warm-up: `compiled=0`, `elapsed_s=1.258`, no new artifact.
  - no-op scheduler root after the shared-root fix:
    `C:\mimita-v9\.dev\build-scheduler`.
- Build exclusivity test: a second process waited on
  `Local\MiMITA-EXE-Build` and observed `WAIT_TIMEOUT` (`0x102`) while the
  first held it.
- Runtime behavior: not exercised for the GLB v2 sidecar in this pass; build
  and static changes only. Human acceptance: not claimed.
- Performance status: the 20-minute cold rebuild remains far above the 2–3
  minute target. The regression stays open until a warm rebuild and future
  sidecar-only edits are measured.
