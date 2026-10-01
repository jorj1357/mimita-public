// 2026-10-01T19:18:40Z (display: 2026-10-01 15:18:40 EDT)
/* purpose
* Record the session that fixed the frame-time/perf reporting (real
* physics/render timers + numeric events.jsonl records) and pruned the
* entity-vs-world collision narrowphase with an AABB tree. The remaining
* collision-correctness and full local-space refactor items are tracked in
* docs/regressions/2026-10-01/physical-objects-collision-REG.md (Attempt 6).
*/

# Task

- Summary: Make fps/physics/render numbers truthful and searchable, and reduce
  holey-crate body-vs-world narrowphase cost.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`; base commit at session start (post `cce04d99`).
- Result states: build PASS; all four destruction selftests PASS; runtime
  frame-time acceptance NOT performed.

# Human feedback driving this pass

Grouped in the regression record. Highlights: perf_report said ~70 fps while
the game felt like 1 fps; per-subsystem ms did not add up; physics/logging not
in events.jsonl; fragments fall through the floor; crate goes into walls; weapon
hitboxes not unified; holey/settling crates cause large frame drops.

# Changes

## P0 — Truthful frame-time measurement + events.jsonl

- `src/perf/perf.h`/`perf.cpp`: added `PerfTimes.entityPhysics`,
  `destruction`, `simulation`; `"Simulation"` now maps to `simulation` (it used
  to be added to `physics` as well as the nested `"Physics"`, double-counting).
  `"PhysicsEntities"` and `"Destruction"` get their own fields.
- `src/engine/engine-tick.cpp`: the render stage is wrapped in
  `Perf::ScopedTimer("Rendering")` (it was only a spike-scope, so
  `PerfTimes.rendering` was always 0).
- `src/engine/engine-tick-combat.cpp`: `advanceKinematics` is wrapped in
  `Perf::ScopedTimer("PhysicsEntities")` so entity/destruction physics appears
  in the breakdown (it was only `MIMITA_PERF_SCOPE` and never summed).
- `src/perf/perf.cpp` `PERFORMANCE_FRAME` now emits numeric `fields`
  (`fps`, `frame_ms`, `budget_ms`, `physics_ms`, `entity_physics_ms`,
  `destruction_ms`, `simulation_ms`, `rendering_ms`, `networking_ms`,
  `combat_ms`, `npc_ms`, `npcs`, `effects`, `audio`, `projectiles`, `allocs`)
  at `Important` level.
- `config/debuglogger.json`: `performance.level` `off` -> `important`, so the
  frame records land in `logs/<date>/<run>/events.jsonl`.

Note on why the on-screen FPS disagreed: it is `FramePacer`'s previous-frame
wall time averaged over 120 frames (`frame-pacer.cpp:37,167`) and the graph
clamps each sample at 20 ms, so a multi-second stall barely moves the average.

## P1 — Entity-vs-world narrowphase pruning (partial WD)

- `src/physics/physical-entity.cpp` `advanceKinematics`: each collision pass now
  builds an `AabbTree` over its gathered world candidates and passes it to
  `collectActorMeshContactsInto`, so a body triangle tests only world triangles
  whose bounds it can reach instead of scanning the whole candidate list. This is
  the documented accelerated path (identical semantics) and is the main cost for
  holey crates (thousands of body triangles). A one-time-per-substep gather was
  tried first and reverted because it changed the settle behavior; the per-pass
  gather is preserved.

# Reasoning

- `docs/architecture/collision/collision.md` hard rules: cached/pruned
  broadphase, no per-query allocations, and the accelerated pair-directory path.
- The build environment cannot run the game, so the perf fix is: make the
  numbers real and searchable first, then act on them.

# Files changed

- `src/perf/perf.h`, `src/perf/perf.cpp`
- `src/engine/engine-tick.cpp`, `src/engine/engine-tick-combat.cpp`
- `src/physics/physical-entity.cpp`
- `config/debuglogger.json`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (Attempt 6
  appended)

# Pre-existing work (not authored this session)

`config/accounts/default.json` was already modified; left untouched. The
previous pass (terminal UI, settling, fragments, per-entity GPU cache, rounded
entity contacts) is committed.

# Documents and skills reviewed

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/destructible-world/destructible-world.md`
- `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`
- `docs/skills/efficiency-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md`

# Validation

- Build: `python build.py build-only` -> success; relinked `mimita.exe`.
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest` PASS.
- Cold-build debt: appended `Cold-build occurrence 40`.

# Not done this pass (open, in the regression record)

- Full local-space entity surface refactor (local triangles + local tree,
  transform the query) — the user approved it; deferred to a focused pass
  because it is a large, high-risk change and the per-pass gather + tree is a
  safe first step.
- Entity broadphase for `resolveEntityContacts`; deep-depenetration pass;
  weapon hitboxes as config triangles; settle-on-cylinder; the 0.5 s shoot
  freeze profile.

# Human review still needed

- Confirm the new `events.jsonl` `PERFORMANCE_FRAME` records show the real
  fps/physics/render ms during the drops.
- Re-test holey/settling crate interaction and fragment behavior.
