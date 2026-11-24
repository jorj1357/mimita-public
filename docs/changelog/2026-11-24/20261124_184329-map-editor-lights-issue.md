// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Map editor lights issue

# Task

- Task ID: session-a689eaa021e5
- Session ID: session-a689eaa021e5
- Owner: Codex
- Focus: Issue intake and coordination
- Summary: Created a complete, unowned agent issue and task to investigate and implement persistent map-editor lights using shared dynamic lighting; linked the exact user request and documented lifetime/cap risks. No source code changed.
- Status: PASS
- Date, time, timezone: 2026-11-24T18:43:28+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874
- Final commit: c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874

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
| `config/maps/arena1.json` |  |  |  | git checkout -- config/maps/arena1.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md |
| `docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md |
| `docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md` |  |  |  | git checkout -- docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/specs/vehicles/vehicles.md` |  |  |  | git checkout -- docs/specs/vehicles/vehicles.md |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `assets/sound/weapon/ak47/` |  |  |  | Remove-Item -LiteralPath 'assets/sound/weapon/ak47/' |
| `assets/sound/weapon/judgement/` |  |  |  | Remove-Item -LiteralPath 'assets/sound/weapon/judgement/' |
| `docs/issues/feature/20261124-investigate-and-add-persistent-map-editor-lights-issue-1e01f275c858.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/feature/20261124-investigate-and-add-persistent-map-editor-lights-issue-1e01f275c858.md' |

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

- Owner: Codex
- Focus: Issue intake and coordination
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874 / c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-24T17:05:20.935679Z", "latest_successful_build_id": "2026-11-24T17:05:20.935679Z:c8f5b6b4f9d0ca8a0c2400890dd2cba972bd66fa0a23cd323077460a21de516d", "latest_successful_source_generation": 12}, "game_current": {"build_mode": "debug", "code_hash": "fae92d9328dfc861a59c106888234927d93df0b49d70ad5dae2daff89ac5415c", "dll": "build\\game\\generation-599\\game-g599.dll", "generation": 599, "revision": "5f8250c3c73855c103b83174432657bf0979da47861c6bf4d3b302294da8b0ff", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-599\\game-g599.dll", "generation": 599, "published_utc": "2026-11-24T18:29:30.763Z", "revision": "5f8250c3c73855c103b83174432657bf0979da47861c6bf4d3b302294da8b0ff", "status": "ready"}, "progress": {"compiled": 12, "elapsed_s": 105.626, "phase": "build_complete", "result": "success", "skipped": 75, "updated_utc": "2026-11-24T17:05:08.989Z"}}
- Generation: 599
- Artifact: build\game\generation-599\game-g599.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_174426\client-01\events.jsonl
- Session ID: session-a689eaa021e5

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
 M config/maps/arena1.json
 m devscripts/blender-ai
 M docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md
 M docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md
 M docs/issues/README.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md
 M docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/specs/vehicles/vehicles.md
 M src/vehicles/vehicle-system.cpp
?? assets/sound/weapon/ak47/
?? assets/sound/weapon/judgement/
?? docs/issues/feature/20261124-investigate-and-add-persistent-map-editor-lights-issue-1e01f275c858.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  2 +-
 config/analytics.json                              |  2 +-
 config/maps/arena1.json                            | 25 ++++++++
 devscripts/blender-ai                              |  0
 .../20261124_1740-header-fanout-reduction-plan.md  |  8 +++
 ...124_1746-sampler-setup-cost-and-failed-build.md | 18 ++++++
 docs/issues/README.md                              | 13 ++--
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  8 +++
 ...e-dll-build-at-game-build-issue-d82b02478dff.md | 28 +++++++++
 ...ient-release-gate-for-all-issue-c8290acca248.md | 29 +++++++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md | 15 +++++
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  6 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 52 ++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 56 +++++++++++++++++
 docs/specs/vehicles/vehicles.md                    | 71 +++++++++++++++-------
 src/vehicles/vehicle-system.cpp                    | 37 +++++++++--
 16 files changed, 335 insertions(+), 35 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/maps/arena1.json
M	devscripts/blender-ai
M	docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md
M	docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md
M	docs/issues/README.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md
M	docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/specs/vehicles/vehicles.md
M	src/vehicles/vehicle-system.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-24T17:05:20.935679Z",
    "latest_successful_build_id": "2026-11-24T17:05:20.935679Z:c8f5b6b4f9d0ca8a0c2400890dd2cba972bd66fa0a23cd323077460a21de516d",
    "latest_successful_source_generation": 12
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "fae92d9328dfc861a59c106888234927d93df0b49d70ad5dae2daff89ac5415c",
    "dll": "build\\game\\generation-599\\game-g599.dll",
    "generation": 599,
    "revision": "5f8250c3c73855c103b83174432657bf0979da47861c6bf4d3b302294da8b0ff",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-599\\game-g599.dll",
    "generation": 599,
    "published_utc": "2026-11-24T18:29:30.763Z",
    "revision": "5f8250c3c73855c103b83174432657bf0979da47861c6bf4d3b302294da8b0ff",
    "status": "ready"
  },
  "progress": {
    "compiled": 12,
    "elapsed_s": 105.626,
    "phase": "build_complete",
    "result": "success",
    "skipped": 75,
    "updated_utc": "2026-11-24T17:05:08.989Z"
  }
}
```
