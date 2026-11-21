// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest behavior intake and source trace

# Task

- Task ID: session-20261121-230552
- Session ID: session-20261121-230552
- Owner: bloodtest-ground-gore-movement
- Focus: Bloodtest collision, projectile impact effects, movement preset, and slope behavior
- Summary: Recorded the Bloodtest report, compared active projectile/movement config to its spec, and identified missing live reproduction evidence.
- Status: NEEDS_SPEC_DECISION
- Date, time, timezone: 2026-11-21T23:05:54+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d89884f64374810efa624eac8ee6cc5af7e19dae
- Final commit: d89884f64374810efa624eac8ee6cc5af7e19dae

# Pre-existing changes

- Exact status output:
- Files not created or modified by this session:

# Requested behavior

# Specification alignment

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md |
| `docs/issues/docs/20261120-spec-review-build-queue-runtime-registry-subagen-issue-394f2607b148.md` |  |  |  | git checkout -- docs/issues/docs/20261120-spec-review-build-queue-runtime-registry-subagen-issue-394f2607b148.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md` |  |  |  | git checkout -- docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md |
| `docs/specs/procedural-infinite-world/procedural-infinite-world.md` |  |  |  | git checkout -- docs/specs/procedural-infinite-world/procedural-infinite-world.md |
| `src/effects/zombie-tower-effects.cpp` |  |  |  | git checkout -- src/effects/zombie-tower-effects.cpp |
| `src/gamemode/map-config.cpp` |  |  |  | git checkout -- src/gamemode/map-config.cpp |
| `src/mapeditor/map-editor-commands.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor-commands.cpp |
| `src/mapeditor/map-editor.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor.cpp |
| `src/mapeditor/map-editor.h` |  |  |  | git checkout -- src/mapeditor/map-editor.h |
| `docs/changelog/2026-11-21/20261121_215833-infinite-xxity-scale-feature-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_215833-infinite-xxity-scale-feature-intake.md' |
| `docs/changelog/2026-11-21/20261121_220424-issue-136d4553ff92-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_220424-issue-136d4553ff92-independent-review.md' |
| `docs/changelog/2026-11-21/20261121_220437-one-source-compile-link-trial.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_220437-one-source-compile-link-trial.md' |
| `docs/changelog/2026-11-21/20261121_222546-infinite-xxity-generation-zone-map-editor-controls.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_222546-infinite-xxity-generation-zone-map-editor-controls.md' |
| `docs/changelog/2026-11-21/20261121_222900-live-build-timer-and-resource-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_222900-live-build-timer-and-resource-audit.md' |
| `docs/changelog/2026-11-21/20261121_223000-issue-6907072bb155-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_223000-issue-6907072bb155-independent-review.md' |
| `docs/changelog/2026-11-21/20261121_225553-issue-23148d00bf5b-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_225553-issue-23148d00bf5b-independent-review.md' |
| `docs/changelog/2026-11-21/20261121_225638-horde-launch-and-join-trace.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_225638-horde-launch-and-join-trace.md' |
| `docs/changelog/2026-11-21/20261121_230200-build-latency-gap-plan-and-agent-closure-rule.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_230200-build-latency-gap-plan-and-agent-closure-rule.md' |
| `docs/changelog/2026-11-21/20261121_230327-reconnect-horde-consistency-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_230327-reconnect-horde-consistency-investigation.md' |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-1b08300935ae.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-1b08300935ae.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-44d82d8626ad.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-44d82d8626ad.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-585ca55b5bb8.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-585ca55b5bb8.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-a1eebc340521.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-a1eebc340521.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-cc0a03f471ad.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-cc0a03f471ad.md' |
| `docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-4651ec01f8a5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-4651ec01f8a5.md' |
| `docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-d69d22e34259.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-d69d22e34259.md' |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md' |
| `docs/reports/20261121_220437-one-source-compile-link-trial.md` |  |  |  | Remove-Item -LiteralPath 'docs/reports/20261121_220437-one-source-compile-link-trial.md' |
| `docs/reports/20261121_222900-live-build-timer-and-resource-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/reports/20261121_222900-live-build-timer-and-resource-audit.md' |

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

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

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

