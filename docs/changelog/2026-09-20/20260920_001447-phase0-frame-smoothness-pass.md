# Phase 0 frame-smoothness pass (S1)

- Task ID: S1 (v2.1.0 foundation, Phase 0)
- Status: CODE COMPLETE — build verified; runtime/frame-time evidence NOT yet observed
- Date, time, timezone: 2026-09-20T00:14:47Z
- Branch: `8292026stash`
- Base commit: `32d576e`
- Final commit: uncommitted (working tree)

# Pre-existing changes

- `git status --short` at session start already showed:
  - `M config/analytics.json`
  - `M config/audio/music-settings.json`
- These were not created or modified by this session and are preserved untouched.

# Requested behavior

Phase 0 of the v2.1.0 foundation plan: audit and repair the render/simulation
split so local walking/looking is smooth and jump input is immediate, while
keeping gameplay at fixed 60 Hz, VSync forced off, and remote interpolation
separate from authoritative movement. Keep the cached collision broadphase.

S1 scope was the frame-smoothness fixes from the read-only recon.

# Specification alignment

- Current specification paths:
  - `docs/specs/performance/performance.md`
  - `docs/architecture/collision/collision.md`
  - `docs/specs/debug-logging/debug-logging.md`
- Exact requirements:
  - performance.md: never run collision/physics outside the 60 Hz tick; VSync
    forced off; measure before optimizing.
  - collision.md: cached broadphase only; no `std::vector` returns in the hot
    path; `PhysicsScratch` for reusable buffers.
  - debug-logging.md: one logger, no raw printf debug architecture; sampling,
    throttling, and queues; crash-sensitive records flush promptly; hot-reloadable
    JSON authority.
- Why the change follows the specification:
  - Removed per-frame blocking disk reads (hot-source hashing, shader reload)
    and per-event `fflush`, which are blocking work on the gameplay/render thread
    (efficiency-checker priority 1).
  - Removed per-frame heap allocations in perf aggregation and the throttle
    logger, and throttled repeated warning spam.
  - Errors still flush synchronously, satisfying "critical records flush
    promptly".
- Conflicts or decisions:
  - Two hard-rule items were found and deliberately NOT changed this session
    because they materially change gameplay and need runtime acceptance (see
    "Still unverified"): projectile/grenade physics and weapon/NPC combat still
    run with frame `dt` outside the 60 Hz accumulator
    (`src/pobjects/persistent-physics.cpp:137-138`,
    `src/engine/engine-tick-combat.cpp:104/110/113`).
  - The collision by-value vector refactor touches ~35 call sites across 8 files
    and overlaps the Phase 1 contact-path redesign; deferred to S2 rather than
    rushed.

# Exact implementation changes

## File: `src/hot-reload/hot-reload-system.h`

- Added `hashSourceCached(relative)` declaration and a `SourceHashEntry`
  (mtime/size/hash) `mutable` cache `sourceHashCache_`.
- Reason: full-file SHA-256 of every hot source every poll ran on the game
  thread.

## File: `src/hot-reload/hot-reload-system.cpp`

- `computeSourceHash()` and `diffSourceHashes()` now call `hashSourceCached`
  and reserve the combined buffer. `hashSourceCached` stats size/mtime and only
  re-reads + SHA-256s a file when it changed.
- Old: `LiveCodeHash::sha256File(...)` per hot source every poll (`:1065-1096`).
- New: cached per-file hash keyed on mtime+size; watcher-dirty still forces the
  scan. `pollAndAdvance` behavior is unchanged (still compares the combined hash
  to `observedSourceHash_`), so live reload semantics are preserved.
- Why unrelated behavior is preserved: the final combined hash and the
  per-file diff results are identical; only the redundant reads are removed.

## File: `src/renderer/renderer.cpp`

- Added mtime/size stamp cache to `basicShaderContentHash()` so `shaders/basic.vert`
  and `.frag` are read + hashed only when a stamp changes.
- Old: two blocking `readTextFileQuiet` + FNV hash every frame via
  `pollShaderReload` (`engine-tick-render.cpp:317`).
