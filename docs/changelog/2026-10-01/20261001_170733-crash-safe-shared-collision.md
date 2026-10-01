// 2026-10-01T17:07:33Z (display: 2026-10-01 13:07:33 EDT)
/* purpose
* Record the session that (1) made destruction crash-safe and diagnosable,
* (2) replaced the coarse entity-vs-entity AABB response with the shared
* triangle collision path, (3) added projectile momentum/torque to crates,
* (4) moved destruction budgets into config, and (5) added numbered GLB object
* spawning. Phase 5 (frame-time pass) and Phase 9 (other weapons) remain.
*/

# Task

- Summary: Crash-safe + diagnosable destruction; shared triangle collision for
  all moving entities; projectile momentum/torque; config-driven destruction
  budgets; numbered GLB object spawning.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`; base commit `10e17e3c`.
- Result states: build PASS; all destruction selftests PASS; crash diagnostics
  validated; runtime two-client visual + frame-time acceptance NOT performed.

# Phase 1 — Crash-safe, diagnosable destruction

## What the 11:36 crash actually is

`0x20474343` is `STATUS_GCC_THROW`, the MinGW/GCC C++ exception code raised by
`_Unwind_RaiseException` through `KERNELBASE!RaiseException`. The old report
named only `KERNELBASE.dll` because `crash-handler.cpp` recorded only the
`RaiseException` frame and had no terminate handler or stack symbolication. The
exact failing function is still unknown, so the crash stays `UNRESOLVED`
(see `docs/regressions/2026-10-01/destruction-uncaught-exception-REG.md`).

## Crash diagnostics

File: `src/debug/crash-handler.{h,cpp}`
- Added an allocation-free breadcrumb ring `recordCrashBreadcrumb(subsystem,
  fmt, ...)` and dumped it into every text report ("Recent activity").
- Added a `std::terminate` handler capturing
  `abi::__cxa_current_exception_type()` + `std::current_exception()` `what()`,
  with `std::rethrow_exception` (not a bare `throw;`, which recursed).
- Added best-effort symbolized stacks (`StackWalk64` + `SymFromAddr` +
  `__cxa_demangle`) with a `module+offset (link 0x...)` fallback so MinGW DWARF
  frames resolve offline: `addr2line -e mimita.exe 0x<link address>`.
- Recognized `STATUS_GCC_THROW` as `CXX_EXCEPTION_GCC` instead of `UNKNOWN` and
  added an explanatory note.
- Breadcrumbs added at: impact submit/reject, flush, boolean rebuild
  (start/success/rejection/exception), fracture decompose, entity add/remove,
  render-buffer upload, projectile entity hit, replicated cut apply.

File: `src/game/game-cli.cpp`
- Added `--crash-exception-selftest` (installs the handler, throws an uncaught
  exception, suppresses the dialog) to validate the report. Example output:
  `Exception: CXX_EXCEPTION_GCC (0x20474343)`, the recorded breadcrumb, and a
  stack with `link 0x140...` addresses that `addr2line` resolves
  (`terminateHandler`, `main`, `handleGameCLI`, ...).

## Crash-safe rejection (keep the previous valid mesh)

File: `src/impact/destructible-geometry.cpp`
- Fixed the confirmed `addCut` double-rollback: `rebuild` already rolls back a
  failed pending cut via `discardPending`, and `addCut` popped a *valid* cut a
  second time. It now returns 0 without popping.
- Guarded `rebuild`'s `cuts.size() - pendingCutCount` underflow.
- Added `meshIsSane` (non-empty, triangle-aligned, indices in range, finite
  vertices) and a finite/positive volume check; a bad boolean result is
  rejected and the previous valid mesh is kept.
- Wrapped `booleanSubtractIncremental` and `booleanDecomposePieces` in
  try/catch that logs, breadcrumbs, and rolls back to the last valid revision.
- Bounds-checked `fillSurface` indices as a last-resort guard.

File: `src/impact/impact-system.cpp`
- `submit` rejects non-finite `ImpactEvent` inputs before any geometry math.

File: `src/network/multiplayer-physical-entities.cpp`
- Reject non-finite spawn fields (half extents, position, orientation,
  velocity, angular velocity, density) and non-finite/unsafe cut fields before
  they reach the boolean/mass code.

## Stress test

File: `src/impact/destructible-selftest.cpp` + `src/game/game-cli.cpp`
- Added `--destruction-stress-selftest`: 4 rounds x 120 deferred projectile
  shots on one crater (real `ImpactSystem::submit` + `advanceKinematics` flush),
  forced motion, 6x dumbbell fracture, and a client mirror reproducing the
  server cut mesh. Asserts finite state, non-shrinking history, bounded entity
  count, tunneling, and matching client geometry. PASS 5/5 consecutive runs.

# Phase 2 — One shared triangle collision path

File: `src/physics/physical-entity.cpp`
- Replaced the AABB-only `resolveEntityContacts` outright. Each movable pair
  now runs the shared actor-vs-mesh narrowphase
  (`collectActorMeshContactsInto`) in both directions, reusing each entity's
  `cachedEntitySurface` world triangles + `AabbTree` (swapped into a scratch
  `World`, no triangle copy, no per-query allocation). Normals are oriented
  toward the first body; duplicate reverse contacts are dropped.
- Added `resolveEntityPairContact`: positional correction split by inverse mass,
  and a normal + Coulomb friction impulse at the contact point with an
  effective-mass term, so both bodies gain translation and torque.
- The manifold is aggregated into one representative contact (average normal,
  central point, max penetration) so a multi-triangle box face applies a single
  impulse and exchanges momentum correctly; per-triangle sequential impulses
  under-exchanged momentum.
- Crates/fragments are `PhysicalEntity` movable bodies, so this covers
  crate-vs-crate, fragment-vs-world (via the existing entity-vs-world sweep),
  fragment-vs-fragment, and player/NPC-vs-crate (via `collectActorEntityContacts`).

Test: `--moving-crate-selftest` "dynamic crates exchange momentum" and "do not
remain overlapped" PASS. All other selftests unchanged.

# Phase 3 — Projectile momentum and crate response

File: `src/physics/physical-entity.{h,cpp}`
- Exposed `applyPhysicalEntityImpulse(entity, impulse, worldPoint)` (wraps the
  existing `applyImpulseAtPoint`, which computes torque from the COM offset).

File: `src/impact/impact-system.cpp`
- `submit` now transfers the projectile's momentum to a Dynamic target at the
  real `worldPoint`, reduced by the impact angle and `material.holeEnergyScale`.
  Deterministic from the event, so server + local prediction agree; mirrors get
  the resulting transform through the state packet. No teleporting.
- New `--moving-crate-selftest` case 7 asserts the crate is pushed along the
  shot, spins from an off-center hit, and is not teleported.

# Phase 4 — Config-driven destruction budgets

Files: `src/impact/destructible-world-config.{h,cpp}` (new), `config/destructible-world.json`
- One owner `DestructibleWorldConfig` (mirrors `MaterialConfig`: load +
  `pollReload`, malformed JSON keeps the previous values) owns
  `maxCutsPerEntityPerTick`, `cutBudgetMsPerTick`, `maxTrianglesPerEntity`, and
  the `FractureTuning`.
- `PhysicalEntitySystem::advanceKinematics` uses the config for the flush caps;
  `ImpactSystem::flushPendingCuts` applies the max-triangles cap and fracture
  tuning to the geometry owner each flush; `engine-tick-setup.cpp` polls reload.
- `config/destructible-world.json` now holds real JSON (comments allowed).
  Defaults equal the previous hardcoded constants.

# Phase 8 — Arbitrary GLB objects, numbered spawn

File: `src/terminal/object-commands.{h,cpp}`
- Added `object_spawn_list`: scans `assets/objects/things/physics-objects`,
  keeps only `.glb`, sorts alphabetically, prints 1..N.
- `object_spawn <n>` spawns the nth entry in front of the player; path-based
  `object_spawn <glb-path> ...` keeps working. Same import/validate/physics path.
- `listPhysicsObjectGlbs()` exposed and checked by `--destructible-selftest`
  (non-empty, `.glb` only, sorted). Current folder has 5 GLBs.

# Reasoning

- Spec alignment: `destructible-world.md` sections 17/19 (penetration,
  `subtractCapsule`), 20/21 (fracture), 44 (explicit budgets), and 45/46
  (bounded work). Collision rules: `docs/architecture/collision/collision.md`
  (fixed 60 Hz, cached broadphase, no per-query allocation in hot loops).
- The AABB entity response was the documented gap
  (`physical-entity.h` TODO-DELETE Phase 2); it is now the shared triangle path.
- Smallest correct changes; no collision universe added; the same narrowphase
  routine is reused, not duplicated.

# Files changed and exact old/new content

- `src/physics/physical-entity.cpp` `resolveEntityContacts`: old body was the
  AABB overlap/min-axis response; new body collects triangle contacts both
  directions and applies `resolveEntityPairContact`.
- `src/impact/destructible-geometry.cpp` `addCut`: removed
  `geometry.cuts.pop_back();` (and the `--pendingCutCount`) on failure.
- `src/impact/destructible-geometry.cpp` `rebuild`: clamped `pendingCutCount`;
  added sanity/finite checks + try/catch.
- `src/debug/crash-handler.cpp`: added breadcrumbs, terminate handler,
  symbolication, `STATUS_GCC_THROW` labeling.
- `src/impact/impact-system.cpp` `submit`: added finite-event rejection and the
  momentum impulse; `flushPendingCuts`: applies config caps/tuning.
- `src/terminal/object-commands.cpp`: added `object_spawn_list` and index
  resolution in `object_spawn`.

# Pre-existing work (not authored this session)

The prior session's destructible/penetration work was committed by the human as
`10e17e3c`. Untracked assets (`assets/sound/weapon/...`) are the human's and were
left untouched. `src/impact/impact-system.cpp` had the prior bore-walk; this
session extended it.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/destructible-world/destructible-world.md` (17, 19, 20, 21, 44)
- `docs/architecture/collision/collision.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/regressions/README.md`

