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
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-19T01:30:57+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 2ec7671ce73a94c28e9687dca602ab1b0291a1e5
- Final commit: 84494dc0

# Pre-existing changes

- Current checkout already contains the shared DLL queue, note script, and
  foundational build-trace work in base commit `2ec7671c`; this session did not
  create or claim those committed files.
- Concurrent unrelated edits preserved: `config/accounts/default.json`,
  `config/analytics.json`, `config/hot-reload/packet-ledger.json`,
  `devscripts/blender-ai`, `docs/ROUTER.md`,
  `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`,
  creator/mode/movement/procedural-world docs, and the NPC/network C++ files.
- Three untracked changelogs already existed before this session's final
  changelog: `20261119_011946-infinite-xxity-editor-and-performance-evidence.md`,
  `20261119_012838-investigate-juggernaut-server-tick-slowdown.md`, and
  `20261119_0129-npc-hitscan-blood-without-damage-trace.md`.

# Requested behavior

Trace the inputs and time spent per whole-game DLL generation, make that
trace visible in a large live queue panel, and let agents attach concise
change notes to the next generation. The user also asked for live verification.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/build-time-observability.md`,
  `docs/specs/dev-loop/dashboard.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`, and
  `docs/specs/dev-loop/dev-loop.md`.
- Exact requirements: one active whole-game DLL build; coalesce saved edits
  into one next generation; show a percentage only for a real source-unit
  denominator; preserve per-source and per-phase timing; keep build/runtime
  evidence separate.
- Why the change follows the specification: timings stay in per-attempt JSONL
  and bounded progress JSON; GUI reads bounded progress; `SKIP` is described
  as dependency-clean object reuse; queue counts generations, not request IDs.
- Conflicts or decisions: daemon code had an older source hash during the
  observed run. `--ensure-running` kept the existing Juggernaut run alive but
  did not activate the daemon-side changes. No forced daemon restart or build
  was issued because the shared-checkout gate permits only status/list/inspect/
  ensure-running control here.

# Exact implementation changes

## File: `devscripts/dev_loop/game_build.py`

- Lines/functions: `run_game_build`, progress setup, `report_output`, and
  worker command construction.
- Old: source generation and build mode were incomplete in the displayed
  context; worker output handling, stage time, and total attempt duration were
  not fully attributed; worker command duplicated `--attempt-context`.
- New: pass source generation and build mode, time root output handling, attach
  summary metrics, require the published revision to match, contain timing and
  context paths within the checkout, and pass one context argument.
- Reason: stable identity and useful end-to-end duration without accepting a
  stale artifact as this attempt's result.
- Unrelated behavior: build authority and compiler invocation policy stay with
  the root dev-loop.

## File: `devscripts/game_build_worker.py`

- Lines/functions: `Progress`, `early_phase`, source completion records, cache
  scan, compile submission, and phase transitions.
- Old: early worker phases and dependency checks lacked end-to-end timing;
  absent attempt context became `Path(".")`; executor queue timing was not
  tied to submission time.
- New: report early phase boundaries, cache/dependency timing, per-file write
  and compile duration, submit/start timestamps, and queue wait; represent an
  absent timing stream as `None`.
- Reason: distinguish preflight/cache scan, compile, link, validation, and
  publication costs.
- Unrelated behavior: source selection, compile flags, cache key, and link
  inputs were not changed.

- The worker's current DLL inputs (`select_game_sources` and link response
  construction) are documented from source: `owner == game` manifest units
  except the probe module; one object per selected source with fresh cached
  objects reused; generated module identity header; optional GLAD object;
  manifold archive; GLFW and system libraries. Debug/release flags, optional
  PCH, coherence checks, staging rename, and SHA-256 publication are specified.
- The generated identity header changes per generation and is included by 11
  module wrappers. Generation 346 rebuilt all 11 wrappers, a concrete repeat
  compile fan-out candidate. Verify module identity and ABI requirements before
  moving it out of compile-time inputs.

## File: `devscripts/dev_loop/gui.py`

- Lines/functions: `_game_dll_progress`, `_game_dll_queue_view`, and
  `update_game_queue_panel`.
- Old: an old published matching revision could appear 100% complete during a
  new build; pending notes were hidden when no progress record existed.
- New: prevent false completion during an active build, show pending notes,
  expose attempt mode/start time and phase timing, and make long build detail
  scrollable in the queue card.
- Reason: accurately present the information already produced by the worker.
- Unrelated behavior: no gameplay or game process control changes.

## Files: `docs/specs/dev-loop/build-time-observability.md`,
`docs/specs/dev-loop/dashboard.md`, and
`docs/operations/devscripts/script-inventory.md`

- Lines/headings: persisted timing record, whole-game DLL generation trace,
  observed generation 346 trace, dashboard queue requirements, script row.
- Old: timing contract covered EXE builds and lacked phase/per-file attribution
  for DLL attempts.
- New: specify the DLL build pipeline, JSONL/summary output, agent notes,
  queue percentage limits, and recorded live evidence with unresolved causes.
- Reason: align the desired contract with the actual owner and avoid a false
  optimization claim.
- Unrelated behavior: no regression record was added because the build failure
  is not a confirmed gameplay regression.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `devscripts/dev_loop/game_build.py` | No current per-source handler duration; no source-generation/mode context on progress; duplicate context argument. | Timed handler and total build summary, context metadata, path containment, single worker-context argument. | Make the attempt identity and wall time auditable. | If rejected, restore only this path from `84494dc0^`; preserve other files in that commit. |
| `devscripts/game_build_worker.py` | No complete phase trace or precise executor wait; optional path represented as `.`. | Added phase, dependency, queue, and progress-write timing. | Identify the actual long phase before tuning. | If rejected, restore only this path from `84494dc0^`; preserve other files in that commit. |
| `devscripts/dev_loop/gui.py` | Could show old matching output as complete during a new active build; details could overflow. | Pending note fallback, scrollable detail, build mode/start, phase timing. | Prevent misleading percent and expose queue evidence. | If rejected, restore only this path from `84494dc0^`; preserve other files in that commit. |
| `docs/specs/dev-loop/build-time-observability.md` | EXE-only purpose and timing contract. | Added DLL trace contract and generation 346 observation. | Document scope and evidence. | If rejected, restore only this path from `84494dc0^`; preserve other files in that commit. |
| `docs/specs/dev-loop/dashboard.md` | Compact progress requirements omitted full queue and timing. | Queue fields, accurate SKIP meaning, progress and note requirements. | Make dashboard expectations reviewable. | If rejected, restore only this path from `84494dc0^`; preserve other files in that commit. |
| `docs/operations/devscripts/script-inventory.md` | No entry for the already committed note tool. | Added one inventory row for `devscripts/build-note.py`. | Route maintainers to the tool. | If rejected, restore only this path from `84494dc0^`; preserve other files in that commit. |

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

- Owner/category: root dev-loop and whole-game DLL worker; `dev-loop.game-build.*`.
- Input: accepted source generation, source manifest/revision, build mode,
  changed-path report, optional explicit notes.
- Decision: dependency-fresh object reuse or compile; later publish only when
  revision matches.
- Output: attempt JSONL, bounded `build/game/progress.json`, status/dashboard
  fields, and final summary.
- Failure: live generation 346 failed compiling `src/network/server-npcs.cpp`;
  cause remains open. The run status also reported an earlier packet registry
  codegen failure. Neither is evidence of a Juggernaut runtime crash.
- Rate limiting: full file rows live in one per-attempt JSONL stream; dashboard
  reads only bounded live progress.

# Validation

- Focused skills: `docs/skills/spec-behavior-review-v1.md` PASS_WITH_HUMAN_REVIEW;
  `docs/skills/documentation-checker-v1.md` PASS with pre-existing router/skill
  self-references to TODO examples; `docs/skills/efficiency-checker-v1.md`
  flagged the long cache-scan and handler-tail measurements for attribution.
- Tests: none run; repository instructions prohibit agents from running builds
  and this task did not authorize synthetic tests. `git diff --check` passed.
- Build status: no build was started by this session. Existing root status:
  generation 345 published; generation 346 later failed compile. No compiler
  or build request was issued by this session.
- Runtime: `python devscripts/dev-loop.py --ensure-running` preserved run
  `20261119_012255`; status showed server PID 20664 and client PID 18796 alive.
  Dashboard process was reopened as PID 11484. The dashboard source hash was
  updated but the daemon hash was stale, so new daemon timings and visual panel
  layout were not live-accepted.
- Output: changed source/spec files above; live journal
  `logs/2026-11-19/20261119_012255/dev-loop/events.jsonl`.

# Measured evidence

- Live run: `20261119_012255`; attempt `game-dll-b801803cf8e3491e`, generation
  346, revision `002c3887373188e0a2b8dd7928336471919387eec290fff69cad49bd0760b0a4`.
- Worker counts: 642 total; 628 skipped, 13 compiled, 1 failed. The final
  progress record showed 231.6 seconds elapsed and 33.953 seconds compile wall
  time before failing; compiler duration summed per file is concurrent and can
  exceed wall time.
- Cache scan: sequence 625 was at 193.735 seconds elapsed, so the scan/prework
  dominated. Only 244 journal file rows remained. Their median worker-to-daemon
  observation lag was 14 ms; maximum was 25,057 ms. These are observed values,
  not a full per-attempt attribution.
- Compiles: 11 of 13 successful object compiles were module wrappers. The
  compiler durations overlap, so their sum is not the user's wall-clock wait.
- Progress file write: 2,039.7 ms summed across the 244 retained rows, with
  occasional spikes. The cause of the large lag tail remains unproven.
- Compile failure: `src/network/server-npcs.cpp`; GCC output reported an
  `rtmintrin.h` include failure and `stdout: Bad file descriptor`.
- Previous successful DLL: generation 345 at `2026-11-19T00:53:18.768Z`,
  SHA-256 `a124a1ee639728eb55c7c097d248fdf90e0fa548ffd237ac6049c8afe36b8f18`.

# Regression review

- Regression entry appended: no.
- Reason: no gameplay behavior regression was established. The current compile
  failure and slow generation trace need separate technical attribution.
- Related regression paths: none for this UI/observability change.

# Human acceptance

- Visual review: dashboard process reopened, but this session did not capture
  or obtain human review of the updated panel.
- Gameplay review: not performed; the existing Juggernaut run was preserved.
- Multiplayer review: not performed.
- Still unverified: daemon activation of timing changes, visual presentation,
  and a successful subsequent build with complete timing records.

# Agent handoff context

- Owner: dev-loop
- Focus: whole-game DLL build timing and dashboard
- Files to check: `devscripts/dev_loop/game_build.py`,
  `devscripts/game_build_worker.py`, `devscripts/dev_loop/gui.py`, and
  `build-time-observability.md`.
- Do not redo: do not rerun build 346 or re-derive its truncated journal.
- Next exact step: under the user's authorized shared dev-loop workflow, load
  the current daemon source only when the existing running pair can be kept;
  inspect one future `.dev/build-timings/<attempt-id>.jsonl` to attribute the
  cache scan and 25-second output-observation tail, then tune that measured
  owner. Evaluate the 11-wrapper identity-header fan-out separately against
  the module ABI/source-identity contract.

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
