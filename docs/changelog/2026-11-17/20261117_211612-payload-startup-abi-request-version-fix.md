// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Payload startup ABI request version fix

# Task

- Task ID: session-20261117-211535
- Session ID: session-20261117-211535
- Owner: Codex
- Focus: Fix Payload mode 57 startup rejection and verify a real dev-loop launch
- Summary: Aligned the server startup, world-load, and configure request version defaults with policy ABI v2; launched the current dev-loop artifact in Payload mode 57 and verified the server/client runtime.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-17T21:16:12+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

At first inspection, the shared tree already contained networking cutover,
performance, dev-loop, and documentation work. During this session, commit
`43b89938b8aa29e3bffab982a544972f8dad7968` (`now mimtia launches`) appeared on
the current branch, authored by jorj1357. It includes the ABI defaults
correction and regression files touched here, along with concurrent work. I did
not create that commit. At final capture, unrelated local changes remained in
`config/accounts/default.json`, `config/audio/music-settings.json`,
`config/video-settings.json`, and the `devscripts/blender-ai` submodule pointer;
they were preserved.

# Requested behavior

Run MiMITA through the existing root dev-loop in Payload mode, keep its game
window visible, and continue until the exact published executable starts the
Payload server and client.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/dev-loop.md` (launch flow
  lines 110–114; build identity and journal requirements lines 157–169) and
  `docs/workflows/runtime-scenario-validation.md`.
- Exact requirements: launch the matching server/client pair and capture
  version, process, journal, and readiness evidence; prove runtime behavior
  from the exact executable's journals.
- Why the change follows the specification: request defaults now match the
  ABI version enforced by the game-owned server policy, and the fix was checked
  in a fresh mode 57 server/client run.
- Conflicts or decisions: none. Build publication alone was not treated as
  launch proof.

# Exact implementation changes

## File: `src/host/abi/game-server-abi.h`

- Lines/functions/headings: `ServerStartupV1` line 98,
  `ServerWorldLoadV1` line 135, `ServerConfigureV1` line 156.
- Old content or behavior: each request's `version` default was literal `1`.
  `server-policy-module.cpp::validStartup`, `validWorldLoad`, and
  `validConfigure` require `kGameServerPolicyAbiVersion`, which is `2`.
- New content or behavior: all three request defaults use
  `kGameServerPolicyAbiVersion`.
- Reason: value-initialized resident requests were rejected before mode
  configuration; aligning the later request defaults also prevents rejection
  at world-load/configure stages.
- Why unrelated behavior is preserved: struct layouts and other startup values
  are unchanged; only the version default follows the existing ABI constant.

The source correction is present in commit
`43b89938b8aa29e3bffab982a544972f8dad7968`, authored by jorj1357 during this
session. I did not create that commit.

## Regression record

- `docs/regressions/networking/README.md`: added index entry 16.
- `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md`:
  recorded the failed run, cause, correction, and fresh runtime evidence.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `src/host/abi/game-server-abi.h` | Three request structs defaulted to version `1` | Defaults use `kGameServerPolicyAbiVersion` (`2`) | Server policy's version validation rejected Payload startup | Do not revert alone; it restores the launch failure. |
| `docs/regressions/networking/README.md` | 15 entries | Added startup ABI regression entry 16 | Index confirmed failure and fix evidence | Remove only with a deliberate record correction |
| `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md` | No record | New failure/cause/fix/runtime record | Append-only regression evidence | Remove only with a deliberate record correction |
| `docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md` | Not present | This session record | Required repository session record | Preserve as audit history |

# Contract / ABI changes

- Contract: server policy request version fields.
- Version bump: none in this fix; aligns initialization with the existing ABI
  v2 constant.
- Compatibility: current executable and game generation both expose ABI v2.
- Reverse: reverting these defaults causes the validation failure again.

# State / schema changes

- Not applicable; no persisted state or schema changed.

# Deleted paths

- None.

# Diagnostics

- Owner/category: `src/network/server.cpp::runServer`, NETWORK
  `server.startup.stage`; validator owner is
  `src/game/server-policy-module.cpp::validStartup`.
- Input: value-initialized `ServerStartupV1`, `ServerWorldLoadV1`, and
  `ServerConfigureV1` requests.
- Decision: ABI validator compares each request version and size with the
  active generation's contract.
- Output: prior run logged `invalid server startup request`; corrected run
  completed configuration/world/map stages and emitted `payload.configured`.
- Failure or rejection reason: request version `1` did not equal ABI `2`.
- Rate limiting: startup emits one event per lifecycle stage; no new logging
  was added.

# Validation

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` —
  pass; `docs/skills/logging-checker-v1.md` — pass; existing startup-stage
  logging exposed the failing owner and reason. Computer-use skill screenshot
  inspection confirmed a visible `mimita.exe` window with Payload HUD.
