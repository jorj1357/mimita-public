// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Whole-game DLL queue timing

# Task

- Task ID: session-20261119-013056
- Session ID: session-20261119-013056
- Owner: dev-loop
- Focus: whole-game DLL build timing and dashboard
- Summary: Expanded the whole-game DLL queue view and per-attempt timing trace, attached explicit agent change notes, and documented the observed generation 346 cache-scan delay and compile failure.
- Status: in-progress
- Date, time, timezone: 2026-11-19T01:30:57+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 2ec7671ce73a94c28e9687dca602ab1b0291a1e5
- Final commit: 2ec7671ce73a94c28e9687dca602ab1b0291a1e5

# Pre-existing changes

- Exact status output:
- Files not created or modified by this session:

# Requested behavior

# Specification alignment

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/operations/devscripts/script-inventory.md` |  |  |  | git checkout -- docs/operations/devscripts/script-inventory.md |
| `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md` |  |  |  | git checkout -- docs/regressions/performance/server-tick-budget-REG-2026-11-03.md |
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
| `docs/changelog/2026-11-19/20261119_012838-investigate-juggernaut-server-tick-slowdown.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_012838-investigate-juggernaut-server-tick-slowdown.md' |
| `docs/changelog/2026-11-19/20261119_0129-npc-hitscan-blood-without-damage-trace.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_0129-npc-hitscan-blood-without-damage-trace.md' |

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

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Owner: dev-loop
- Focus: whole-game DLL build timing and dashboard
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 2ec7671ce73a94c28e9687dca602ab1b0291a1e5 / 2ec7671ce73a94c28e9687dca602ab1b0291a1e5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-19T00:53:35.016823Z", "latest_successful_build_id": "2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "a124a1ee639728eb55c7c097d248fdf90e0fa548ffd237ac6049c8afe36b8f18", "dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "published_utc": "2026-11-19T00:53:18.768Z", "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 9.709, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-19T00:53:32.510Z"}}
- Generation: 345
- Artifact: build\game\generation-345\game-g345.dll
- Journal path: C:\mimita-v9\logs\2026-11-19\20261119_012255\server-01\events.jsonl
- Session ID: session-20261119-013056

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
 M config/hot-reload/packet-ledger.json
 m devscripts/blender-ai
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/game_build_worker.py
 M docs/ROUTER.md
 M docs/operations/devscripts/script-inventory.md
 M docs/regressions/performance/server-tick-budget-REG-2026-11-03.md
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
?? docs/changelog/2026-11-19/20261119_012838-investigate-juggernaut-server-tick-slowdown.md
?? docs/changelog/2026-11-19/20261119_0129-npc-hitscan-blood-without-damage-trace.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 config/hot-reload/packet-ledger.json               |   6 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/game_build.py                  |  51 ++++++-
 devscripts/dev_loop/gui.py                         |  23 +++-
 devscripts/game_build_worker.py                    |  97 +++++++++++--
 docs/ROUTER.md                                     |   1 +
 docs/operations/devscripts/script-inventory.md     |   1 +
 .../server-tick-budget-REG-2026-11-03.md           |  36 +++++
 docs/specs/creators/monster-authoring.md           | 134 +++++++++++++++++-
 docs/specs/dev-loop/build-time-observability.md    | 105 +++++++++++---
 docs/specs/dev-loop/dashboard.md                   |  32 +++--
 docs/specs/gamemodes/bloodtest.md                  | 119 +++++++++++++++-
 docs/specs/movement/movement.md                    |  18 ++-
 .../procedural-infinite-world.md                   | 107 +++++++++++++++
 src/network/multiplayer-shots.cpp                  |  46 +++++++
 src/network/server-npcs.cpp                        | 151 ++++++++++++++++++---
 src/npc/npc-combat.cpp                             |  38 +++++-
 19 files changed, 879 insertions(+), 90 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/hot-reload/packet-ledger.json
M	devscripts/blender-ai
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/game_build_worker.py
M	docs/ROUTER.md
M	docs/operations/devscripts/script-inventory.md
M	docs/regressions/performance/server-tick-budget-REG-2026-11-03.md
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
