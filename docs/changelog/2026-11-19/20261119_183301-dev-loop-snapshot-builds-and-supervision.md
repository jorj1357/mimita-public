# Dev-loop snapshot builds and build supervision

# Task

- Task ID: session-20261119-dev-loop-snapshot
- Session ID: session-20261119-dev-loop-snapshot
- Owner: opencode
- Focus: Stop concurrent saves from aborting builds, free disk, and supervise the
  whole-game DLL worker so it cannot hang or fail silently.
- Status: implemented and runtime-validated for snapshot builds and incremental
  rebuild; auto-retry and the per-agent runtime registry remain open.
- Date, time, timezone: 2026-11-19T18:33:01+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85 (working tree; not committed)

# Evidence before change

- Repeated `initial_source_coherence` / `coherence_validation` failures with
  `changed during build: <path>` whenever another session saved during a build.
  The worker compiled the live tree and discarded the staged DLL on any change
  (`game_build_worker.py:974-981`, `1599-1623`).
- `C:` had 0.10 GB free. `build/game` held 156 generations (20.59 GB) with no
  retention; `logs` held 16.81 GB.
- `_run_checked` waited on `process.wait()` with no timeout
  (`dev_loop/game_build.py`), the game-DLL build did not acquire the
  machine-wide build slot, and `manifest_snapshot` ran in the daemon thread.
- A generation-369 build published at 18:18:18 UTC while a source file was
  edited after the snapshot; the daemon then wedged once until restarted.

# Specification alignment

- Root dev-loop remains the only build/run authority.
- Fixed 60 Hz, roster, and gameplay rules are untouched; this is build
  infrastructure only.
- No second worktree or lane was created; the change keeps one shared checkout.

# Exact implementation changes

- `devscripts/game_build_worker.py`:
  - New `--source-root` plus module-level `SOURCE_ROOT` (defaults to `ROOT`).
  - Compiles snapshot translation units with `cwd=SOURCE_ROOT` and relative
    source/include paths, so dependency files stay relative.
  - Cache identity (`flags_identity`) is computed from the historical logical
    `-I{ROOT}/...` paths while the real command uses the snapshot include dirs.
    This keeps the existing object-cache directory and invalidates nothing.
  - Headers are isolated: every include resolves inside the snapshot. Only
    `src/pch.h`/`pch.h.gch` remain the repository copy (chosen tradeoff), so a
    mid-build header edit is excluded from that generation and queued next.
  - Coherence checks use `SOURCE_ROOT`; snapshot dependency paths map back to
    the live tree for cache freshness.
  - `glad.c` resolves against the snapshot.
- `devscripts/dev_loop/game_build.py`:
  - Creates `.dev/build-snapshots/<attempt>/` from `src/`, `include/`, and
    `external/` (game sources live in `src/` and `external/recastnavigation`),
    then verifies the manifest hashes. Up to `MIMITA_SNAPSHOT_ATTEMPTS` (default
    3) manifest+copy retries; a mid-build save can no longer invalidate the
    build.
  - Deletes the snapshot in a `finally` unless `MIMITA_KEEP_BUILD_SNAPSHOT=1`.
  - Disk guard: `MIMITA_GAME_BUILD_MIN_FREE_MB` (default 2048); prunes first,
    then fails with `disk_low` instead of crashing.
  - Retention: keeps `ready.json`/`current.json` + 5 newest generations; prunes
    dated `logs/` beyond 3 days at build start.
  - `_run_checked` timeout with `taskkill /T` and a 124 exit code
    (`MIMITA_GAME_BUILD_TIMEOUT`, default 3600 s).
  - Acquires the machine-wide `module` build slot
    (`MIMITA_GAME_BUILD_SLOT_TIMEOUT`, default 600 s) around the worker.
- `devscripts/prune-builds.py`: manual retention tool (5 generations, 3 log
  days, `--dry-run`).
- `devscripts/agent-runtime.py`: standalone per-agent runtime launcher from the
  same `.dev/current/mimita.exe` and `build/game/ready.json` generation. Each
  runtime gets its own launch mode, map, UDP port, room file, run directory, and
  server/client `events.jsonl`; it launches a client after the server publishes
  its gameplay port. Commands: `start`, `stop`, `status`, `list`.
- `devscripts/dev_loop/runtimes.py`: dev-loop-native `RuntimeManager`. The
  dev-loop owns `start-runtime` / `stop-runtime` / `reload-runtime` /
  `list-runtimes`, stores state under `.dev/runtimes/<id>.json`, pins the game
  DLL generation at launch, and exposes every runtime in `--status`,
  `--list-instances`, and the dashboard. `reload-runtime` re-launches on the
  newest generation.
- `devscripts/dev-loop.py`, `devscripts/dev_loop/cli.py`: runtime control
  actions and CLI flags (`--runtime`, `--runtime-launch-mode`,
  `--runtime-profile`, `--runtime-map`, `--no-runtime-client`).
- `devscripts/dev_loop/gui.py`: collapsible dashboard sections (BUILD, queue,
  checkouts, processes, RUNTIMES, loop output) with `[-]`/`[+]` toggles,
  persisted state in `.dev/dashboard-ui.json`, and a RUNTIMES section with
  Start / Stop / Reload.
- `docs/specs/dev-loop/dev-loop.md` and
  `docs/specs/dev-loop/dashboard.md`: documented the native runtime workflow,
  header-isolated snapshot builds, and collapsible sections.

# Build and validation

- Ran `python devscripts/prune-builds.py`; freed 18.8 GB (free space went from
  0.10 GB to 21.7 GB; later 23.0 GB). Locked in-use DLL generations were
  skipped.