- Owner: bloodtest-ground-gore-movement
- Focus: Bloodtest collision, projectile impact effects, movement preset, and slope behavior
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d89884f64374810efa624eac8ee6cc5af7e19dae / d89884f64374810efa624eac8ee6cc5af7e19dae
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-21T02:26:50.306117Z", "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "63386cc96d870e80700eb8a6e2e125ccdca3df12290f40cc0f17ff4646e3f293", "dll": "build\\game\\generation-436\\game-g436.dll", "generation": 436, "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-436\\game-g436.dll", "generation": 436, "published_utc": "2026-11-21T22:36:56.026Z", "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45", "status": "ready"}, "progress": {"compiled": 17, "elapsed_s": 2727.517, "phase": "build_complete", "result": "success", "skipped": 70, "updated_utc": "2026-11-21T02:26:10.895Z"}}
- Generation: 436
- Artifact: build\game\generation-436\game-g436.dll
- Journal path: C:\mimita-v9\logs\2026-11-21\20261121_224058\server-01\events.jsonl
- Session ID: session-20261121-230552

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
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev_loop/gui.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md
 M docs/issues/docs/20261120-spec-review-build-queue-runtime-registry-subagen-issue-394f2607b148.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md
 M docs/specs/procedural-infinite-world/procedural-infinite-world.md
 M src/effects/zombie-tower-effects.cpp
 M src/gamemode/map-config.cpp
 M src/mapeditor/map-editor-commands.cpp
 M src/mapeditor/map-editor.cpp
 M src/mapeditor/map-editor.h
?? docs/changelog/2026-11-21/20261121_215833-infinite-xxity-scale-feature-intake.md
?? docs/changelog/2026-11-21/20261121_220424-issue-136d4553ff92-independent-review.md
?? docs/changelog/2026-11-21/20261121_220437-one-source-compile-link-trial.md
?? docs/changelog/2026-11-21/20261121_222546-infinite-xxity-generation-zone-map-editor-controls.md
?? docs/changelog/2026-11-21/20261121_222900-live-build-timer-and-resource-audit.md
?? docs/changelog/2026-11-21/20261121_223000-issue-6907072bb155-independent-review.md
?? docs/changelog/2026-11-21/20261121_225553-issue-23148d00bf5b-independent-review.md
?? docs/changelog/2026-11-21/20261121_225638-horde-launch-and-join-trace.md
?? docs/changelog/2026-11-21/20261121_230200-build-latency-gap-plan-and-agent-closure-rule.md
?? docs/changelog/2026-11-21/20261121_230327-reconnect-horde-consistency-investigation.md
?? docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-1b08300935ae.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-44d82d8626ad.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-585ca55b5bb8.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-a1eebc340521.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-cc0a03f471ad.md
?? docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-4651ec01f8a5.md
?? docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-d69d22e34259.md
?? docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
?? docs/reports/20261121_220437-one-source-compile-link-trial.md
?? docs/reports/20261121_222900-live-build-timer-and-resource-audit.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  10 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/gui.py                         |  84 ++++++-
 docs/issues/README.md                              |  30 ++-
 ...rs-fail-to-consistently-e-issue-b95574b63f29.md |  32 +++
 ...e-dll-build-at-game-build-issue-2235461bb31b.md |  31 +++
 ...e-dll-build-at-game-build-issue-c3ecfab72cba.md |  30 +++
 ...e-dll-build-at-game-build-issue-c632de44f8d6.md |  36 +++
 ...-runtime-registry-subagen-issue-394f2607b148.md |   6 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  18 ++
 ...nd-observe-context-bundle-issue-6907072bb155.md |  53 ++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  68 +++++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 128 ++++++++++
 ...r-migration-without-live--issue-23148d00bf5b.md |  41 +++
 .../procedural-infinite-world.md                   |   9 +
 src/effects/zombie-tower-effects.cpp               |   1 +
 src/gamemode/map-config.cpp                        |   7 +-
 src/mapeditor/map-editor-commands.cpp              |  59 +++--
 src/mapeditor/map-editor.cpp                       | 280 ++++++++++++++++++++-
 src/mapeditor/map-editor.h                         |   8 +
 23 files changed, 882 insertions(+), 55 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev_loop/gui.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md
