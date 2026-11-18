// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Fun Counterstrike HUD settings

# Task

- Task ID: session-20261118-013739
- Session ID: session-20261118-013739
- Owner: Codex
- Focus: config/gamemodes/funcounterstrike.json
- Summary: Set Fun Counterstrike to hide friendly and enemy outlines and show friendly and enemy actor healthbars. Smoke-based healthbar occlusion is not exposed as JSON policy; healthbars are drawn in screen space and need code support for smoke-aware occlusion.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T01:37:41+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The checkout already had unrelated edits across configuration, movement,
  dev-loop, regression, and issue-tracker files, plus untracked changelogs and
  other files. `config/gamemodes/funcounterstrike.json` was clean before this
  session's four presentation changes.
- All pre-existing paths and edits were preserved.

# Requested behavior

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/graphics/graphics-presentation.md`,
  `docs/architecture/json-configuration/json-configuration.md`.
- Exact requirements: per-mode `enemy_healthbars` and `friendly_healthbars`
  are independent boolean overrides; `player_outlines` controls team outlines.
- The JSON field owners already accept these settings. The current healthbar
  renderer draws bars in screen-space UI, and no smoke-occlusion JSON setting
  was found. Therefore smoke-aware hiding requires code support and is outside
  the requested JSON-only scope.
- Human decision is needed before expanding the request to code that hides
  healthbars based on smoke.

# Exact implementation changes

## File: `config/gamemodes/funcounterstrike.json`

- Lines/functions/headings: `presentation` in
  `config/gamemodes/funcounterstrike.json`, lines 184-187.
- Old: enemy/friendly healthbars inherited (`null`); player and friendly
  outlines enabled (`true`).
- New: enemy/friendly healthbars enabled (`true`); player and friendly outlines
  disabled (`false`).
- Reason: match the requested mode presentation using existing JSON controls.
- No other presentation fields were changed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/gamemodes/funcounterstrike.json` | healthbar values `null`, outlines `true` | healthbars `true`, outlines `false` | Requested mode behavior | Restore only these four values to their old values |
| `docs/changelog/2026-11-18/20261118_013741-fun-counterstrike-hud-settings.md` | absent | this record | Required session record | Remove this changelog if reverting the session |

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

- Focused skills: `docs/skills/spec-behavior-review-v1.md` — PASS; JSON
  configuration architecture reviewed.
- No tests were requested or run. Diff was inspected. No code or executable
  build was needed for the four existing JSON fields.
- The dev-loop status before the edit showed running EXE and game DLL generation
  328; its active mode was `funcounterstrike`. This is not proof that these new
  values reloaded in that process.
- Runtime visual confirmation of outlines and bars remains outstanding. Smoke
  occlusion is unsupported by existing JSON.

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
- Focus: config/gamemodes/funcounterstrike.json
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82", "dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "published_utc": "2026-11-17T20:36:28.167Z", "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 13.098, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-17T21:52:51.348Z"}}
- Generation: 328
- Artifact: build\game\generation-328\game-g328.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_005857\client-01\events.jsonl
- Session ID: session-20261118-013739

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
 M config/gamemodes/funcounterstrike.json
 M config/movement.json
 M config/movement/movement-source.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M devscripts/dev-loop.py
 M devscripts/dev-profiles/npc-navigation.json
 M docs/regressions/dev-loop/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/open-gameplay-issues.md
?? config/movement/movement-deliberate.json
?? docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md
?? docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md
?? docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md
?? docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md
?? docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md
?? docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md
?? docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  6 ++---
 config/analytics.json                              |  2 +-
 config/audio/music-settings.json                   |  4 +--
 config/collision.json                              |  4 +--
 config/gamemodes/funcounterstrike.json             |  8 +++---
 config/movement.json                               |  1 +
 config/movement/movement-source.json               |  5 ++++
 config/video-settings.json                         |  2 +-
 devscripts/blender-ai                              |  0
 devscripts/dev-launch-modes.json                   |  1 +
 devscripts/dev-loop.py                             | 22 ++++++++++++++++
 devscripts/dev-profiles/npc-navigation.json        |  2 +-
 docs/regressions/dev-loop/README.md                | 16 +++++++++++-
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 ++++++++++++++--------
 docs/specs/open-gameplay-issues.md                 | 13 +++++++++-
 16 files changed, 90 insertions(+), 28 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/collision.json
M	config/gamemodes/funcounterstrike.json
M	config/movement.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	devscripts/dev-loop.py
M	devscripts/dev-profiles/npc-navigation.json
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
    "build_status": "published_current",
    "latest_build_stale": false,
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
    "compiled": 0,
    "elapsed_s": 13.098,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-17T21:52:51.348Z"
  }
}
```