- Restarted the dev-loop to load the new harness.
- Build `game-dll-20261119T180134725382Z-bd7c8b` published generation 369 at
  18:18:18 UTC. `src/npc/npc-nav-graph.cpp` was edited **after** the snapshot
  while the build was compiling; the build still published (no coherence
  abort).
- The follow-up build `game-dll-20261119T182840464577Z-86fb1b` published
  generation 370 in **74.2 s** (incremental; warm object cache).
- `python -m py_compile` passed for the edited scripts.
- Ran `agent-runtime.py start perf-c --launch-mode 9`: server PID 31336 and
  client PID 23144 on port 58406, mode `juggernaut`, map `dust2cyberiav4`,
  generation 370. The server journal reported 67 NPCs, 66 moved, and ~51 fixed
  ticks/s. The runtime was stopped afterward.
- Build `game-dll-20261119T193258940268Z-e6c35a` restored the cache identity:
  object cache `build/game/objects/b10e3a31b3ae8637`, **12 compiled / 630
  skipped**, generation 372. No full rebuild.
- Header isolation: build `game-dll-20261119T193842148564Z-05949f` took its
  snapshot, then `src/npc/npc-nav-graph.h` was edited during compile. The build
  still published generation 373 with no coherence failure; the header edit
  queued the next generation (`bf300e`).
- Native registry: `--request start-runtime --runtime test-a
  --runtime-launch-mode 9` launched server PID 4808 and client PID 25404 on
  port 57777, pinned to generation 371. `--status` and `--list-instances`
  showed the runtime; the journal reported 67 NPCs and 64 moved at ~55 ticks/s.
  `--request stop-runtime` stopped it.
- The Tk dashboard was restarted with the new module; no stderr startup errors.
  The layout/toggle behavior is code-validated but not visually accepted.

# Implementation update (2026-11-19 later)

## Incremental build-source mirror

- Replaced the per-build source snapshot with one persistent
  `.dev/build-source/` mirror. The first update populates it (~20.5 s); every
  later build diffs the manifest and copies only changed/added files, then
  hashes only those. Measured incremental mirror update: **0.024 s**.
- Build `game-dll-20261119T205320300003Z-7cc9cb` (gen 381) completed with the
  mirror after a header-driven rebuild; total 1047 s under heavy concurrent
  compilation (305 compiled / 337 skipped), versus 283 s for the old snapshot.
  The mirror removes the 150 s copy/verify phase; compile/link dominate.

## Arcade collision mode (gen 381, compiled)

- `GamemodeFeatures.arcadeCollision` parsed from `features.arcade_collision`;
  Juggernaut sets it true and leaves `features.destructible_world` null. The
  masterlist declares `arcade_collision` optional, and the config validator now
  honors `required: false` for nested fields instead of requiring every mode to
  declare every optional feature.
- `CollisionConfig` gained a runtime `arcadeCollision` flag; the server sets it
  from the active gamemode at the start of player/world simulation, and the
  client sets it in `simulateTick`.
- Arcade skips the actor-triangle/body/weapon collision pass and forces the
  capsule path; the server-side animated physical-contact pose/clash pass
  returns early (local client prediction still runs so it can send claims).
- NPC damage classification falls back to the shared limb template scaled by
  `sizeScale` when an NPC has no collision body parts, giving per-limb zones
  without body-part collision.
- Weapon hit defaults: headshot 1.5x, limb 0.75x, torso 1.0x; explicit
  per-weapon overrides remain.

## Roster blocker root cause (config fixed, runtime pending)

- Runs `20261119_211818` and `20261120_000300` reported `npcs=0`,
  `gamemode.match.configured` with `teams:0`, and
  `gamemode.npc-spawn.policy` max 0. The server journal `20261120_000300`
  recorded `config.gamemode.invalid` for `features.arcade_collision: unknown
  setting` and `features.destructible_world: expected boolean`. The invalid
  Juggernaut file left the active gamemode with no teams or roster.
- Fixed `config/gamemodes/juggernaut.json` (`destructible_world` back to null),
  added the optional `arcade_collision` field to
  `config/gamemodes/masterlist/gamemode-masterlist.json`, and changed
  `src/gamemode/gamemode.cpp` so nested schema fields with
  `required: false` may be omitted. Build note `839aeab0c1aa` is queued for the
  validator change.
- The live server reloaded the corrected Juggernaut file, but the already
  configured empty match still needs a fresh server start. Runtime validation
  is pending because the root dev-loop has a pending/active EXE build and its
  control endpoint became unavailable; no second loop was started.
- Not yet implemented: background nav chunk builder, CPU affinity/core
  reservation, snapshot send-rate decoupling, claimed-melee end-to-end, and
  the 100-actor stress roster. No 60 Hz acceptance is claimed.

# Remaining work

- Re-run mode 9 after the pending EXE build completes and the dev-loop control
  endpoint recovers, verify the restored 67-NPC roster, then re-baseline arcade
  tick rate and physical-contact timing.
- Background nav chunk builder to remove the 0.5-1.0 s `chunk_build_ms` stalls.
- CPU affinity: one reserved core per server tick, client on another, builds on
  the rest at high priority.
- Snapshot send-rate decoupling (test 20 vs 30 Hz) while damage stays 60 Hz.
- Claimed-melee end-to-end validation and the 100-actor stress run.
- Bounded auto-retry for environmental build failures (worker timeout, slot
  timeout, disk_low) while the source revision is unchanged.
- Build queue summary/visibility polish.
- Human gameplay acceptance, including the collapsible dashboard layout, was not
  performed and is not claimed.
- Concurrent-editor changes are preserved and not claimed.
