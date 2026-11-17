// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Cap EXE compiler jobs

# Task

- Task ID: session-20261117-185347
- Session ID: session-20261117-185347
- Owner: Codex
- Focus: build.py job count
- Summary: Set the build.py default compiler job cap to four while preserving the previous all-cores default as a comment.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-17T18:53:48+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 6c9bc7c562dd306f879bcb1737bdadea6a7ba3db
- Final commit: 6c9bc7c562dd306f879bcb1737bdadea6a7ba3db

# Pre-existing changes

- Exact status output: `git status --short` showed pre-existing changes in `config/analytics.json`, `devscripts/blender-ai`, two earlier changelogs, `docs/handoffs/networking-hot-boundary-split.md`, `docs/specs/open-gameplay-issues.md`, `src/engine/engine-tick-setup.cpp`, and `src/npc/npc-navigator.cpp`.
- Files not created or modified by this session: all paths listed above.

# Requested behavior

Set `build.py` default compiler jobs to four and leave the previous all-cores default commented out.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/operations/build-and-exe/build-and-exe.md`.
- Exact requirements: EXE job count is configurable with `MIMITA_BUILD_JOBS`; the existing default was all logical processors.
- Why the change follows the specification: keeps the existing override and applies a four-job default in `build.py`.
- Conflicts or decisions: reference prose in build docs still describes the previous all-processors policy; no documentation policy update was requested. This affects EXE builds only.

# Exact implementation changes

## File: `build.py`

- Lines/functions/headings: `_requested_jobs`, lines 123-133.
- Old content or behavior: unset `MIMITA_BUILD_JOBS` defaulted to `0`, meaning all logical processors; malformed values also fell back to all processors.
- New content or behavior: unset or malformed value defaults to `4`; prior `0` default is commented out; environment override remains honored.
- Reason: reduce default EXE compile concurrency to keep the PC responsive.
- Why unrelated behavior is preserved: existing `min(processor_count, requested_jobs)` cap and override behavior remain.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `build.py` | all-cores default | four-job default, previous setting commented | reduce default EXE compiler concurrency | git checkout -- build.py |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/changelog/2026-11-17/20261117_115247-networking-hot-boundary-cutover-regression.md` |  |  |  | git checkout -- docs/changelog/2026-11-17/20261117_115247-networking-hot-boundary-cutover-regression.md |
| `docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md` |  |  |  | git checkout -- docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md |
| `docs/handoffs/networking-hot-boundary-split.md` |  |  |  | git checkout -- docs/handoffs/networking-hot-boundary-split.md |
| `docs/specs/open-gameplay-issues.md` |  |  |  | git checkout -- docs/specs/open-gameplay-issues.md |
| `src/engine/engine-tick-setup.cpp` |  |  |  | git checkout -- src/engine/engine-tick-setup.cpp |
| `src/npc/npc-navigator.cpp` |  |  |  | git checkout -- src/npc/npc-navigator.cpp |

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

- Focused skill paths and results: not applicable for this small build configuration adjustment.
- Tests and exact commands: none run, per instruction not to add or run tests unless requested.
- Build status: no build run; setting will apply to a future EXE build.
- Runtime or hot-reload evidence: not applicable; game DLL build path is unaffected.
- Output files: none.

# Measured evidence

- Before values: EXE compile jobs defaulted to all 12 logical processors on this machine.
- After values: default cap is four jobs.
- Timestamps: edit recorded 2026-11-17T18:53Z.
- Tick/frame/network measurements: not applicable.

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review: not applicable.
- Gameplay review: not applicable.
- Multiplayer review: not applicable.
- Still unverified: next EXE build should confirm expected four-job limit; this does not cap game DLL compilation.

# Agent handoff context

