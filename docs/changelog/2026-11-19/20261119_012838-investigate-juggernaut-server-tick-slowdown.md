// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Investigate Juggernaut server tick slowdown

# Task

- Task ID: session-20261119-012837
- Session ID: session-20261119-012837
- Owner: server NPC simulation and navigation performance
- Focus: Juggernaut dev-loop run 20261119_012255, NPC workload, actual server Hz, and movement evidence
- Summary: Inspected the active dev-loop Juggernaut server journal. Confirmed severe NPC simulation overruns and attributed measured time chiefly to NPC physics and collision, with intermittent navigation spikes.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-19T01:28:38+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 2ec7671ce73a94c28e9687dca602ab1b0291a1e5
- Final commit: 2ec7671ce73a94c28e9687dca602ab1b0291a1e5

# Pre-existing changes

- Exact status output: working tree already had 17 modified/untracked paths before this investigation, including user edits in `src/network/server-npcs.cpp` and `src/npc/npc-combat.cpp`. They were preserved. The only added/modified session files are this changelog and the append-only regression occurrence below.
- Files not created or modified by this session: all other paths in the captured git status.

# Requested behavior

Investigate why Juggernaut mode's dev-loop server runs below 60 Hz with NPCs moving, and identify the measured work that must be improved while retaining authoritative movement and physics for every NPC.

# Specification alignment

- Current specification paths: `docs/specs/performance/performance.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, and `docs/specs/gamemodes/gamemodes.md`.
- Exact requirements: server acceptance is 99.99% of fixed ticks at or below 16.667 ms; NPC sensing, targeting, navigation replanning, and decisions may be staggered, but movement, collision, damage, and authoritative physics remain on every fixed 60 Hz tick.
- Why the investigation follows the specification: it used the real dev-loop process's canonical server journal and measured existing timing events by their owning scopes.
- Conflicts or decisions: none. No simulation cadence was lowered and no source behavior was changed.

# Exact implementation changes

## File: no source file changed

- Lines/functions/headings: investigation traced `runServer()` timings in `src/network/server.cpp:495,625,1595`; `NpcSystem::updateOneNpc()` in `src/npc/npc.cpp:1545`, with `npc.slow-update` and `npc.phase-window` emitted at lines 3781 and 3872.
- Old content or behavior: existing runtime had the same severe tick overruns; no source was modified.
- New content or behavior: appended the 2026-11-19 runtime occurrence and measured owner attribution to `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md:169`.
- Reason: retain the new confirmed runtime evidence with the existing unresolved server-tick regression.
- Why unrelated behavior is preserved: pre-existing source/config/docs edits were not modified.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/operations/devscripts/script-inventory.md` |  |  |  | git checkout -- docs/operations/devscripts/script-inventory.md |
| `docs/specs/creators/monster-authoring.md` |  |  |  | git checkout -- docs/specs/creators/monster-authoring.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/gamemodes/bloodtest.md` |  |  |  | git checkout -- docs/specs/gamemodes/bloodtest.md |
| `docs/specs/movement/movement.md` |  |  |  | git checkout -- docs/specs/movement/movement.md |
| `docs/specs/procedural-infinite-world/procedural-infinite-world.md` |  |  |  | git checkout -- docs/specs/procedural-infinite-world/procedural-infinite-world.md |
| `src/network/multiplayer-shots.cpp` |  |  |  | git checkout -- src/network/multiplayer-shots.cpp |
| `src/network/server-npcs.cpp` |  |  |  | git checkout -- src/network/server-npcs.cpp |
| `src/npc/npc-combat.cpp` |  |  |  | git checkout -- src/npc/npc-combat.cpp |
| `docs/changelog/2026-11-19/20261119_011946-infinite-xxity-editor-and-performance-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_011946-infinite-xxity-editor-and-performance-evidence.md' |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results: `docs/skills/efficiency-checker-v1.md` — confirmed owners/costs only, no speculative rewrite; `docs/skills/spec-behavior-review-v1.md` — fixed-60-Hz requirement aligned, no spec conflict.
- Tests and exact commands: no tests added or run. Read-only commands: `python devscripts/dev-loop.py --status`; `python devscripts/dev-loop.py --list-instances`; journal summaries from the run's server `events.jsonl`.
- Build status: no build requested. Dev-loop status reported published EXE build `2026-11-19T00:53:35.016823Z:eabcdcd2...`; current runtime NPC module generation was 345. Status also reported `game-build status=failed generation=345 detail=codegen check failed: packet_registry_codegen.py`, so this investigation does not claim the current dirty source edits were compiled or loaded.
- Runtime or hot-reload evidence: real active dev-loop run `20261119_012255`; server PID 20664, client PID 18796; server journal `C:\mimita-v9\logs\2026-11-19\20261119_012255\server-01\events.jsonl`; run identity mode Juggernaut/map `dust2cyberiav4`. Runtime journal reports 67 NPCs despite the dev-loop launch label saying `npcs=0`.
- Output files: this changelog and the appended regression occurrence.

# Measured evidence