- Tests and exact commands: no tests added or run. Observed with
  `python devscripts/dev-loop.py --status`, `--list-instances`, and a normal
  `--request launch --launch-mode 57`.
- Build status: root dev-loop automatically published source generation 98 at
  `2026-11-17T21:12:47.128057Z`; artifact is current and non-stale.
- Runtime or hot-reload evidence: run `20261117_211306`; managed mode launch
  completed on mode 57. Server PID 5636 and client PID 6608 used build
  `2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac`.
- Output files: server journal
  `logs/2026-11-17/20261117_211306/server-01/events.jsonl`; client journal
  `logs/2026-11-17/20261117_211306/client-01/events.jsonl`; loop journal
  `logs/2026-11-17/20261117_211306/dev-loop/events.jsonl`.

# Measured evidence

- Before values: server startup failed at `configuration_load` with
  `invalid server startup request` in run `20261117_210637`.
- After values: configuration loaded for `payload` on `funworld3`; map config
  applied with 19 startup NPCs; `payload.configured` emitted; room `K2TTN85`
  registered; client connected to `127.0.0.1:62301` and applied authoritative
  snapshots containing the NPCs.
- Timestamps: failed run `2026-11-17T21:06:37Z`; corrected artifact published
  `2026-11-17T21:12:47.128057Z`; fresh run started `2026-11-17T21:13:06Z`.
- Tick/frame/network measurements: client applied snapshots at server ticks
  206 and 299. This proves connection/snapshot flow, not broad gameplay.

# Regression review

- Regression entry appended: yes.
- Why this is or is not a confirmed regression: the exact server journal
  showed failure before configuration; the corrected build completed Payload
  startup and client connection in a fresh run.
- Related regression paths:
  `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md`.

# Human acceptance

- Visual review: visible `mimita.exe` window observed with Payload HUD and
  intermission. This was agent inspection, not human acceptance; the screenshot
  also showed an open console overlay and a dark scene.
- Gameplay review: startup, Payload config, room registration, client
  connection, and snapshots confirmed. No human play session was performed.
- Multiplayer review:
- Still unverified: human visual/play acceptance, additional clients, and
  sustained Payload gameplay.

# Agent handoff context

- Owner: Codex
- Focus: Fix Payload mode 57 startup rejection and verify a real dev-loop launch
- Files to check: ABI request defaults in `src/host/abi/game-server-abi.h`,
  regression record above, and run `20261117_211306` journals.
- What another agent should not redo: do not re-investigate the old run's
  generic startup rejection; it was caused by request defaults at ABI v1.
- Next exact step: if continuing beyond launch confirmation, review the live
  Payload scene without replacing this currently running user session.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82", "dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "published_utc": "2026-11-17T20:36:28.167Z", "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ready"}, "progress": {"compiled": 6, "elapsed_s": 17.401, "phase": "build_complete", "result": "success", "skipped": 81, "updated_utc": "2026-11-17T21:12:46.384Z"}}
- Generation: 328
- Artifact: build\game\generation-328\game-g328.dll
- Journal path: C:\mimita-v9\logs\2026-11-17\20261117_211306\server-01\events.jsonl
- Session ID: session-20261117-211535

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: none; this is a startup regression correction, not a feature
  acceptance record.

## Captured git state

```text
M config/accounts/default.json
 M config/audio/music-settings.json
 M config/video-settings.json
 m devscripts/blender-ai
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  6 ++---
 config/audio/music-settings.json                   |  2 +-
 config/video-settings.json                         |  2 +-
 devscripts/blender-ai                              |  0
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 ++++++++++++++--------
 6 files changed, 25 insertions(+), 17 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/audio/music-settings.json
M	config/video-settings.json
M	devscripts/blender-ai
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
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
