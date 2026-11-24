// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Matched runtime CPU sample and action attribution

# Task

- Task ID: work-ef28bcbf270e
- Session ID: work-ef28bcbf270e
- Owner: resource-profiler
- Focus: Runtime CPU and memory measurement, function-level attribution gaps, action provenance
- Summary: Captured a bounded matched server/client CPU and memory sample during the current TDM run, recorded uncertainty in the CPU issue, and sent the result to the DLL latency owner. Added a human-action attribution proposal to the existing agent-awareness issue.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-24T00:26:01+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Final commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83

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
| `config/maps/dust2cyberiav4.json` |  |  |  | git checkout -- config/maps/dust2cyberiav4.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/features/npc-behavior.md` |  |  |  | git checkout -- docs/features/npc-behavior.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md` |  |  |  | git checkout -- docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `src/config/movement-config.cpp` |  |  |  | git checkout -- src/config/movement-config.cpp |
| `src/entities/player.h` |  |  |  | git checkout -- src/entities/player.h |
| `src/input/input-frame.h` |  |  |  | git checkout -- src/input/input-frame.h |
| `src/input/input-state.h` |  |  |  | git checkout -- src/input/input-state.h |
| `src/network/server-players.cpp` |  |  |  | git checkout -- src/network/server-players.cpp |
| `src/physics/movement/movement-conversion.cpp` |  |  |  | git checkout -- src/physics/movement/movement-conversion.cpp |
| `src/physics/movement/movement-step.cpp` |  |  |  | git checkout -- src/physics/movement/movement-step.cpp |
| `src/physics/movement/movement-types.h` |  |  |  | git checkout -- src/physics/movement/movement-types.h |
| `docs/changelog/2026-11-24/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/' |
| `docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md' |

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

- Owner: resource-profiler
- Focus: Runtime CPU and memory measurement, function-level attribution gaps, action provenance
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-23T19:22:15.804284Z", "latest_successful_build_id": "2026-11-23T19:22:15.804284Z:2e24d312603e4135cdd9be0d870a11ce4be98042e253296de6ca16ca189d512b", "latest_successful_source_generation": 4}, "game_current": {"build_mode": "debug", "code_hash": "0aca6e2698d833f9baca4e390f57ac7f2b26acc9c2bbf7f37d2d600b7cc94ade", "dll": "build\\game\\generation-574\\game-g574.dll", "generation": 574, "revision": "421df633d9b5bfad37947b035806e7a2aade16a2a2059f26443a626e0634e40e", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-574\\game-g574.dll", "generation": 574, "published_utc": "2026-11-23T23:18:32.508Z", "revision": "421df633d9b5bfad37947b035806e7a2aade16a2a2059f26443a626e0634e40e", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 68.784, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-23T19:22:12.842Z"}}
- Generation: 574
- Artifact: build\game\generation-574\game-g574.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_001546\server-01\events.jsonl
- Session ID: work-ef28bcbf270e

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
 M config/maps/dust2cyberiav4.json
 m devscripts/blender-ai
 M docs/features/npc-behavior.md
 M docs/issues/README.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M src/config/movement-config.cpp
 M src/entities/player.h
 M src/input/input-frame.h
 M src/input/input-state.h
 M src/network/server-players.cpp
 M src/physics/movement/movement-conversion.cpp
 M src/physics/movement/movement-step.cpp
 M src/physics/movement/movement-types.h
?? docs/changelog/2026-11-24/
?? docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/maps/dust2cyberiav4.json                    | 110 ++++++++++++++++++++-
 devscripts/blender-ai                              |   0
 docs/features/npc-behavior.md                      |  30 ++++++
 docs/issues/README.md                              |  11 ++-
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   8 ++
 ...e-dll-build-at-game-build-issue-b1744402b0a5.md |  46 +++++++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |   6 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |   8 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  96 ++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |  22 +++++
 src/config/movement-config.cpp                     |  20 ++++
 src/entities/player.h                              |   1 +
 src/input/input-frame.h                            |   1 +
 src/input/input-state.h                            |   2 +
 src/network/server-players.cpp                     |   8 +-
 src/physics/movement/movement-conversion.cpp       |   4 +
 src/physics/movement/movement-step.cpp             |  83 +++++++++++++++-
 src/physics/movement/movement-types.h              |  27 +++++
 20 files changed, 468 insertions(+), 23 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/maps/dust2cyberiav4.json
