// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# GUI room startup join state

# Task

- Task ID: session-20261119-004818
- Session ID: session-20261119-004818
- Owner: Codex
- Focus: src/gui/gui-main.cpp GUI local room join state
- Summary: Removed premature gameplay state during local GUI room join and recorded the white-screen regression with the unresolved server exit cause.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-19T00:48:20+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 1726e34b48314cb6a3a88b5401c7b7770aa83f13
- Final commit: 1726e34b48314cb6a3a88b5401c7b7770aa83f13

# Pre-existing changes

- Exact status output is preserved in the captured git state below. It includes
  unrelated edits to `config/accounts/default.json`, `config/analytics.json`,
  `devscripts/blender-ai`, `docs/ROUTER.md`, `docs/specs/gamemodes/gamemodes.md`,
  and new Zombie Attack / weapon-authoring documents. None was edited by this
  session.
- Files not created or modified by this session: all pre-existing paths above.

# Requested behavior

Investigate and stop the white gameplay screen seen when joining a GUI-created
Juggernaut Horde room on `juggernaut6`, then record why the regression occurred.

# Specification alignment

- Current specification paths: `docs/specs/networking/networking.md`,
  `docs/specs/dev-loop/dev-loop.md`,
  `docs/workflows/runtime-scenario-validation.md`.
- Exact requirement: networking spec, “Loading and reconnecting presentation
  contract”: “An initial join or game/map synchronization is **Loading**, with
  the current stage and total elapsed time visible to the client, until local
  readiness is reached.”
- Removing the GUI's premature gameplay assignment leaves connection/map
  state transitions to the network owner, which already enters loading on the
  required-map step and enters play after it resolves.
- Runtime acceptance remains open because the exact reported scenario could
  not be tied to a fresh, coherent client/server journal in this session.

# Exact implementation changes

## File: `src/gui/gui-main.cpp`

- Lines/functions/headings: `launchServerPresetAndJoin`, local `queueLocalJoin`
  lambda (around line 560).
- Old behavior: set `GAME_STATE = GAME_PLAYING` before calling asynchronous
  `MimitaNet::mpInit`.
- New behavior: reset the local player and begin the direct UDP join without
  forcing gameplay state; the network path owns Loading and Playing transitions.
- Reason: avoid rendering an unaccepted, mapless local join as gameplay.
- Unrelated behavior preserved: address, requested map, player name, and UDP
  join initialization are unchanged.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `src/gui/gui-main.cpp` | Eager gameplay-state assignment in local join queue | Assignment removed | Prevent mapless asynchronous join from being shown as gameplay | Restore `GAME_STATE = GAME_PLAYING;` in the lambda |
| `docs/regressions/networking/README.md` | 16 entries | 17 entries | Index the new record | Remove Entry 17 and restore count |
| `docs/regressions/networking/gui-room-startup-white-screen-REG-2026-11-18.md` | Absent | New regression record | Preserve evidence, cause limits, and follow-up | Delete this file |
| `docs/changelog/2026-11-19/20261119_004821-gui-room-startup-join-state.md` | Absent | This session's changelog | Required session record | Delete this file |

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

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md`
  applied; cause separated into the evidenced client-state defect and
  unresolved server-process exit. `docs/skills/documentation-checker-v1.md`
  applied; regression links and scope reviewed.
- Tests and exact commands: none run; the user did not request tests, and the
  task is not proven by a synthetic check.
- Build status: No explicit build request or build wait was issued; the dev-loop last
  reported pending/stale status for the pre-existing runtime artifact.
- Runtime evidence: prior historical journal only at
  `logs/2026-11-17/20261117_211306/client-01/events.jsonl`; current run/process
  identity was incoherent, so no fresh runtime proof.
- Output files: source mitigation, networking regression record/index, this
  changelog.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes; networking Entry 17.
- The premature gameplay transition is source-confirmed, but attribution to
  the user's exact Juggernaut reproduction is unverified. The hosted server's
  earlier exit cause remains unknown.
- Related regression paths: `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md` (separate, not causal); `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md` (different failure).

# Human acceptance

- Visual review: no white-screen reproduction or post-change visual review.
- Gameplay review: no Juggernaut Horde room created after this source change.
- Multiplayer review: no fresh same-run client/server evidence.
- Still unverified: compiled executable behavior, visible loading/error state,
  and the cause of the local server process exit.

# Agent handoff context

- Owner: Codex
- Focus: src/gui/gui-main.cpp GUI local room join state
- Files to check: `src/gui/gui-main.cpp`, the regression record, and paired
  client/server journals from a fresh reproduction.
- What another agent should not redo: re-triage the older PID 30824 exit as
  though `0xC000013A` identifies its cause; it does not.
- Next exact step: after a human-authorized build is published, reproduce
  GUI-created Juggernaut Horde on `juggernaut6`, inspect both owned journals,
  and capture server exit diagnostics if it exits again.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 1726e34b48314cb6a3a88b5401c7b7770aa83f13 / 1726e34b48314cb6a3a88b5401c7b7770aa83f13
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "pending", "latest_build_stale": true, "latest_successful_build": "2026-11-18T10:05:30.909128Z", "latest_successful_build_id": "2026-11-18T10:05:30.909128Z:50972d79618bc0b72220dc0afba59b5e6890c7a0fb878a8245ce59d70c5d08c5", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "eac97971d34d03cbda79c4740797bb34485e023b0963c6bd0ad4726fb83be5c9", "dll": "build\\game\\generation-344\\game-g344.dll", "generation": 344, "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-344\\game-g344.dll", "generation": 344, "published_utc": "2026-11-18T09:50:03.870Z", "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 21.664, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-18T10:16:50.569Z"}}
- Generation: 344
- Artifact: build\game\generation-344\game-g344.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_100534\client-01\events.jsonl
- Session ID: session-20261119-004818

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
 M docs/ROUTER.md
 M docs/regressions/networking/README.md
 M docs/specs/gamemodes/gamemodes.md
 M src/gui/gui-main.cpp
?? docs/changelog/2026-11-18/20261118_111500-zombie-attack-mode-spec.md
?? docs/changelog/2026-11-19/
?? docs/regressions/networking/gui-room-startup-white-screen-REG-2026-11-18.md
?? docs/specs/creator-tools/weapon-authoring.md
?? docs/specs/creators/
?? docs/specs/gamemodes/zombie-attack.md
```

## Captured diff stat

```text
config/accounts/default.json          | 2 +-
 config/analytics.json                 | 2 +-
 devscripts/blender-ai                 | 0
 docs/ROUTER.md                        | 1 +
 docs/regressions/networking/README.md | 9 ++++++++-
 docs/specs/gamemodes/gamemodes.md     | 1 +
 src/gui/gui-main.cpp                  | 1 -
 7 files changed, 12 insertions(+), 4 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	devscripts/blender-ai
M	docs/ROUTER.md
M	docs/regressions/networking/README.md
M	docs/specs/gamemodes/gamemodes.md
M	src/gui/gui-main.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "pending",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-18T10:05:30.909128Z",
    "latest_successful_build_id": "2026-11-18T10:05:30.909128Z:50972d79618bc0b72220dc0afba59b5e6890c7a0fb878a8245ce59d70c5d08c5",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "eac97971d34d03cbda79c4740797bb34485e023b0963c6bd0ad4726fb83be5c9",
    "dll": "build\\game\\generation-344\\game-g344.dll",
    "generation": 344,
    "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-344\\game-g344.dll",
    "generation": 344,
    "published_utc": "2026-11-18T09:50:03.870Z",
    "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 21.664,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-18T10:16:50.569Z"
  }
}
```