# Validation

- Build: `python build.py build-only` -> success; relinked `mimita.exe`.
- `mimita.exe --destructible-selftest` PASS (incl. test 15b spawn list,
  tests 1-23).
- `mimita.exe --moving-crate-selftest` PASS (incl. new projectile momentum/torque
  case and the shared-collision momentum exchange).
- `mimita.exe --destruction-replication-selftest` PASS.
- `mimita.exe --destruction-stress-selftest` PASS 5/5.
- `mimita.exe --crash-exception-selftest` writes a labeled report with
  breadcrumbs and a resolvable stack.
- Cold-build debt: appended `Cold-build occurrence 38`.

# Specification/behavior review result

No new spec-code disagreement introduced. `ImpactSource::Hitscan/Melee/
Explosion` remain declared but unimplemented (Phase 9). Frame-time target
(<4 ms) is not measured in this environment (Phase 5).

# Human review still needed

- Reproduce or confirm the 11:36 crash is gone; if it recurs, the new
  `crash-*.txt` names the subsystem via breadcrumbs and gives a resolvable
  stack. The crash stays `UNRESOLVED` until then.
- Two-client visual: same mesh/revision/fragments; crate push and spin look
  right.
- Frame time while shooting a crate (`perf_top`/`perf_file_logging`), and bursts.
- Crate resting on flat/edges/curved geometry; no obvious wrong-pose freeze.
- `object_spawn_list` and `object_spawn <n>` in a live session; arbitrary GLB cut.
- Retune `config/destructible-world.json` and material cut sizes after play.

# Remaining phases (not done this session)

- Phase 5 (performance pass): measure whole-frame/gameplay/collision/boolean/
  render separately and act on hotspots. Existing `bool=...ms` logging and
  config budgets are in place; no dedicated timers added.
- Phase 9 (other weapons): route hitscan/shotgun/explosion/melee through
  `ImpactSystem` as `ImpactSource::Hitscan/Explosion/Melee`. Explosions and
  melee currently create no destructible cuts.
- Phase 7 (textures): intentionally deferred.
