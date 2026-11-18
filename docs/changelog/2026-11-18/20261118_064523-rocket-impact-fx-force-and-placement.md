// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Rocket Impact FX Force and Placement

# Task

- Task ID: session-20261118-064520
- Session ID: session-20261118-064520
- Owner: Codex
- Focus: Rocket launcher FX, actor-safe impact marks, shockwave texture and alpha fade
- Summary: Scale rocket explosion visuals from projectile momentum, project actor-hit marks onto nearby world geometry, and activate the textured shockwave path.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T06:45:22+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The checkout already contained unrelated edits across config, dev-loop, NPC,
  networking, UI, specs, regression records, handoffs, and prior changelogs.
  They were preserved. The rocket config and six C++ files listed below were
  clean before this task.
- The exact initial `git status --short` was inspected at task start; unrelated
  changed paths remain listed in the automatic evidence below.

# Requested behavior

- Scale rocket impact visuals up/down with projectile impact force.
- Keep explosion marks off actor body geometry so they do not float on limbs or
  body parts; place them at the hit location's nearest sensible world surface.
- Make the shockwave visibly textured, large enough to see, and fade its alpha
  over its lifetime. Log whether the texture reaches the draw call.

# Specification alignment

- Current specifications: `docs/specs/weapons/weapons.md`, especially
  “Explosion shockwave render configuration (2026-10-29)”; `docs/ROUTER.md`;
  `docs/specs/dev-loop/agent-lanes-build-authority.md`.
- Exact requirement: weapon config defines shockwave model/texture, and a
  non-empty `shockwaveTexture` renders a surface-aligned PNG quad in preference
  to the model. Start/end alpha define opacity over the effect lifetime.
- The renderer interpolates alpha from `startAlpha` to `endAlpha`, but the GLB
- draw path lacked alpha blending. The transparent ring uses the blended
  textured-quad path; the GLB fallback also enables blending.
- Actor marks resolve to the exact closest point on static world collision
  triangles within 8 m. Actor geometry is not part of the query. If no world
  surface is found, the mark is omitted. No gameplay damage or projectile
  authority changed.

# Exact implementation changes

## `config/weapons/rocket_launcher/config.json`

- `render.explosion.shockwaveTexture`: changed from `""` to
  `"assets/textureshq/rocket-shockwave-v1.png"`; the previous Giygas image was
  an opaque tiled red pattern. The transparent ring now takes precedence over
  the GLB model. Start/end size changed from 0.1/2.5 m to 2/14 m so the textured
  quad is visible; alpha remains 1→0 over 0.18 seconds.

## `src/combat/weapon-rocket-launcher.cpp`

- `rocketImpactForceScale` compares projectile momentum (`projectileMass ×
  impact speed`) with configured nominal launch momentum and clamps scale to
  0.25–3.0.
- `findNearestWorldSurface` queries the world collision broadphase and finds
  the exact closest point on a static triangle. A reused thread-local candidate
  vector avoids per-impact candidate allocation.
- `doExplosion` emits `rocket.impact-mark.resolved` with hit class, target kind
  and ID, world triangle index, nearest distance, mark position, force scale,
  and the actual decal-spawn result. Actor hits use only world triangles within
  8 m; otherwise the mark is suppressed. World hits retain their collision
  triangle index; lifetime expiry creates no mark without a world contact.
- Force scale is passed at expiry, world hit, NPC hit, and player hit sites.

## `src/combat/explosion-fx.h/.cpp`

- Added optional force scale and actor mark anchor inputs to the shared spawner.
- Shockwave start/end scales, explosion sphere start/end scales, and mark radius
  now follow the clamped force scale. Existing callers retain scale 1 defaults.
- The shared spawner returns whether the explosion decal was actually inserted.

## `src/effects/effect-part.h/.cpp`

- `spawnExplosionMark` accepts an optional size scale and applies it to the
  configured decal radius; it returns false when marks are disabled. Default
  callers preserve their previous size.

