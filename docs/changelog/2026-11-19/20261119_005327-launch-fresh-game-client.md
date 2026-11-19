// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Launch fresh game client

# Task

- Task ID: session-20261119-005325
- Session ID: session-20261119-005325
- Owner: Codex
- Focus: dashboard launch semantics and fast client entry
- Summary: Make the dashboard's bottom launch button replace existing clients, open one fresh client, and retain a healthy matching server; keep Start client for adding clients.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-19T00:53:27+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 1726e34b48314cb6a3a88b5401c7b7770aa83f13
- Final commit: 1726e34b48314cb6a3a88b5401c7b7770aa83f13

# Pre-existing changes

- The shared checkout already contained edits/untracked files in
  `config/accounts/default.json`, `config/analytics.json`, `devscripts/blender-ai`,
  `docs/ROUTER.md`, networking regressions, game-mode specs, and `src/gui/gui-main.cpp`.
  Those were preserved. `devscripts/dev-loop.py` and the two dev-loop specs also
  contained earlier unrelated edits; this session changed only the launch path,
  process-reconciliation function, and relevant control-contract paragraphs.
  See the captured git state below.

# Requested behavior

- Make **Launch latest stable MiMITA.exe** the fastest route into the game:
  close existing clients, start one fresh client from the latest valid published
  build, keep a healthy matching server, and leave additional clients to
  **Start client**. Do not make the launch button wait for a build.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/dev-loop.md` sections 3
  and 3.1; `docs/specs/dev-loop/dashboard.md`, Process controls and feedback.
- Exact requirements: the explicit bottom-button action replaces same-checkout
  clients only, verifies target identity and role before stopping, preserves a
  healthy matching server, and starts one client. **Start client** adds further
  clients. **Build** is an explicit separate action.
- Why the change follows the specification: `launch_latest` now always takes
  the fresh-client path; the client reconciliation scan distinguishes server
  command lines and leaves server PIDs alive; dashboard `build-or-launch`
  launches the latest valid published artifact immediately and clears any
  deferred launch-after-build intent.
- Conflicts or decisions: if process role is missing, launch fails closed rather
  than risking termination of a server. A mismatched server can still be
  replaced by existing build/configuration rules.

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings: `devscripts/dev-loop.py` control action
  `build-or-launch`, `DevLoop.launch_latest`, and
  `DevLoop.reap_stray_mimita_processes`; Process controls and feedback in
  `docs/specs/dev-loop/dashboard.md`; launch replacement paragraph in
  `docs/specs/dev-loop/dev-loop.md`.
- Old content or behavior: the button could wait for a new build, return early
  by reusing a healthy existing client, stop every same-checkout MiMITA process
  including servers, and start extra clients from mode configuration.
- New content or behavior: the button immediately launches the latest valid
  published build, closes identified same-checkout clients only, retains
  servers, and starts exactly one fresh client. Additional clients remain
  available through **Start client**. Build remains a separate button.
- Reason: the bottom button should get the user into one game client quickly;
  additional client startup should be intentional.
- Why unrelated behavior is preserved: identity checks remain strict; unknown
  process roles abort safely; healthy matching servers are retained while the
  existing server mismatch policy is unchanged.

## Exact session-touched files

- `devscripts/dev-loop.py` — button routing, fresh-client policy, client-only
  process replacement, and role-aware verification.
- `docs/specs/dev-loop/dashboard.md` — bottom-button and separate Build/Start
  client semantics.
- `docs/specs/dev-loop/dev-loop.md` — launch authorization and replacement
  specification.
- This changelog.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/regressions/networking/README.md` |  |  |  | git checkout -- docs/regressions/networking/README.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/dev-loop/dev-loop.md` |  |  |  | git checkout -- docs/specs/dev-loop/dev-loop.md |
| `docs/specs/gamemodes/gamemodes.md` |  |  |  | git checkout -- docs/specs/gamemodes/gamemodes.md |
| `src/gui/gui-main.cpp` |  |  |  | git checkout -- src/gui/gui-main.cpp |
| `docs/changelog/2026-11-18/20261118_111500-zombie-attack-mode-spec.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-18/20261118_111500-zombie-attack-mode-spec.md' |
| `docs/changelog/2026-11-19/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/' |
| `docs/regressions/networking/gui-room-startup-white-screen-REG-2026-11-18.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/networking/gui-room-startup-white-screen-REG-2026-11-18.md' |
| `docs/specs/creator-tools/weapon-authoring.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/creator-tools/weapon-authoring.md' |
| `docs/specs/creators/` |  |  |  | Remove-Item -LiteralPath 'docs/specs/creators/' |
| `docs/specs/gamemodes/zombie-attack.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/gamemodes/zombie-attack.md' |

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