- New: cheap `last_write_time`/`file_size` stat per frame; content read only on
  change. Live shader reload is preserved.
- Added `<cstdint>`, `<filesystem>` includes.

## File: `src/debug/structured-log.h` / `src/debug/structured-log.cpp`

- `writeLine(json, forceFlush=false)`; `flushEachEvent` default changed to `false`.
- The `bypassAggregate` path (Errors/Fatal/forced) now calls `writeLine(..., true)`,
  so critical records always flush.
- Old: `flush_each_event: true` caused `fflush` after every event on the frame
  thread.
- New: ordinary records rely on the existing bucket/window flush; errors flush.

## File: `config/debuglogger.json`

- `"flush_each_event": true` -> `false`. Errors still flush via the code path
  above.

## File: `src/engine/engine-tick-replay.cpp`

- Wrapped the per-frame config `pollReload()` set (GuiLayout, Killfeed, Lighting,
  Shadow, void-death, hitmarker, replay export/outro/hitmarker) in a 15-frame
  throttle. Each of these does a `last_write_time` stat.
- Old: ~9 filesystem stats every rendered frame (`:949-958`).

## File: `src/perf/perf-spike.cpp`

- `perfAggregateScopes` now reuses `static thread_local` buffers instead of
  allocating two vectors every frame (`childInclusiveSum`, `entries`).
- Old: two heap allocations per frame even when no spike.

## File: `src/debug/debug-log.cpp`

- `Debug::logThrottled` throttle map changed from `std::unordered_map` to
  `std::map<std::string,double,std::less<>>` and looked up via `std::string_view`,
  so no `std::string` is constructed on the already-seen-key path.
- Added `<map>`, `<string_view>` includes.

## File: `src/physics/movement/physics-collision-glb-main.cpp`

- `trackGrowth` growth warnings converted from `Debug::warn` (emitted every frame
  while the counter grows) to `Debug::logThrottled(..., 1.0f)`.

# Diagnostics

- Owner/category: performance / collision / debug logging.
- Input: rendered frames, hot-source polls, config polls, collision frame diag.
- Decision: cache by mtime+size; throttle stat/warn storms; force-flush only
  critical records.
- Rate limiting: config poll 15 frames (~0.25 s); growth warnings 1.0 s.
- Failure or rejection reason: none; build succeeded.

# Validation

- Focused skill paths and results:
  - `docs/skills/efficiency-checker-v1.md` — findings matched the recon list;
    each change removes repeated per-frame work or per-frame allocation.
- Tests and exact commands: `python build_agent.py` (canonical agent build).
- Build status: `Status: SUCCESS`, `mimita-20260919T200941.exe`, return code 0,
  295 compiled, 298.46 s.
- Runtime or hot-reload evidence: NONE observed this session.
- Output files: `build/changelog.txt`, `build/build-result.json`,
  `mimita-20260919T200941.exe`.

# Measured evidence

- Before values: not measured at runtime this session. The recon identified the
  cost sources by code path only.
- After values: not measured.
- Timestamps: build finished 2026-09-19T20:14:40 local (2026-09-20T00:14:40Z).
- Tick/frame/network measurements: not collected.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: no behavior regression has been
  observed; build only. Changes preserve output hashes/behavior by design.
- Related regression paths: none.

# Remaining Phase 0 items (not done; need runtime/decision)

- Quantize `PersistentPhysicsSystem::update` and weapon/NPC combat to the fixed
  60 Hz accumulator — material behavior change, needs human acceptance and
  server/client parity review.
- Collision `std::vector`-by-value hot-path refactor (~35 sites); belongs with
  the Phase 1 single contact path.
- Unthrottled raw `printf` in `engine-tick-net.cpp:525/696/706` — left because
  log greppers may depend on `[NET SHOT ...]` output.
- `CHECK_COLL_SPIKE` (`physics-collision-glb-main.cpp:52-63`) still fires when a
  stage is consistently over threshold; not yet throttled.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: local walking/looking smoothness, jump immediacy, no stutter,
  remote interpolation stability, frame-time before/after.

# Related feature record

- Feature path: none yet (Phase 0 is unfeatured; `docs/features/` has no
  performance record).
