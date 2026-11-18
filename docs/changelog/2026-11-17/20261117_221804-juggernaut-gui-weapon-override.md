// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Juggernaut GUI weapon override

# Task

- Task ID: session-20261117-juggernaut-gui-weapon-set-fix
- Session ID: session-20261117-juggernaut-gui-weapon-set-fix
- Owner: dev-loop launch mode
- Focus: devscripts/dev-loop.py launch mode selection and logs/2026-11-17/20261117_221550
- Summary: Clear stale weapon overrides when selecting an Auto mode; verify Juggernaut role loadouts from a fresh GUI launch.
- Status: FIXED; GUI launch and live role weapons verified
- Date, time, timezone: 2026-11-17T22:18:04Z (2026-11-17 17:18 EST)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The workspace already had unrelated edits in account, analytics, audio, collision, movement, video, the `devscripts/blender-ai` submodule, networking regressions, and `docs/specs/open-gameplay-issues.md`; those were left intact.
- This task continued the prior Juggernaut fix: launch mode 9 already declared Auto, its prior follow-up changelog was already present, and the NPC navigation profile had already been changed to Auto. The previous run `20261117_215600` and the user's selected GUI mode were established as context.
- The dashboard, watcher, server, and client were already running. The game was relaunched only through the dashboard; no extra dev-loop instance was started.

# Requested behavior

Selecting `9 — juggernaut` in the dashboard must launch Juggernaut without carrying Captain Falcon's explicit weapon set into the server. Juggernaut NPCs must resolve their configured role weapons.

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/gamemodes.md`, `docs/specs/dev-loop/dev-loop.md`, and `docs/workflows/runtime-scenario-validation.md`.
- Exact requirement: the gamemode spec says a concrete host weapon-set choice overrides role loadouts; when no explicit host selection exists, mode and role loadout precedence remains in force. The dev-loop spec requires the exact selected executable and its live journal for runtime claims.
- Why the change follows the specification: a stale one-shot profile/override value is no longer treated as a fresh host selection after choosing a new Auto mode in the dashboard. Server mode, role assignment, and role-owned loadouts remain authoritative.
- Conflicts or decisions: none. This did not change native game code or the explicit weapon-set behavior for other launch modes.

# Exact implementation changes

## File: `devscripts/dev-loop.py`

- Lines/functions/headings: `DevLoop.launch_for_control_request`, dashboard `build-or-launch`, and `set-launch-mode` control handlers.
- Old content or behavior: dashboard Auto launches could retain a one-shot weapon override through mode selection, and a profile or request value could still become an explicit `--weapon-set` argument.
- New content or behavior: each launch reloads current profile and mode data; changing the selected mode clears one-shot map/weapon overrides; dashboard launch in an Auto mode discards stale weapon values before constructing server arguments.
- Reason: the live run `20261117_221239` still had `--weapon-set 12` even after the profile was Auto, while the selected mode was Juggernaut.
- Why unrelated behavior is preserved: an explicitly configured non-Auto launch preset still passes its concrete weapon set; the mode's own role rules are unchanged.

## Supporting configuration already present for this task

- `devscripts/dev-launch-modes.json`, mode 9: `weapon_set: auto` prevents profile fallback from adding a weapon argument.
- `devscripts/dev-profiles/npc-navigation.json`: `weapon_set: auto` keeps its SSBM default mode-driven and avoids globally pinning its development profile to Captain Falcon.
- `docs/regressions/dev-loop/README.md` and `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md`: status now records the fresh GUI proof and remaining visual/in-game command review.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `devscripts/dev-loop.py` | stale overrides survived mode selection | Auto launch clears mode-scoped stale overrides | Prevents Captain Falcon override leaking into Juggernaut | Restore only these handler changes |
| `docs/regressions/dev-loop/README.md` | Entry 28 pending | Entry 28 records runtime evidence | Regression index | Restore Entry 28 text if needed |
| `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md` | runtime pending | verified GUI run and precise evidence | Regression record | Restore previous contents if needed |
| `docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md` | did not exist | this session's final record | Required session history | Remove only this new changelog |

# Contract / ABI changes

- Contract: none.
- Version bump: none.
- Compatibility: no protocol or ABI changes.
- Reverse: restore only the two launch-control handler edits.

# State / schema changes

- Domain: none.
- Old schema: not applicable.
- New schema: not applicable.
- Migration: none.
- Reverse: not applicable.

# Deleted paths

- Path: none.
- Before:
- Why:
- Reverse: not applicable; no paths were deleted.

# Diagnostics

- Owner/category: `DevLoop` GUI launch configuration.
- Input: selected launch mode and dashboard launch request.
- Decision: reload profile/mode, clear one-shot overrides on mode selection, and do not treat stale weapon state as explicit for Auto GUI modes.
- Output: server args omit `--weapon-set`; runtime config reports `gamemode_default`; NPC role resolver logs the chosen role set and weapons.
- Failure or rejection reason: not applicable for successful launch. Earlier runs gave evidence in `--weapon-set 12` and `captain_falcon_moveset`.
- Rate limiting: no diagnostics added.

# Validation

- Focused skill paths and results: reviewed `docs/skills/spec-behavior-review-v1.md`; traced the launch owner, mode policy, and exact live journal. `docs/workflows/runtime-scenario-validation.md` followed for GUI launch and server journal review.
- Tests and exact commands: no tests added or run. Read-only evidence commands: `python devscripts/dev-loop.py --status`; `Select-String` over run `20261117_221550` launch and server journals.
- Build status: no build request or build run; this is Python launch logic and JSON configuration. Existing published build generation 328, artifact `2026-11-17T21:12:47.128057Z`, matched between server and client.
- Runtime or hot-reload evidence: dashboard selected mode 9 and launched run `20261117_221550`. The server args contain no `--weapon-set`; `gamemode.match.configured` says `weapon_set_id=1`, `weapon_set_source=gamemode_default`; four Juggernaut NPCs resolve role set 11 with Heavy Shotgun, Large Machine Gun, Force Punch. Fighter NPCs and player's mode-transition spawn resolve role set 10. The launcher logged successful server/client readiness. Journal paths are listed below.
- Output files: this changelog, [regression record](../../regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md), and [dev-loop regression index](../../regressions/dev-loop/README.md).

# Measured evidence

- Before values: run `20261117_221239` launched mode 9 with `--weapon-set 12`; NPC attack-gate records contained `captain_falcon_moveset`.
- After values: run `20261117_221550` launched mode 9 with no explicit weapon-set argument; four Juggernaut NPCs resolve weapon set 11 and role weapon list `heavy_shotgun`, `large_machine_gun`, `force_punch`, `nothing`.
- Timestamps: after run `20261117_221550` was created at `2026-11-17T22:15:50Z`, loadout events observed around `2026-11-17T22:15:57Z`; launcher complete at `2026-11-17T22:16:04Z`.
- Tick/frame/network measurements: the server resolved the mode at tick 0; Juggernaut NPC roles were logged during roster adoption at tick 222. No performance claim is made.

# Regression review

- Regression entry appended: yes — existing Entry 28 detail was updated with the confirming live run.
- Why this is or is not a confirmed regression: a GUI-selected Juggernaut mode previously launched with an unintended Captain Falcon override, which changed actor loadouts.
- Related regression paths: `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: dashboard GUI launch, server readiness, gamemode resolution, and NPC role weapon resolution verified from live events. No manual interaction with the rendered game was performed.
- Multiplayer review: not performed beyond the local dev-loop server/client pair.
- Still unverified: a separate in-game `modestart` command and visual weapon model review. The fresh GUI launch path is verified.

