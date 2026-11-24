// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Mode3 edit-to-live latency evidence review

# Task

- Task ID: work-177f0ee3671d
- Session ID: work-177f0ee3671d
- Owner: hot-reload-latency-root
- Focus: Measure build latency and establish a safe next batch
- Summary: Rechecked root dev-loop status: no active build, 23 changed paths pending (17 C++ sources, 6 headers), launch mode is TDM with one NPC, so the live state does not satisfy the Juggernaut Horde heavy-workload acceptance. Parsed gen574 attempt timeline: 156.5s changed-source preflight, 1052.5s compile, 28.9s LLD link, 0.47s publish; 158 translation units compiled. Slowest units included ragdoll-mode.cpp 156.3s, melee-client.cpp 153.7s, npc.cpp 143.0s; GCC reports show parsing and optimization/code generation are major contributors in ragdoll-mode.cpp and npc.cpp. Confirmed the retained multiplayer-projectiles.cpp call precedes its helper definition and coordinated with the issue owner, CPU profiler, resource profiler, and batch contributor rather than editing shared sources. Added evidence-ranked next steps to the build issue. No source edit or build was performed.
- Status: in progress
- Date, time, timezone: 2026-11-24T00:22:32+00:00
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
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md` |  |  |  | git checkout -- docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `src/config/movement-config.cpp` |  |  |  | git checkout -- src/config/movement-config.cpp |
| `src/entities/player.h` |  |  |  | git checkout -- src/entities/player.h |
| `src/network/server-players.cpp` |  |  |  | git checkout -- src/network/server-players.cpp |
| `src/physics/movement/movement-step.cpp` |  |  |  | git checkout -- src/physics/movement/movement-step.cpp |
| `src/physics/movement/movement-types.h` |  |  |  | git checkout -- src/physics/movement/movement-types.h |
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

- Owner: hot-reload-latency-root
- Focus: Measure build latency and establish a safe next batch
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
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_001546\client-01\events.jsonl
- Session ID: work-177f0ee3671d

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
 M docs/issues/README.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M src/config/movement-config.cpp
 M src/entities/player.h
 M src/network/server-players.cpp
 M src/physics/movement/movement-step.cpp
 M src/physics/movement/movement-types.h
?? docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  6 +-
 config/analytics.json                              |  2 +-
 devscripts/blender-ai                              |  0
 docs/issues/README.md                              |  9 ++-
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  8 ++
 ...e-dll-build-at-game-build-issue-b1744402b0a5.md | 46 ++++++++++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  6 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  8 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 86 ++++++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 16 ++++
 src/config/movement-config.cpp                     | 20 +++++
 src/entities/player.h                              |  1 +
 src/network/server-players.cpp                     |  8 +-
 src/physics/movement/movement-step.cpp             | 83 +++++++++++++++++++--
 src/physics/movement/movement-types.h              | 27 +++++++
 15 files changed, 309 insertions(+), 17 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	devscripts/blender-ai
M	docs/issues/README.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	src/config/movement-config.cpp
M	src/entities/player.h
M	src/network/server-players.cpp
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

## Timing interpretation correction

The gen574 `changed_source_preflight` phase lasted 156.500 seconds, but this was not pure scan overhead. The raw timeline shows that two actual translation units compiled during this phase: `src/npc/npc.cpp` took 142.957 seconds and `src/ragdoll/ragdoll-mode.cpp` took 156.301 seconds (parallel elapsed durations). The next `compile` phase took 1,052.461 seconds. The worker summary reports 1,208.949 seconds of total compile work, consistent with those two preflight compiles plus the compile phase. The dependency prepass itself was about 1.030 seconds and object-cache scan 2.725 seconds. Treat the `changed_source_preflight` label as mixed compile/preflight work, not 156 seconds of scanning.

This distinction makes expensive translation units and the reason for their invalidation the main measured optimization targets. Host CPU averaged 99.997% and minimum available RAM was 165,240,832 bytes during gen574, but the attempt lacked a matched pre-build baseline; resource pressure is correlated context, not proven build-time causation.

Coordination bookkeeping: the initial lease commit used the pre-edit hash and recorded conflict `conflict-8bf6491e161e`. I verified there was no concurrent change, retained the intended correction, and reconciled that record with decision `decision-3f62c4f8633a`.

## Latest root status

At 2026-11-24 00:27Z, root dev-loop reported no active build and a pending game-only batch of 37 paths (24 `.cpp`, 13 headers), source generation 30. The current root runtime was TDM on `atdm`, one NPC; this is not the required Juggernaut Horde workload. Resource-profiler's matched no-build sample from 00:18:57–00:19:11Z recorded host CPU mean99.992% and RAM used92.53% (available mean1.187GB/min0.916GB), while the server/client pair used17.09% of host CPU. The top competing process ranking was unavailable, and no build overlapped, so this is not evidence of build-time causation. The batch remains unsealed while source ownership and the live workload are reconciled.

## No-op summary mismatch found

An audit of apparent sub-10-second history found `game-dll-20261119T151310460091Z-d7eef1` completes in 9.927 seconds but has `no_op=true` and `changed_paths=[]`. Its timeline contains only a 0.055-second object-cache scan and a 0.006-second no-op phase; there is no compile or link phase. The same summary's `worker_metrics` incorrectly claims 12 compiled TUs, 32.642 seconds in changed-source preflight, 42.755 seconds compiling, and 16.501 seconds linking. Treat those worker metrics as mismatched/stale and do not cite this attempt as a changed-source build. Among successful attempts with a non-empty `changed_paths`, the current best inspected sample is 13.859 seconds for `src/npc/npc-world-query.cpp` (two TUs compiled); it lacks a matched resource/workload comparison and does not meet the 10-second target.


## Runtime identity correction

The previous root-status paragraph described `effective_npcs=1` from dev-loop status as the runtime count. The server journal for run `20261124_001546` instead reports `npcs=9`, `players=1` at 00:17:01Z; the run/status view remains TDM on `atdm`. Another agent observed a client snapshot gap of about149.7s followed by Connecting/zero received packets while the server continued ticking and reconnecting. Treat the runtime workload and pair health as inconsistent; this is not verified Juggernaut Horde evidence.
