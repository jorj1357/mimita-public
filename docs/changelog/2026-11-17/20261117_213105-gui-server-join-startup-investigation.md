// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# GUI server join startup investigation

# Task

- Task ID: session-20261117-gui-server-join-investigation
- Session ID: session-20261117-gui-server-join-investigation
- Owner: dev-loop process ownership and GUI server startup
- Focus: logs/2026-11-17/20261117_211306; devscripts/dev-loop.py; src/gui/gui-main.cpp
- Summary: Traced the GUI-created server's pre-room-code exit and found a separate dev-loop process-ownership regression. No source changes or builds were made; first exit cause remains unknown.
- Status: INVESTIGATION — process ownership regression confirmed; GUI server exit cause unresolved
- Date, time, timezone: 2026-11-17T21:31:05+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The initial `git status --short` showed existing unrelated changes to
  configuration, the Blender AI submodule, gameplay-issue docs, and the
  networking regression record. Those changes were preserved.
- The two existing changelog files listed in that status were from earlier
  sessions and were not modified here.
- The dev loop was already active on run `20261117_213000` (sandbox, server
  PID `14700`, client PID `26176`). It was left running.

# Requested behavior

# Specification alignment

- [docs/specs/dev-loop/dev-loop.md](../../specs/dev-loop/dev-loop.md), sections
  2 and 8.1–8.2: process termination requires proven launch/adoption ownership
  and verified identity; if ownership cannot be proven, report
  `NEEDS_USER_ACTION`.
- [docs/specs/dev-loop/multi-instance.md](../../specs/dev-loop/multi-instance.md):
  external processes remain outside active-run ownership and may be stopped
  only by explicit verified process action.
- `checkout_mimita_processes` finds every same-checkout executable, while
  `reap_stray_mimita_processes` targets each PID without an ownership check.
  This mismatch is recorded as an open regression. No fix was made because
  this request was to investigate the user's observed exit, and the earlier
  PID `30824` termination remains unexplained.

# Exact implementation changes

- No source implementation changes.
- Added the open process-ownership regression record and its dev-loop index
  entry. Added this session's changelog.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `docs/regressions/dev-loop/README.md` | Appended entry 27 and updated count | Remove only entry 27 and restore the prior count |
| `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md` | Added evidence-backed open regression | Remove this new record |
| This changelog | Added investigation evidence | Remove this changelog |

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

- Read the dev-loop ownership and multi-instance specifications and inspected
  the exact source/log evidence cited in the regression record.
- No tests were run; this was a forensic investigation and no implementation
  code changed.
- No build was requested or run. `dev-loop.py --status` reported an existing
  published build as current; it is not evidence for the GUI server attempt.
- Runtime evidence is from prior run `20261117_211306`; no new GUI launch or
  Payload acceptance was performed. The unrelated active sandbox run was
  preserved.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes — Entry 27 in
  `docs/regressions/dev-loop/README.md`.
- Confirmed regression: the dev-loop invoked `taskkill` on GUI server PID
  `14608` without proving active-run ownership. The return code was `128`, so
  whether this command terminated PID `14608` is not established.
- The earlier GUI server PID `30824` exited with `0xC000013A` before logger
  startup. Its sender/cause is unknown; this record does not claim the process
  reaper caused that earlier exit.
- Related record:
  `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md`.

# Human acceptance

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Owner: dev-loop process ownership and GUI server startup
- Focus: logs/2026-11-17/20261117_211306; devscripts/dev-loop.py; src/gui/gui-main.cpp
- Files to check: `devscripts/dev-loop.py` process discovery and reaper;
  `src/gui/gui-main.cpp` GUI child journal setup; referenced run journals.
- Do not repeat the initial log search. Preserve active run `20261117_213000`.
- Next exact step: repair reaper ownership filtering under the existing
  dev-loop workflow, then reproduce GUI server launch in Payload and collect
  an independent server journal. Separately trace the sender of the
  `0xC000013A` exit; do not infer it from this audit.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82", "dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "published_utc": "2026-11-17T20:36:28.167Z", "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ready"}, "progress": {"compiled": 6, "elapsed_s": 17.401, "phase": "build_complete", "result": "success", "skipped": 81, "updated_utc": "2026-11-17T21:12:46.384Z"}}
- Generation: 328
- Artifact: build\game\generation-328\game-g328.dll
- Journal path: C:\mimita-v9\logs\2026-11-17\20261117_213000\server-01\events.jsonl
- Session ID: session-20261117-gui-server-join-investigation

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
?? docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  6 ++---
 config/analytics.json                              |  2 +-
 config/audio/music-settings.json                   |  6 ++---
 config/movement.json                               |  3 ++-
 devscripts/blender-ai                              |  0
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 ++++++++++++++--------
 docs/specs/open-gameplay-issues.md                 | 13 +++++++++-
 8 files changed, 41 insertions(+), 21 deletions(-)
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
