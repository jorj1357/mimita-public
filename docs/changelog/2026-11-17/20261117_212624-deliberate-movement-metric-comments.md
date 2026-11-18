// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Deliberate movement metric comments

# Task

- Task ID: codex-20261117-deliberate-metric-comments
- Session ID: codex-20261117-deliberate-metric-comments
- Owner: Codex
- Focus: slow human-scale values and metric comments in config/movement/movement-deliberate.json
- Summary: Tuned the deliberate movement preset to slow human-scale metric movement and documented the physical comparisons.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-17T21:26:24+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- Exact status output was captured in the automatic evidence below before this session's config edits. It included unrelated edits to account, analytics, audio, movement selection, Blender tooling, networking regression records, and open gameplay issues.
- `config/movement/movement-deliberate.json` was already untracked as the requested task file. `config/movement.json` was already modified to select `deliberate`; this session did not change that selection.
- The earlier untracked payload startup ABI changelog was pre-existing and left untouched.

# Requested behavior

Set the deliberate movement preset to slow, typical human-scale metric movement values and explain the references in its comments.

# Specification alignment

- Current specification: `docs/specs/movement/movement.md`, sections 4 and 7. It describes movement values as tunable configuration, identifies ground/air speed and jump/gravity as movement values, and distinguishes the default arcade feel from optional movement modes.
- Human requested the deliberate preset to use typical human metric movement values and to feel slow. The file uses Source-family equations; therefore accel/friction values are documented as Source tuning values with their resulting approximate physical scale, not treated as direct SI quantities.
- No specification conflict found. Special abilities and impulses remain separately documented configuration and were disabled or bounded for this human-scale preset.

# Exact implementation changes

## File: `config/movement/movement-deliberate.json`

- Lines/headings: world-unit scale, ground speed/acceleration/friction, air control, gravity/jump/fall, jump behavior, MiMITA dash extensions, knockback cap, global speed limit, and `tuning_notes`.
- Old values included 20 units/s ground speed, gravity -70, jump speed 23.1, auto-bhop and one air jump enabled, dash/down-dash/freeze enabled, unlimited legacy knockback cap (9999), and disabled global speed limit.
- New values: 1.0 m/s relaxed walking speed; ground acceleration 1.5 (about 1.5 m/s²); friction and stop speed 1.0 (about 1.0 m/s² braking, roughly one second to stop from walking pace); air gain 1.0, air acceleration 1.5, projected air wish speed 1.0; gravity -9.81 m/s²; jump speed 3.13 m/s (about 0.5 m rise); max fall speed 55 m/s; no auto-bhop, extra air jumps, dash, down-dash, freeze, or air strafing; legacy external impulse cap 8 m/s; enabled 8 m/s fixed speed limit.
- Reason: bring the active preset's ordinary movement close to slow human walking, jumping, gravity, and acceleration scales, and bound the configured impulse paths to a fast human sprint speed.
- Other gameplay behavior is unchanged except for listed movement-related fields. The mode selection and unrelated working-tree changes were preserved.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/movement/movement-deliberate.json` | Fast Source-scale values | Slow human-scale movement values and explanatory comments | Match user's requested slow human metric feel | Restore the prior values listed above |
| `docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md` | Not present | This session record | Required repository record | Remove only this session changelog if reverting the entire session |

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

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` reviewed; no behavior/schema disagreement identified. The task is a config edit.
- Tests and exact commands: None run; no tests requested. A JSON5 parser was not installed in the Python environment (`ModuleNotFoundError: json5`), so no Python parser validation is claimed.
- Build status: No build requested or triggered. Existing dev-loop/build identity in automatic evidence predates this config edit and is unrelated.
- Runtime or hot-reload evidence: None; new movement values have not been observed in a live run.
- Output files: `config/movement/movement-deliberate.json`.

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

- Visual review: Not applicable to this config-only edit.
- Gameplay review: Still needed to confirm the movement feels appropriately human and slow.
- Multiplayer review: Not performed.
- Still unverified: Config load/hot-reload and runtime feel for the new values.

# Agent handoff context

- Owner: Codex
- Focus: config/movement/movement-deliberate.json comments
- Files to check: `config/movement/movement-deliberate.json`.
- What another agent should not redo: Do not change the unrelated `config/movement.json` selection or other pre-existing working-tree edits.
- Next exact step: Load/reload the active preset and assess its movement in-game; tune only if the user requests further adjustment.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82", "dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "published_utc": "2026-11-17T20:36:28.167Z", "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ready"}, "progress": {"compiled": 6, "elapsed_s": 17.401, "phase": "build_complete", "result": "success", "skipped": 81, "updated_utc": "2026-11-17T21:12:46.384Z"}}
- Generation: 328
- Artifact: build\game\generation-328\game-g328.dll
- Journal path: C:\mimita-v9\logs\2026-11-17\20261117_212313\client-01\events.jsonl
- Session ID: codex-20261117-deliberate-metric-comments

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
 M config/movement.json
 m devscripts/blender-ai
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/open-gameplay-issues.md
?? config/movement/movement-deliberate.json
?? docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  6 ++---
 config/analytics.json                              |  2 +-
 config/audio/music-settings.json                   |  4 +--
 config/movement.json                               |  3 ++-
 devscripts/blender-ai                              |  0
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 ++++++++++++++--------
 docs/specs/open-gameplay-issues.md                 | 13 +++++++++-
 8 files changed, 40 insertions(+), 20 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/movement.json
M	devscripts/blender-ai
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
    "compiled": 6,
    "elapsed_s": 17.401,
    "phase": "build_complete",
    "result": "success",
    "skipped": 81,
    "updated_utc": "2026-11-17T21:12:46.384Z"
  }
}
```