# Agent handoff context

- Owner: dev-loop launch mode
- Focus: devscripts/dev-loop.py launch mode selection and logs/2026-11-17/20261117_221550
- Files to check: `devscripts/dev-loop.py`, launch mode 9 in `devscripts/dev-launch-modes.json`, `npc-navigation.json`, and the regression record.
- What another agent should not redo: the fresh GUI mode 9 launch and exact journal inspection are already complete; do not diagnose this as the user choosing the wrong mode.
- Next exact step: if further evidence is needed, issue `modestart 9` in-game on a session without a host weapon-set override and inspect the resulting role/loadout events.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82", "dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "published_utc": "2026-11-17T20:36:28.167Z", "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 13.098, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-17T21:52:51.348Z"}}
- Generation: 328
- Artifact: build\game\generation-328\game-g328.dll
- Journal path: C:\mimita-v9\logs\2026-11-17\20261117_221550\server-01\events.jsonl
- Session ID: session-20261117-juggernaut-gui-weapon-set-fix

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: none; regression detail is the tracking record.

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
?? docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md
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
 devscripts/dev-loop.py                             | 22 ++++++++++++++++
 devscripts/dev-profiles/npc-navigation.json        |  2 +-
 docs/regressions/dev-loop/README.md                | 16 +++++++++++-
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 ++++++++++++++--------
 docs/specs/open-gameplay-issues.md                 | 13 +++++++++-
 14 files changed, 83 insertions(+), 26 deletions(-)
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
