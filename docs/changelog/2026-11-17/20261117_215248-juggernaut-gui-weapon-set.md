// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Juggernaut GUI weapon set

# Task

- Task ID: session-20261117-juggernaut-gui-weapon-set
- Session ID: session-20261117-juggernaut-gui-weapon-set
- Owner: dev-loop launch preset
- Focus: devscripts/dev-launch-modes.json: launch mode 9; logs/2026-11-17/20261117_214010/run.json
- Summary: The GUI's Juggernaut launch preset inherited weapon set 12 from the active dev profile; make the preset explicitly use Auto (mode set).
- Status: SOURCE FIXED; NEW GUI LAUNCH VALIDATION PENDING
- Date, time, timezone: 2026-11-17T21:52:47+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The initial status also showed pre-existing changes to `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/collision.json`, `config/movement.json`, `config/video-settings.json`, the `devscripts/blender-ai` submodule, `docs/regressions/dev-loop/README.md`, `docs/regressions/networking/README.md`, `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md`, and `docs/specs/open-gameplay-issues.md`, plus untracked `config/movement/movement-deliberate.json`, three earlier changelogs, and `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md`. These were preserved. The README received only the additional Entry 28 in this session.

# Requested behavior

The user selected `9 — juggernaut` in the GUI and asked why the run used
Captain Falcon's weapon set.

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/gamemodes.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`.
- Exact requirement: the gamemode spec says Auto uses the selected gamemode's weapon-set behavior and a concrete host selection takes precedence.
- Why the change follows the specification: the GUI-selected mode preset now explicitly requests Auto so an unrelated dev-profile selection cannot displace Juggernaut role loadouts.
- Conflicts or decisions: none. Existing role sets already define Juggernaut weapons.

# Exact implementation changes

## File: `devscripts/dev-launch-modes.json`

- Lines/functions/headings: launch mode `9` (`juggernaut`), line 85.
- Old content or behavior: no `weapon_set` field; dev-loop fell back to the active profile's value `12`.
- New content or behavior: adds `"weapon_set": "auto"`; the dev-loop omits the explicit `--weapon-set` argument.
- Reason: ensure the selected GUI launch mode controls weapon-set precedence.
- Why unrelated behavior is preserved: only launch mode 9 changed; other presets and the current game process are untouched.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/collision.json` |  |  |  | git checkout -- config/collision.json |
| `config/movement.json` |  |  |  | git checkout -- config/movement.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-launch-modes.json` |  |  |  | git checkout -- devscripts/dev-launch-modes.json |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/regressions/networking/README.md` |  |  |  | git checkout -- docs/regressions/networking/README.md |
| `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md` |  |  |  | git checkout -- docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md |
| `docs/specs/open-gameplay-issues.md` |  |  |  | git checkout -- docs/specs/open-gameplay-issues.md |
| `config/movement/movement-deliberate.json` |  |  |  | Remove-Item -LiteralPath 'config/movement/movement-deliberate.json' |
| `docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md' |
| `docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md' |
| `docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md' |
| `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md' |

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

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` reviewed; cause and correction are documented.
- Tests and exact commands: no tests run; inspected the launch-mode config and `DevLoop.launch_for_control_request` argument-resolution path.
- Build status: no build requested or run; this is launch configuration. `python devscripts/dev-loop.py --status` reported the existing published build current.
- Runtime or hot-reload evidence: old run `20261117_214010` confirms pre-fix launch identity `weapon_set=12` and Captain Falcon NPC equipment. No new run was started; GUI acceptance remains pending.
- Output files: this changelog and `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md`.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes — Entry 28.
- Why this is or is not a confirmed regression: the user-selected GUI mode was Juggernaut but the effective live launch selected Captain Falcon weapons from the profile.
- Related regression paths: `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: pending next GUI launch.
- Multiplayer review: not performed.
- Still unverified: a fresh GUI launch resolves Juggernaut's role weapons in the server journal.

# Agent handoff context

- Owner: dev-loop launch preset
- Focus: devscripts/dev-launch-modes.json: launch mode 9; logs/2026-11-17/20261117_214010/run.json
- Files to check: `devscripts/dev-launch-modes.json` mode 9; `devscripts/dev-loop.py` launch argument precedence; the linked regression record.
- What another agent should not redo: do not diagnose this as the user selecting the wrong mode; the screenshot shows mode 9 Juggernaut and the fallback cause is documented.
- Next exact step: launch mode 9 through the GUI and inspect the new server journal for resolved role loadouts.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "building", "latest_build_stale": true, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82", "dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "published_utc": "2026-11-17T20:36:28.167Z", "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ready"}, "progress": {"elapsed_s": 0.156, "phase": "setup", "updated_utc": "2026-11-17T21:52:37.781Z"}}
- Generation: 328
- Artifact: build\game\generation-328\game-g328.dll
- Journal path: C:\mimita-v9\logs\2026-11-17\20261117_214010\server-01\events.jsonl
- Session ID: session-20261117-juggernaut-gui-weapon-set

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
 M config/audio/music-settings.json
 M config/collision.json
 M config/movement.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M docs/regressions/dev-loop/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/open-gameplay-issues.md
?? config/movement/movement-deliberate.json
?? docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md
?? docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md
?? docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md
?? docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  8 +++---
 config/analytics.json                              |  2 +-
 config/audio/music-settings.json                   |  6 ++---
 config/collision.json                              |  4 +--
 config/movement.json                               |  1 +
 config/video-settings.json                         |  2 +-
 devscripts/blender-ai                              |  0
 devscripts/dev-launch-modes.json                   |  1 +
 docs/regressions/dev-loop/README.md                |  9 ++++++-
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 ++++++++++++++--------
 docs/specs/open-gameplay-issues.md                 | 13 +++++++++-
 12 files changed, 53 insertions(+), 25 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/collision.json
M	config/movement.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	docs/regressions/dev-loop/README.md
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
M	docs/specs/open-gameplay-issues.md
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "building",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-17T21:12:47.128057Z",
    "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac",
    "latest_successful_source_generation": 98
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82",
    "dll": "build\\game\\generation-328\\game-g328.dll",
    "generation": 328,
    "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-328\\game-g328.dll",
    "generation": 328,
    "published_utc": "2026-11-17T20:36:28.167Z",
    "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae",
    "status": "ready"
  },
  "progress": {
    "elapsed_s": 0.156,
    "phase": "setup",
    "updated_utc": "2026-11-17T21:52:37.781Z"
  }
}
```