M	docs/issues/docs/20261120-spec-review-build-queue-runtime-registry-subagen-issue-394f2607b148.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md
M	docs/specs/procedural-infinite-world/procedural-infinite-world.md
M	src/effects/zombie-tower-effects.cpp
M	src/gamemode/map-config.cpp
M	src/mapeditor/map-editor-commands.cpp
M	src/mapeditor/map-editor.cpp
M	src/mapeditor/map-editor.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-21T02:26:50.306117Z",
    "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "63386cc96d870e80700eb8a6e2e125ccdca3df12290f40cc0f17ff4646e3f293",
    "dll": "build\\game\\generation-436\\game-g436.dll",
    "generation": 436,
    "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-436\\game-g436.dll",
    "generation": 436,
    "published_utc": "2026-11-21T22:36:56.026Z",
    "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45",
    "status": "ready"
  },
  "progress": {
    "compiled": 17,
    "elapsed_s": 2727.517,
    "phase": "build_complete",
    "result": "success",
    "skipped": 70,
    "updated_utc": "2026-11-21T02:26:10.895Z"
  }
}
```


# Session findings

- Requested behavior: keep Bloodtest actors on the floor; use physical projectiles with force-scaled skin/meat/bone transitions and strong destruction blood effects; retain slow, traversable slope movement.
- Spec: docs/specs/gamemodes/bloodtest.md describes these goals and states that runtime and visual acceptance are still open.
- Observed config: both Bloodtest guns already use physical projectiles and set player/world explosion flags. Their splash radius is zero and visual projectile radii are 0.004 m (revolver) and 0.003 m (shotgun). Client terminal-event handling routes non-projectile-rifle events through spawnExplosionFx. This does not establish whether the effect renders visibly in Bloodtest.
- Observed movement mismatch: config/movement/movement-bloodtest.json configures 12.0 max/ground speed and 16.0 jump velocity. The current spec describes a deliberate 1 m/s baseline. The user's new slow, Bloodtest-specific clone and slope request needs spec alignment before tuning shared movement behavior.
- Runtime evidence: none gathered for Bloodtest. At inspection, the shared dev-loop ran Payload mode 33 under run 20261121_224058; its EXE identity was stale. It was left running. No build, source edit, Bloodtest launch, or gameplay proof was performed.
- Missing repro: exact map/arena, player or NPC, spawn/location, action/input, timing, and whether fall-through is repeatable. For weapon feedback, missing target type, impact distance, and whether the visible failure is absent explosion, blood, layer transition, or part destruction.
- Decision: no code change is safe from the report alone because falling through the floor is a behavior bug whose first missing collision stage is unknown, and enabling projectile/explosion flags would duplicate settings already present. The issue is recorded as issue-2e7b0cd6f868; source findings and discriminating next checks are appended there.
- Validation: source/config inspection only; no build or live test. Runtime and human visual acceptance remain unproven.
- Focused documents: docs/specs/gamemodes/bloodtest.md, docs/specs/ragdoll-retrograd/ragdoll-retrograd.md, docs/specs/movement/movement.md, docs/architecture/collision/collision.md, docs/specs/creators/monster-authoring.md, docs/skills/spec-behavior-review-v1.md, docs/specs/dev-loop/agent-proof-and-verification.md, docs/workflows/issue-intake-and-triage.md, docs/workflows/runtime-scenario-validation.md, docs/operations/task-completion/task-completion.md.
- Pre-existing working tree edits were preserved; no unrelated files were modified by hand.