- Focused skill paths and results: reviewed `docs/skills/logging-checker-v1.md`,
  `docs/skills/efficiency-checker-v1.md`, and
  `docs/skills/spec-behavior-review-v1.md`. No second output sink or server
  process owner was added; the replacement still verifies process identity.
- Tests and exact commands: no tests or builds were run. The allowed
  `git diff --check -- devscripts/dev-loop.py docs/specs/dev-loop/dashboard.md
  docs/specs/dev-loop/dev-loop.md` check passed, with only Git's LF-to-CRLF
  warning for the existing dev-loop spec.
- Build status: no build was requested or awaited. Automatic changelog capture
  reported root dev-loop `build_status=building`, latest successful EXE stale,
  and game DLL generation 345 ready. These facts do not prove that the current
  Python launcher edits were loaded into the live daemon or that a new EXE was
  built.
- Runtime or hot-reload evidence: not verified. The launch button was not
  pressed, and no clients or servers were terminated by this editing session.
- Output files: no runtime artifacts generated by this task.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: this is a requested change to
  explicit dashboard action semantics, not a newly observed failure in the
  changed code.
- Related regression paths: existing
  `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md` is
  relevant historical context; this change narrows cleanup to verified clients
  and does not close that separate regression.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: confirm that the edited daemon code is loaded, press the
  bottom button with a healthy server and one or more old clients, and observe
  one fresh client enter the selected mode while the server remains alive.

# Agent handoff context

- Owner: Codex
- Focus: dashboard launch semantics and fast client entry
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 1726e34b48314cb6a3a88b5401c7b7770aa83f13 / 1726e34b48314cb6a3a88b5401c7b7770aa83f13
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "building", "latest_build_stale": true, "latest_successful_build": "2026-11-18T10:05:30.909128Z", "latest_successful_build_id": "2026-11-18T10:05:30.909128Z:50972d79618bc0b72220dc0afba59b5e6890c7a0fb878a8245ce59d70c5d08c5", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "a124a1ee639728eb55c7c097d248fdf90e0fa548ffd237ac6049c8afe36b8f18", "dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "published_utc": "2026-11-19T00:53:18.768Z", "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ready"}, "progress": {"elapsed_s": 0.234, "phase": "setup", "updated_utc": "2026-11-19T00:53:21.636Z"}}
- Generation: 345
- Artifact: build\game\generation-345\game-g345.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_100534\client-01\events.jsonl
- Session ID: session-20261119-005325

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
 M devscripts/dev-loop.py
 M docs/ROUTER.md
 M docs/regressions/networking/README.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/dev-loop.md
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
config/accounts/default.json          |  2 +-
 config/analytics.json                 |  2 +-
 devscripts/blender-ai                 |  0
 devscripts/dev-loop.py                | 76 ++++++++++++++++-------------------
 docs/ROUTER.md                        |  1 +
 docs/regressions/networking/README.md |  9 ++++-
 docs/specs/dev-loop/dashboard.md      |  5 +++
 docs/specs/dev-loop/dev-loop.md       | 14 ++++---
 docs/specs/gamemodes/gamemodes.md     |  1 +
 src/gui/gui-main.cpp                  |  1 -
 10 files changed, 61 insertions(+), 50 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	docs/ROUTER.md
M	docs/regressions/networking/README.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/gamemodes/gamemodes.md
M	src/gui/gui-main.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "building",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-18T10:05:30.909128Z",
    "latest_successful_build_id": "2026-11-18T10:05:30.909128Z:50972d79618bc0b72220dc0afba59b5e6890c7a0fb878a8245ce59d70c5d08c5",
    "latest_successful_source_generation": 0
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
    "elapsed_s": 0.234,
    "phase": "setup",
    "updated_utc": "2026-11-19T00:53:21.636Z"
  }
}
```