- Owner: Codex
- Focus: build.py job count
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 6c9bc7c562dd306f879bcb1737bdadea6a7ba3db / 6c9bc7c562dd306f879bcb1737bdadea6a7ba3db
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T17:02:29.218920Z", "latest_successful_build_id": "2026-11-17T17:02:29.218920Z:ac525b001e42d6b78d49ea7cefa444bb82a6925e5a303f9836877c7e0ab3f8d0", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "ac35c7ea4518a56b587b1ca3705d85245810d3e26dc60d0037b6276024808aeb", "dll": "build\\game\\generation-322\\game-g322.dll", "generation": 322, "revision": "f984b8af418c3aee9a22559670235fab2a638ccb95bf54675d8bbb4054b41514", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-322\\game-g322.dll", "generation": 322, "published_utc": "2026-11-17T18:32:55.427Z", "revision": "f984b8af418c3aee9a22559670235fab2a638ccb95bf54675d8bbb4054b41514", "status": "ready"}, "progress": {"compiled": 1, "elapsed_s": 44.844, "phase": "build_complete", "result": "success", "skipped": 86, "updated_utc": "2026-11-17T17:02:26.927Z"}}
- Generation: 322
- Artifact: build\game\generation-322\game-g322.dll
- Journal path: C:\mimita-v9\logs\2026-11-17\20261117_170234\server-01\events.jsonl
- Session ID: session-20261117-185347

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
M build.py
 M config/analytics.json
 m devscripts/blender-ai
 M docs/changelog/2026-11-17/20261117_115247-networking-hot-boundary-cutover-regression.md
 M docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md
 M docs/handoffs/networking-hot-boundary-split.md
 M docs/specs/open-gameplay-issues.md
 M src/engine/engine-tick-setup.cpp
 M src/npc/npc-navigator.cpp
```

## Captured diff stat

```text
build.py                                           |   6 +-
 config/analytics.json                              |   2 +-
 devscripts/blender-ai                              |   0
 ...7-networking-hot-boundary-cutover-regression.md |   4 +-
 ...4410-perf-report-modes-and-frame-owner-fixes.md |  10 ++
 docs/handoffs/networking-hot-boundary-split.md     | 110 +++++++++++++++++++++
 docs/specs/open-gameplay-issues.md                 |   1 +
 src/engine/engine-tick-setup.cpp                   |   2 +-
 src/npc/npc-navigator.cpp                          |  31 ++++++
 9 files changed, 161 insertions(+), 5 deletions(-)
```

## Captured changed paths

```text
M	build.py
M	config/analytics.json
M	devscripts/blender-ai
M	docs/changelog/2026-11-17/20261117_115247-networking-hot-boundary-cutover-regression.md
M	docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md
M	docs/handoffs/networking-hot-boundary-split.md
M	docs/specs/open-gameplay-issues.md
M	src/engine/engine-tick-setup.cpp
M	src/npc/npc-navigator.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-17T17:02:29.218920Z",
    "latest_successful_build_id": "2026-11-17T17:02:29.218920Z:ac525b001e42d6b78d49ea7cefa444bb82a6925e5a303f9836877c7e0ab3f8d0",
    "latest_successful_source_generation": 3
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "ac35c7ea4518a56b587b1ca3705d85245810d3e26dc60d0037b6276024808aeb",
    "dll": "build\\game\\generation-322\\game-g322.dll",
    "generation": 322,
    "revision": "f984b8af418c3aee9a22559670235fab2a638ccb95bf54675d8bbb4054b41514",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-322\\game-g322.dll",
    "generation": 322,
    "published_utc": "2026-11-17T18:32:55.427Z",
    "revision": "f984b8af418c3aee9a22559670235fab2a638ccb95bf54675d8bbb4054b41514",
    "status": "ready"
  },
  "progress": {
    "compiled": 1,
    "elapsed_s": 44.844,
    "phase": "build_complete",
    "result": "success",
    "skipped": 86,
    "updated_utc": "2026-11-17T17:02:26.927Z"
  }
}
```