M	devscripts/blender-ai
M	docs/features/npc-behavior.md
M	docs/issues/README.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	src/config/movement-config.cpp
M	src/entities/player.h
M	src/input/input-frame.h
M	src/input/input-state.h
M	src/network/server-players.cpp
M	src/physics/movement/movement-conversion.cpp
M	src/physics/movement/movement-step.cpp
M	src/physics/movement/movement-types.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-23T19:22:15.804284Z",
    "latest_successful_build_id": "2026-11-23T19:22:15.804284Z:2e24d312603e4135cdd9be0d870a11ce4be98042e253296de6ca16ca189d512b",
    "latest_successful_source_generation": 4
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "0aca6e2698d833f9baca4e390f57ac7f2b26acc9c2bbf7f37d2d600b7cc94ade",
    "dll": "build\\game\\generation-574\\game-g574.dll",
    "generation": 574,
    "revision": "421df633d9b5bfad37947b035806e7a2aade16a2a2059f26443a626e0634e40e",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-574\\game-g574.dll",
    "generation": 574,
    "published_utc": "2026-11-23T23:18:32.508Z",
    "revision": "421df633d9b5bfad37947b035806e7a2aade16a2a2059f26443a626e0634e40e",
    "status": "ready"
  },
  "progress": {
    "compiled": 4,
    "elapsed_s": 68.784,
    "phase": "build_complete",
    "result": "success",
    "skipped": 83,
    "updated_utc": "2026-11-23T19:22:12.842Z"
  }
}
```

# Session narrative

## What changed

- Added runtime sample evidence to `issue-ed3ac775283d` at its appended entry (the exact evidence text begins at line 2617 of the current issue file). Before this session, there was no sample for run `20261124_001546`; after this session, the issue records its UTC interval, live pair PIDs, normalized CPU, memory, threads, mode/NPC/player state, and known attribution gaps.
- Added a human-action attribution proposal to `issue-91e3384b219b` under “Human-action attribution follow-up” at line 675. There was no prior text for this request; the new entry proposes a correlated actor/origin/action/result record in canonical journals and leaves actor unknown when no source can prove identity.
- Added this changelog. No game source, configuration, build driver, or runtime process was changed by this session.

## Evidence and limits

- Runtime artifact: `.dev/perf-captures/runtime-pair-20261124T0018Z.csv` (15 host samples, 2026-11-24 00:18:57.157–00:19:11.648 UTC).
- Run manifest: `logs/2026-11-24/20261124_001546/run.json`; server journal: `logs/2026-11-24/20261124_001546/server-01/events.jsonl`; client journal: `logs/2026-11-24/20261124_001546/client-01/events.jsonl`.
- Server 5192 and client 13292 used the same EXE build ID. In-window server events identify TDM on `atdm`, 9 NPCs and 1 player, module generation 574. Build state was idle at the sample's start.
- Host CPU mean 99.992%; RAM used mean 92.53%, available mean 1,186,666,359 bytes (minimum 915,582,976); pagefile used mean 27.63 GB. Server mean CPU 6.30% whole host and client 10.79%, pair 17.09%. Mean working sets 1331/521 MiB, private bytes 1391/862 MiB, and thread counts 15/37.
- The attempted top-process ranking did not produce an artifact. Do not infer which external process consumed the remainder. No exact callsite CPU rows were verified, and existing frame scopes do not prove the 240 FPS target. This is runtime resource evidence, not build-time causation.
- A handoff with the sample was sent to the active DLL latency thread. No response/build result was observed in this session.

## Validation and review

- Read `docs/ROUTER.md`, `docs/specs/dev-loop/scenario-input-recording.md`, `docs/specs/debug-logging/canonical-jsonl.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/change-to-evidence-latency.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/operations/task-completion/task-completion.md`, and `docs/skills/documentation-checker-v1.md`.
- No source tests or build were run because no source code changed. No profiler/admin command was sent to the shared game runtime.
- Runtime health, human gameplay acceptance, 240 FPS client compliance, 60 Hz server compliance, function-level CPU attribution, GPU/network attribution, and the <=1%/<=500 MB quiet-workload targets remain unproven.

## Next step

Coordinate an explicitly owned instrumentation/build window with the DLL latency and gameplay owners. Verify the bounded callsite probe on a connected pair, capture matching runtime journals and host/process metrics, then use the measured hot function to select one optimization and repeat the same workload. Preserve separate proof for build latency, frame time, server tick, and gameplay behavior.
