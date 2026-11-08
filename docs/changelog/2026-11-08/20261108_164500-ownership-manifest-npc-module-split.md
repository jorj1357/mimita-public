# Ownership manifest, single-owner NPC policy, consolidated network policy

## Changed

- Added `devscripts/module_ownership.py` as the single source of truth for
  module versus EXE source ownership. It expands `config/modules/modules.json`,
  builds the real EXE source inventory, detects source/event overlap, checks
  that each module has exactly one `<name>-module.cpp` entry, and reports
  `shared_with_host` migration entries.
- `build.py` now derives its `*.cpp` list from that manifest instead of walking
  `src/` and excluding `*-module.cpp` by name.
- The dev-loop cold watcher now excludes catalog-owned module sources (via the
  same manifest, cached by catalog mtime) instead of relying on the
  `-module.cpp` suffix. Editing a module-owned `.cpp` no longer queues an EXE
  build.
- Removed the dual compilation of `src/npc/npc-utility.cpp`. The policy file is
  module-owned; `src/npc/npc-utility-bridge.cpp` is the host adapter. It builds
  bounded requests from `UtilityContext`/`UtilityState`, calls the module, and
  applies the result. The host no longer scores goals, maps actions, or reasons
  about grenades itself.
- Extended the `npc-behavior` ABI with versioned goal-score, action-mapping,
  grenade-decision, and self-test events (500-505), including generation fields.
- Consolidated `network-snapshot-policy` and `network-reconnect-policy` into one
  cohesive `network-policy` module (events 700/701). Removed the two old entry
  files and switched host dispatches to catalog-owned `invokeEvent`.
- Added live movement-correction classification to `network-policy`
  (event 702). `classifyMovementCorrection` now asks the module for the
  none/small/medium/major decision and keeps the previous thresholds only as a
  no-generation fallback.
- Module builds now default to `-O2` and support a per-module `optimize` and
  `compile_flags` manifest field, so hot policy is not compiled slower than the
  host.
- Updated `docs/specs/hotreload/hotreload.md` and
  `docs/specs/dev-loop/dev-loop.md` to describe catalog-driven ownership and
  the module optimization rule.
- Appended `Cold-build occurrence 58` to the canonical cold-build record.

## Evidence

- Source/audit: `python devscripts/module_ownership.py .` and
  `python devscripts/module-build-worker.py --audit` both PASS with
  `module_sources=9 exe_sources=579`.
- Python syntax: `py_compile` passes for the changed build/dev-loop files.
- Targeted C++ syntax checks pass for `npc-utility.cpp`,
  `npc-utility-bridge.cpp`, `npc-behavior-module.cpp`,
  `network-policy-module.cpp`, and `module-host.cpp`.
- Build: `python build.py build-only` succeeded (`22` compiled, `599` skipped).
  The preceding dev-loop build failed at link with
  `undefined reference to scoreUtilityGoal`; the bridge and goal-score event
  resolved it.
- Published artifact: the dev loop published
  `2026-11-08T17:13:30.614560Z` with build ID
  `...:9e37a6dd8e52cb12e9ea05b478467c032ba5e4fb2a707d25f156ea1ff79c6ff8`.
  `nm` on that executable resolves `ModuleHost::npcBehaviorAction`,
  `npcGrenadeDecision`, and `networkMovementCorrection`, so the new host call
  sites are in the published binary.
- Module build: the module host produced `network-policy` generation 7 and
  `npc-behavior` generation 25 after the new ABI and policy sources. This is
  module compile/link evidence only.
- Journal: the connected client journal shows `module.build.started` and
  `module.activated` records for earlier generations; no NPC/network behavior
  acceptance was exercised in this session.

## Limits

- No new gameplay, combat, respawn, collision, or networking trial was run.
  The 240 FPS, 60 Hz, hit-agreement, and correction targets remain unproven.
- The final build was not launched as a dev-loop artifact, so same-PID behavior
  for the new NPC bridge and consolidated network module is not proven.
- The colliding agent edits present in the working tree
  (`multiplayer-*`, `server-gamemode*`, `server-npcs`, `recast-navigation`,
  analytics) were preserved and are not claimed by this changelog.
- Deep collision/network policy extraction (interpolation, reconciliation,
  hit-claim tolerance, solver steps) remains the next slice.

## Review

Routing, live-development, hot-reload, dev-loop, and logging rules were
consulted. The change is substrate and ownership work; it does not by itself
fix gameplay.