- Before values: 229 `performance.server-loop-window` samples; 183/229 below 59.9 Hz, median 45.25 Hz, worst 0.42 Hz. `performance.server-tick-stall` reached 2,489 ms total and 2,187 ms NPC work. NPC phase windows recorded up to 632 ms physics, 428 ms collision, 281.5 ms navigation, and 216.9 ms path work. Individual NPC samples reached 82 ms in physics/collision and 75.8 ms in navigation.
- After values: not applicable; no code change or before/after fix trial was made.
- Timestamps: run launched `2026-11-19T01:22:55.678Z`; inspected through `2026-11-19T01:28:14Z`.
- Tick/frame/network measurements: NPC movement decisions recorded nonzero distances, so actors were moving; repeated stuck/recovery records also exist. The runtime failed the 60 Hz target substantially and had severe >100 ms tick stalls.

# Regression review

- Regression entry appended: yes — new confirmed occurrence appended to the existing server tick budget regression.
- Why this is or is not a confirmed regression: live server timing directly shows repeated fixed-tick and loop overruns under Juggernaut's 67-NPC workload.
- Related regression paths: `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`; `docs/regressions/performance/fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: movement is evidenced by journal distance fields; human visual/feel acceptance was not performed.
- Multiplayer review: one client only; not performed.
- Still unverified: exact inner collision operation causing the physics spikes, and any source fix's loaded/runtime effect.

# Agent handoff context

- Owner: server NPC simulation and navigation performance
- Focus: Juggernaut dev-loop run 20261119_012255, NPC workload, actual server Hz, and movement evidence
- Files to check: `src/npc/npc.cpp` (`NpcSystem::updateOneNpc()` and shared movement/collision calls); `src/network/server.cpp` tick budget owner; exact run journal named above.
- What another agent should not redo: the baseline diagnosis that 67 NPCs overrun the server and that measured costs are chiefly physics/collision with intermittent navigation/path spikes.
- Next exact step: inspect the inner shared collision/movement calls that contribute to `npc.slow-update.physics_ms` / `collision_ms`, attribute the variable 20–82 ms actor updates, then optimize that owner and rerun the same map/mode/67-NPC workload through the existing dev-loop. Preserve per-NPC movement and collision on every fixed tick; require stable 59.9–60.1 Hz and tick-budget evidence before claiming success.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 2ec7671ce73a94c28e9687dca602ab1b0291a1e5 / 2ec7671ce73a94c28e9687dca602ab1b0291a1e5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-19T00:53:35.016823Z", "latest_successful_build_id": "2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "a124a1ee639728eb55c7c097d248fdf90e0fa548ffd237ac6049c8afe36b8f18", "dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "published_utc": "2026-11-19T00:53:18.768Z", "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 9.709, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-19T00:53:32.510Z"}}
- Generation: 345
- Artifact: build\game\generation-345\game-g345.dll
- Journal path: C:\mimita-v9\logs\2026-11-19\20261119_012255\client-01\events.jsonl
- Session ID: session-20261119-012837

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M config/accounts/default.json
 M config/analytics.json
 m devscripts/blender-ai
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/game_build_worker.py
 M docs/ROUTER.md
 M docs/operations/devscripts/script-inventory.md
 M docs/specs/creators/monster-authoring.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/gamemodes/bloodtest.md
 M docs/specs/movement/movement.md
 M docs/specs/procedural-infinite-world/procedural-infinite-world.md
 M src/network/multiplayer-shots.cpp
 M src/network/server-npcs.cpp
 M src/npc/npc-combat.cpp
?? docs/changelog/2026-11-19/20261119_011946-infinite-xxity-editor-and-performance-evidence.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/game_build.py                  |  47 ++++++-
 devscripts/dev_loop/gui.py                         |  23 +++-
 devscripts/game_build_worker.py                    |  97 ++++++++++++--
 docs/ROUTER.md                                     |   1 +
 docs/operations/devscripts/script-inventory.md     |   1 +
 docs/specs/creators/monster-authoring.md           | 134 ++++++++++++++++++-
 docs/specs/dev-loop/build-time-observability.md    |  77 ++++++++---
 docs/specs/dev-loop/dashboard.md                   |  32 +++--
 docs/specs/gamemodes/bloodtest.md                  | 119 ++++++++++++++++-
 docs/specs/movement/movement.md                    |  18 ++-
 .../procedural-infinite-world.md                   | 107 +++++++++++++++
 src/network/multiplayer-shots.cpp                  |  46 +++++++
 src/network/server-npcs.cpp                        | 148 ++++++++++++++++++---
 src/npc/npc-combat.cpp                             |  38 +++++-
 17 files changed, 808 insertions(+), 84 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	devscripts/blender-ai
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/game_build_worker.py
M	docs/ROUTER.md
M	docs/operations/devscripts/script-inventory.md
M	docs/specs/creators/monster-authoring.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/gamemodes/bloodtest.md
M	docs/specs/movement/movement.md
M	docs/specs/procedural-infinite-world/procedural-infinite-world.md
M	src/network/multiplayer-shots.cpp
M	src/network/server-npcs.cpp
M	src/npc/npc-combat.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-19T00:53:35.016823Z",
    "latest_successful_build_id": "2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d",
    "latest_successful_source_generation": 3
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "a124a1ee639728eb55c7c097d248fdf90e0fa548ffd237ac6049c8afe36b8f18",
    "dll": "build\\game\\generation-345\\game-g345.dll",
    "generation": 345,
    "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-345\\game-g345.dll",
    "generation": 345,
    "published_utc": "2026-11-19T00:53:18.768Z",
    "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 9.709,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-19T00:53:32.510Z"
  }
}
```
