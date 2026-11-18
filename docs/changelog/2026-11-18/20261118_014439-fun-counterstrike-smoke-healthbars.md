// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Fun Counterstrike smoke healthbars

# Task

- Task ID: session-20261118-014436
- Session ID: session-20261118-014436
- Owner: Codex
- Focus: smoke-aware healthbar visibility
- Summary: Cull live-world actor healthbars, names, and aim markers when client grenade smoke intersects the camera-to-anchor segment; document the behavior. The game DLL change is saved, but current dev-loop status still shows the older published generation, so same-PID activation and visual result are unverified.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T01:44:38+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The checkout had unrelated edits in other configuration, movement, dev-loop,
  regression, and issue-tracker files. Earlier changelogs and other untracked
  files were preserved.
- The previous turn's `config/gamemodes/funcounterstrike.json` edits (team
  healthbars on, player/friendly outlines off) were preserved.
- The five source files and `docs/specs/gamemodes/funcounterstrike.md` listed
  below were clean before this task.

# Requested behavior

Hide Fun Counterstrike's actor healthbars when grenade smoke blocks the view
between the local camera and the actor.

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/funcounterstrike.md`,
  `docs/specs/hotreload/hotreload.md`,
  `docs/specs/graphics/graphics-presentation.md`, and
  `docs/architecture/json-configuration/json-configuration.md`.
- The FCS smoke section already defines the world volume and NPC LOS behavior.
  The change extends presentation so live-world actor nameplates/healthbars
  hide when the camera-to-anchor segment intersects active grenade smoke.
- Both implementation `.cpp` files are game-owned in the source manifest, so
  this is eligible for a game-DLL hot edit.
- No spec conflict found. Replay overlays are explicitly unaffected.

# Exact implementation changes

## File: `src/network/multiplayer-projectiles.cpp`

- Added `mpClientSmokeBlocksSegment`, reusing the active client grenade-smoke
  spheres already used by the renderer. The segment-to-sphere query adds no
  second smoke state and no allocation.

## Files: `src/gui/hud/player-nameplates.cpp` and `.h`

- `drawPlayerHealthbar` now returns `SmokeOccluded` for live-world actors before
  drawing their healthbar, name, and aim triangle. Replay tags skip the query.
- `healthbarCullReasonName` exposes `smoke-occluded`; the existing once-per-
  second network healthbar debug output reports this reason when enabled.

## File: `src/network/multiplayer-context.h`

- Declares the client projectile owner's smoke-segment query for the HUD.

## File: `docs/specs/gamemodes/funcounterstrike.md`

- Documents smoke hiding live actor healthbars, names, and aim markers when it
  lies between the camera and the actor's healthbar anchor.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `src/network/multiplayer-projectiles.cpp` | smoke volumes render only | exported active-volume segment query | Reuse client smoke owner | Revert `mpClientSmokeBlocksSegment` |
| `src/network/multiplayer-context.h` | no query declaration | declares smoke query | Share owner API with HUD | Remove declaration |
| `src/gui/hud/player-nameplates.cpp` | bars ignore smoke | live bars/name/aim marker cull on intersection | Match smoke visibility | Remove query and cull block |
| `src/gui/hud/player-nameplates.h` | no smoke cull reason | adds `SmokeOccluded` | Report cull decision | Remove enum value |
| `docs/specs/gamemodes/funcounterstrike.md` | no HUD smoke rule | documents occlusion | Record requested behavior | Remove added smoke bullet |
| `docs/changelog/2026-11-18/20261118_014439-fun-counterstrike-smoke-healthbars.md` | absent | this record | Required session record | Remove this file only |

# Contract / ABI changes

- Contract: game-internal C++ helper declaration only; host ABI unchanged.
- Version bump: none.
- Compatibility: unchanged.
- Reverse: revert the helper and renderer/spec edits listed above.

# State / schema changes

- None.

# Deleted paths

- None.

# Diagnostics

- Owner/category: existing healthbar renderer and HUD debug overlay.
- Input: camera position, healthbar anchor, and active client smoke spheres.
- Decision/output: segment intersection yields
  `HealthbarCullReason::SmokeOccluded` / `smoke-occluded`.
- Non-intersecting segments use existing distance, projection, and draw checks.
- Rate limiting: existing network debug overlay reports cull reason at most
  once per second; no per-frame structured event was added.

# Validation

- `docs/skills/spec-behavior-review-v1.md`: PASS; traced the existing smoke
  volume owner and healthbar presentation call.
- `docs/skills/documentation-checker-v1.md`: PASS; added one concise smoke
  visibility rule to the FCS smoke section.
- No tests were requested or run. The diff was manually inspected.
- `build/source-manifest.json` assigns both changed `.cpp` owners to `game`.
  Dev-loop was checked once after saving: run `20261118_005857`, mode 45 / Fun
  Counterstrike, client PID 3500 and server PID 23968 remain alive. It still
  reported generation 328 and the earlier build/source identity. No build
  request was made and no build was awaited.
- Same-PID DLL activation, live smoke scenario, and visual acceptance remain
  unverified; the root dev-loop is the only builder.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- No regression entry appended; this is new requested behavior, not a confirmed
  regression.
- Related regression paths: none identified.

# Human acceptance

- Visual review: pending. Confirm bars/nameplates disappear behind grenade
  smoke and reappear after the view ray clears or the smoke expires.
- Gameplay and multiplayer review: pending.
- Still unverified: DLL compile/publication/activation and live FCS smoke result.

# Agent handoff context

- Owner: Codex
- Focus: smoke-aware healthbar visibility
- Files to check: changed source above and
  `logs/2026-11-18/20261118_005857/client-01/events.jsonl`.
- What another agent should not redo: the client smoke-volume query and
  `smoke-occluded` healthbar path are in source.
- Next exact step: when the dev-loop publishes the revision, confirm same-PID
  activation and inspect the existing healthbar debug cull reason while testing
  a smoke grenade.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "f75a00950042193606ba0cd75ed6d63911b86226dc5f94e2fd9f60ff5cc81b82", "dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-328\\game-g328.dll", "generation": 328, "published_utc": "2026-11-17T20:36:28.167Z", "revision": "2aa19618b08ee112f7f0d5fff2b0cda93ba7f29ea9b48617f214a431b3d814ae", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 13.098, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-17T21:52:51.348Z"}}
- Generation: 328
- Artifact: build\game\generation-328\game-g328.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_005857\client-01\events.jsonl
- Session ID: session-20261118-014436

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
 M docs/specs/gamemodes/funcounterstrike.md
 M docs/specs/open-gameplay-issues.md
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
?? config/movement/movement-deliberate.json
?? docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md
?? docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md
?? docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md
?? docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md
?? docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md
?? docs/changelog/2026-11-18/
?? docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md
?? docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  4 +--
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
 docs/specs/gamemodes/funcounterstrike.md           |  3 +++
 docs/specs/open-gameplay-issues.md                 | 13 +++++++++-
 src/gui/hud/player-nameplates.cpp                  |  8 ++++++
 src/gui/hud/player-nameplates.h                    |  3 ++-
 src/network/multiplayer-context.h                  |  4 +++
 src/network/multiplayer-projectiles.cpp            | 19 ++++++++++++++
 21 files changed, 125 insertions(+), 28 deletions(-)
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
M	docs/specs/gamemodes/funcounterstrike.md
M	docs/specs/open-gameplay-issues.md
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
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