## `src/effects/effect-part-render.cpp` and `src/effects/effect-part.h`

- Textured shockwave drawing reports successful GPU submission and falls back
  to the configured GLB if its texture cannot load.
- The model fallback enables alpha blending and disables depth writes while
  drawing, so its alpha interpolation is visible.
- `explosion.shockwave.draw` emits at most twice per effect: first successful
  draw and first draw at alpha <= 0.5. It records path, texture/model, scale,
  alpha, age fraction, and position.

## `assets/textureshq/rocket-shockwave-v1.png`

- Added a transparent annular energy-ring texture. It is 1254×1254 RGBA; corner
  alpha and center alpha are 0.

Reason: fulfill the requested force-responsive explosion appearance while
anchoring actor-hit marks to world geometry. Other weapon callers use defaults.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `assets/textureshq/rocket-shockwave-v1.png` | absent | transparent shockwave ring | visible shockwave | remove this new file |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/camconfig.json` |  |  |  | git checkout -- config/camconfig.json |
| `config/gamemodes/funcounterstrike.json` |  |  |  | git checkout -- config/gamemodes/funcounterstrike.json |
| `config/healthbar.json` |  |  |  | git checkout -- config/healthbar.json |
| `config/movement.json` |  |  |  | git checkout -- config/movement.json |
| `config/movement/movement-source.json` |  |  |  | git checkout -- config/movement/movement-source.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `config/weapons/rocket_launcher/config.json` |  |  |  | git checkout -- config/weapons/rocket_launcher/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-launch-modes.json` |  |  |  | git checkout -- devscripts/dev-launch-modes.json |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev-profiles/npc-navigation.json` |  |  |  | git checkout -- devscripts/dev-profiles/npc-navigation.json |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/regressions/networking/README.md` |  |  |  | git checkout -- docs/regressions/networking/README.md |
| `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md` |  |  |  | git checkout -- docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md |
| `docs/specs/dev-loop/dev-loop.md` |  |  |  | git checkout -- docs/specs/dev-loop/dev-loop.md |
| `docs/specs/gamemodes/funcounterstrike.md` |  |  |  | git checkout -- docs/specs/gamemodes/funcounterstrike.md |
| `docs/specs/open-gameplay-issues.md` |  |  |  | git checkout -- docs/specs/open-gameplay-issues.md |
| `docs/specs/performance/performance.md` |  |  |  | git checkout -- docs/specs/performance/performance.md |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/explosion-fx.h` |  |  |  | git checkout -- src/combat/explosion-fx.h |
| `src/combat/weapon-rocket-launcher.cpp` |  |  |  | git checkout -- src/combat/weapon-rocket-launcher.cpp |
| `src/effects/effect-part.cpp` |  |  |  | git checkout -- src/effects/effect-part.cpp |
| `src/effects/effect-part.h` |  |  |  | git checkout -- src/effects/effect-part.h |
| `src/effects/effect-part-render.cpp` |  |  |  | git checkout -- src/effects/effect-part-render.cpp |
| `src/gui/hud/player-nameplates.cpp` |  |  |  | git checkout -- src/gui/hud/player-nameplates.cpp |
| `src/gui/hud/player-nameplates.h` |  |  |  | git checkout -- src/gui/hud/player-nameplates.h |
| `src/network/multiplayer-context.h` |  |  |  | git checkout -- src/network/multiplayer-context.h |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `config/movement/movement-deliberate.json` |  |  |  | Remove-Item -LiteralPath 'config/movement/movement-deliberate.json' |
| `docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md' |
| `docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md' |
| `docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md' |
| `docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md' |
| `docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md' |
| `docs/changelog/2026-11-18/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-18/' |
| `docs/handoffs/collision-melee-fcs-trace-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/collision-melee-fcs-trace-handoff.md' |
| `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md' |
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

- Rocket impact owner/category: `src/combat/weapon-rocket-launcher.cpp`,
  `Weapons/Important`.
- `rocket.impact-mark.resolved` records source hit kind, target kind/ID,
  nearest world triangle/distance, final mark position, force scale, and actual
  decal insertion result. Actor colliders are excluded by querying only the
  world's static collision mesh; marks are suppressed if no triangle is within
  8 m or the decal system declines the insertion.
- Render owner/category: `src/effects/effect-part-render.cpp`,
  `Rendering/Important`.
- `explosion.shockwave.draw` records the first successful draw and first draw
  with alpha <= 0.5, including actual path, texture/model, scale, alpha, age,
  and position. At most two events per shockwave.

# Validation

- Focused skills: `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/logging-checker-v1.md` — source/spec and logging review followed;
  runtime and visual acceptance remain outstanding. Applied the `imagegen`
  skill to create the shockwave texture.
- Read-only source/config/diff review completed. `git diff --check` reported
  only two pre-existing trailing whitespace lines in
  `docs/specs/open-gameplay-issues.md`.
- No tests or build commands were run. The agent did not request or wait for a
  build. The shared dev-loop automatically began generation 338; latest
  observed `--status` reported `game-build status=building` and `run=NOT
  RUNNING`. It is unconfirmed whether that candidate captured the final source
  saves.
- No runtime journal from this change exists yet. Required events:
  `rocket.impact-mark.resolved`, `explosion.shockwave.spawn`, and
  `explosion.shockwave.draw` at `first_draw_submitted` and
  `alpha_fade_visible`.

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

- Visual review: pending; confirm the textured ring appears at its logged
  location/scale, fades visibly, scales with impact force, and actor-hit marks
  land on nearby world surfaces rather than bodies.
- Gameplay review: pending; exercise rocket hits on NPCs, players, terrain,
  and actor hits with no world surface beneath them.
- Multiplayer review:
- Still unverified: build completion/activation, runtime journal events,
  real-world placement under gameplay conditions, and human visual/gameplay
  acceptance.

# Agent handoff context

- Owner: Codex
- Focus: Rocket launcher FX, actor-safe impact marks, shockwave texture and alpha fade
- Files to check: `src/combat/weapon-rocket-launcher.cpp`,
  `src/combat/explosion-fx.cpp`, `src/effects/effect-part-render.cpp`,
  `assets/textureshq/rocket-shockwave-v1.png`, and the rocket launcher config.
- What another agent should not redo: actor surface resolution, diagnostic
  fields, or the shockwave texture/draw proof events.
- Next exact step: after the dev-loop publishes the final source revision,
  launch the target mode, hit an actor and a world surface, and inspect the
  client journal for the impact classification plus both shockwave draw phases;
  then review the visible ring and mark locations.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-18T05:35:43.892563Z", "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "ff22294bf5408cd93504d368659a6b6d03968e1e351c95c89173ee44531a4854", "dll": "build\\game\\generation-329\\game-g329.dll", "generation": 329, "revision": "f78f25b74be94b51a35e572aeac3c7f743894eb3725543ab6f59588e4b9e30ae", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-329\\game-g329.dll", "generation": 329, "published_utc": "2026-11-18T02:01:00.682Z", "revision": "f78f25b74be94b51a35e572aeac3c7f743894eb3725543ab6f59588e4b9e30ae", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 329
- Artifact: build\game\generation-329\game-g329.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_062057\server-01\events.jsonl
- Session ID: session-20261118-064520

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
 M config/camconfig.json
 M config/gamemodes/funcounterstrike.json
 M config/healthbar.json
 M config/movement.json
 M config/movement/movement-source.json
 M config/video-settings.json
 M config/weapons/rocket_launcher/config.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M devscripts/dev-loop.py
 M devscripts/dev-profiles/npc-navigation.json
 M docs/regressions/dev-loop/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/specs/gamemodes/funcounterstrike.md
 M docs/specs/open-gameplay-issues.md
 M docs/specs/performance/performance.md
 M src/combat/explosion-fx.cpp
 M src/combat/explosion-fx.h
 M src/combat/weapon-rocket-launcher.cpp
 M src/effects/effect-part.cpp
 M src/effects/effect-part.h
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
 M src/npc/npc.cpp
?? config/movement/movement-deliberate.json
?? docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md
?? docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md
?? docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md
?? docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md
?? docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md
?? docs/changelog/2026-11-18/
?? docs/handoffs/collision-melee-fcs-trace-handoff.md
?? docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md
?? docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  2 +-
 config/analytics.json                              |  4 +-
 config/audio/music-settings.json                   |  2 +-
 config/camconfig.json                              |  4 +-
 config/gamemodes/funcounterstrike.json             |  6 +-
 config/healthbar.json                              |  2 -
 config/movement.json                               |  1 +
 config/movement/movement-source.json               | 10 ++-
 config/video-settings.json                         |  2 +-
 config/weapons/rocket_launcher/config.json         |  2 +-
 devscripts/blender-ai                              |  0
 devscripts/dev-launch-modes.json                   |  1 +
 devscripts/dev-loop.py                             | 22 +++++++
 devscripts/dev-profiles/npc-navigation.json        |  2 +-
 docs/regressions/dev-loop/README.md                | 16 ++++-
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 +++++----
 docs/specs/dev-loop/dev-loop.md                    | 11 ++++
 docs/specs/gamemodes/funcounterstrike.md           |  3 +
 docs/specs/open-gameplay-issues.md                 | 14 ++++-
 docs/specs/performance/performance.md              | 33 ++++++++++
 src/combat/explosion-fx.cpp                        | 21 ++++---
 src/combat/explosion-fx.h                          |  6 +-
 src/combat/weapon-rocket-launcher.cpp              | 71 ++++++++++++++++++++--
 src/effects/effect-part.cpp                        |  4 +-
 src/effects/effect-part.h                          |  2 +-
 src/gui/hud/player-nameplates.cpp                  |  8 +++
 src/gui/hud/player-nameplates.h                    |  3 +-
 src/network/multiplayer-context.h                  |  4 ++
 src/network/multiplayer-projectiles.cpp            | 19 ++++++
 src/npc/npc.cpp                                    |  8 ++-
 31 files changed, 265 insertions(+), 50 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/camconfig.json
M	config/gamemodes/funcounterstrike.json
M	config/healthbar.json
M	config/movement.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	config/weapons/rocket_launcher/config.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	devscripts/dev-loop.py
M	devscripts/dev-profiles/npc-navigation.json
M	docs/regressions/dev-loop/README.md
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/gamemodes/funcounterstrike.md
M	docs/specs/open-gameplay-issues.md
M	docs/specs/performance/performance.md
M	src/combat/explosion-fx.cpp
M	src/combat/explosion-fx.h
M	src/combat/weapon-rocket-launcher.cpp
M	src/effects/effect-part.cpp
M	src/effects/effect-part.h
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-18T05:35:43.892563Z",
    "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "ff22294bf5408cd93504d368659a6b6d03968e1e351c95c89173ee44531a4854",
    "dll": "build\\game\\generation-329\\game-g329.dll",
    "generation": 329,
    "revision": "f78f25b74be94b51a35e572aeac3c7f743894eb3725543ab6f59588e4b9e30ae",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-329\\game-g329.dll",
    "generation": 329,
    "published_utc": "2026-11-18T02:01:00.682Z",
    "revision": "f78f25b74be94b51a35e572aeac3c7f743894eb3725543ab6f59588e4b9e30ae",
    "status": "ready"
  },
  "progress": {
    "compiled": 4,
    "elapsed_s": 54.844,
    "phase": "build_complete",
    "result": "success",
    "skipped": 83,
    "updated_utc": "2026-11-18T05:35:41.192Z"
  }
}
```
