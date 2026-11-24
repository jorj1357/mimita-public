// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest vision pass: world impact, damage, evidence, specs

# Task

- Task ID: session-20261124-193525
- Session ID: session-20261124-193525
- Owner: opencode-deepseek-v4-1
- Focus: Build evidence: game DLL generations 603, 604, and 607 published with no compile errors. Runtime evidence on isolated run 20261124_192914 (generation 605): impact.surface.spawn reports base_scale=2.0 and the shockwave model; explosion.shockwave.draw reports render_path=model_mesh at scale 1.5 with alpha 0.4167; actor.replica.trace reports remote NPC position/previous/step data (0.4-0.9 m per second, no large jumps). Not yet proven: a live projectile body-part damage hit (the automated physical aim missed the NPC), audible sound, visual acceptance, blood density, jitter on death/respawn, and 25v25 capacity. Human gameplay review remains.
- Summary: Replaced the 4 m explosion mark on Bloodtest bullets with a small bullet hole, made the shockwave GLB visible with a base scale, added 0.1 m projectile line trails, wired AK/Judgment sound files and brighter muzzle lights, added a two-point AK/Judgment damage curve and per-part projectile body classification, emitted local-shooter gore, added wall-biased blood splatter, reset corpse/respawn previous transforms, skipped dead network replicas, and added actor replica and combat hit evidence. Wrote the Bloodtest vision update, world-impact spec, reload-mode spec, and black-impact regression.
- Status: built; world impact, shockwave, trails, damage code, evidence, and specs landed; live body-part damage and visual/audio acceptance pending
- Date, time, timezone: 2026-11-24T19:35:31+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874
- Final commit: c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874

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
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/gamemodes/vehicle_arena.json` |  |  |  | git checkout -- config/gamemodes/vehicle_arena.json |
| `config/impact_decals.json` |  |  |  | git checkout -- config/impact_decals.json |
| `config/lighting.json` |  |  |  | git checkout -- config/lighting.json |
| `config/maps/arena1.json` |  |  |  | git checkout -- config/maps/arena1.json |
| `config/vehicles/octane_rl.json` |  |  |  | git checkout -- config/vehicles/octane_rl.json |
| `config/weapon-tracers.json` |  |  |  | git checkout -- config/weapon-tracers.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `config/weapons/bloodtest_judgement/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_judgement/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md |
| `docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md` |  |  |  | git checkout -- docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md |
| `docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md` |  |  |  | git checkout -- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md |
| `docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md` |  |  |  | git checkout -- docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md |
| `docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md` |  |  |  | git checkout -- docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `docs/specs/gamemodes/bloodtest.md` |  |  |  | git checkout -- docs/specs/gamemodes/bloodtest.md |
| `docs/specs/vehicles/vehicles.md` |  |  |  | git checkout -- docs/specs/vehicles/vehicles.md |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/weapon-execution.cpp` |  |  |  | git checkout -- src/combat/weapon-execution.cpp |
| `src/combat/weapon-json-config.cpp` |  |  |  | git checkout -- src/combat/weapon-json-config.cpp |
| `src/combat/weapon-types.h` |  |  |  | git checkout -- src/combat/weapon-types.h |
| `src/effects/effect-part-blood.cpp` |  |  |  | git checkout -- src/effects/effect-part-blood.cpp |
| `src/engine/engine-tick-render.cpp` |  |  |  | git checkout -- src/engine/engine-tick-render.cpp |
| `src/gamemode/map-config.cpp` |  |  |  | git checkout -- src/gamemode/map-config.cpp |
| `src/gamemode/map-config.h` |  |  |  | git checkout -- src/gamemode/map-config.h |
| `src/network/confirmed-damage-presentation.cpp` |  |  |  | git checkout -- src/network/confirmed-damage-presentation.cpp |
| `src/network/multiplayer-interpolation.cpp` |  |  |  | git checkout -- src/network/multiplayer-interpolation.cpp |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/network/vehicle-networking.cpp` |  |  |  | git checkout -- src/network/vehicle-networking.cpp |
| `src/ragdoll/ragdoll-mode.cpp` |  |  |  | git checkout -- src/ragdoll/ragdoll-mode.cpp |
| `src/vehicles/vehicle-config.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-config.cpp |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `src/vehicles/vehicle-types.h` |  |  |  | git checkout -- src/vehicles/vehicle-types.h |
| `assets/sound/weapon/ak47/` |  |  |  | Remove-Item -LiteralPath 'assets/sound/weapon/ak47/' |
| `assets/sound/weapon/judgement/` |  |  |  | Remove-Item -LiteralPath 'assets/sound/weapon/judgement/' |
| `devscripts/quality/test_game_build_failfast_priority.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_game_build_failfast_priority.py' |
| `docs/changelog/2026-11-24/20261124_184329-map-editor-lights-issue.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_184329-map-editor-lights-issue.md' |
| `docs/changelog/2026-11-24/20261124_1845-rl-reference-vehicle-physics.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1845-rl-reference-vehicle-physics.md' |
| `docs/changelog/2026-11-24/20261124_1848-issue-0cdac76fb006-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1848-issue-0cdac76fb006-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1851-wait-review-a6017d163afb.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1851-wait-review-a6017d163afb.md' |
| `docs/changelog/2026-11-24/20261124_1857-cpu-no-mimita-host-control.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1857-cpu-no-mimita-host-control.md' |
| `docs/changelog/2026-11-24/20261124_1916-issue-0539d967d696-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1916-issue-0539d967d696-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1920-cpu-loop-generation604-pressure-only.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1920-cpu-loop-generation604-pressure-only.md' |
| `docs/changelog/2026-11-24/20261124_1930-cpu-next-candidate-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1930-cpu-next-candidate-review.md' |
| `docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a0fa88737830.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a0fa88737830.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-f15a2daf6c16.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-f15a2daf6c16.md' |
| `docs/issues/feature/20261124-investigate-and-add-persistent-map-editor-lights-issue-1e01f275c858.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/feature/20261124-investigate-and-add-persistent-map-editor-lights-issue-1e01f275c858.md' |
| `docs/regressions/20261124-black-impact-ghost.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/20261124-black-impact-ghost.md' |
| `docs/specs/effects/world-impact-feedback.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/effects/world-impact-feedback.md' |
| `docs/specs/weapons/reload-modes.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/weapons/reload-modes.md' |

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

- Owner: opencode-deepseek-v4-1
- Focus: Build evidence: game DLL generations 603, 604, and 607 published with no compile errors. Runtime evidence on isolated run 20261124_192914 (generation 605): impact.surface.spawn reports base_scale=2.0 and the shockwave model; explosion.shockwave.draw reports render_path=model_mesh at scale 1.5 with alpha 0.4167; actor.replica.trace reports remote NPC position/previous/step data (0.4-0.9 m per second, no large jumps). Not yet proven: a live projectile body-part damage hit (the automated physical aim missed the NPC), audible sound, visual acceptance, blood density, jitter on death/respawn, and 25v25 capacity. Human gameplay review remains.
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874 / c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-24T19:27:02.742428Z", "latest_successful_build_id": "2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63", "latest_successful_source_generation": 40}, "game_current": {"build_mode": "debug", "code_hash": "6175ad523fc709fc0524f012a533a25df93da32dac808668380654095b51fa28", "dll": "build\\game\\generation-607\\game-g607.dll", "generation": 607, "revision": "8bf41f30ef15a3058e57f4553d8cd88ff53c07a022c194d9bec0f3c7e6124f45", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-607\\game-g607.dll", "generation": 607, "published_utc": "2026-11-24T19:34:34.734Z", "revision": "8bf41f30ef15a3058e57f4553d8cd88ff53c07a022c194d9bec0f3c7e6124f45", "status": "ready"}, "progress": {"compiled": 13, "elapsed_s": 47.806, "phase": "build_complete", "result": "success", "skipped": 74, "updated_utc": "2026-11-24T19:26:59.275Z"}}
- Generation: 607
- Artifact: build\game\generation-607\game-g607.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_192914\client-01\events.jsonl
- Session ID: session-20261124-193525

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
 M config/gamemodes/bloodtest.json
 M config/gamemodes/vehicle_arena.json
 M config/impact_decals.json
 M config/lighting.json
 M config/maps/arena1.json
 M config/vehicles/octane_rl.json
 M config/weapon-tracers.json
 M config/weapons/bloodtest_ak47/config.json
 M config/weapons/bloodtest_judgement/config.json
 m devscripts/blender-ai
 M devscripts/game_build_worker.py
 M docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md
 M docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md
 M docs/issues/README.md
 M docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
 M docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/specs/gamemodes/bloodtest.md
 M docs/specs/vehicles/vehicles.md
 M src/combat/explosion-fx.cpp
 M src/combat/weapon-execution.cpp
 M src/combat/weapon-json-config.cpp
 M src/combat/weapon-types.h
 M src/effects/effect-part-blood.cpp
 M src/engine/engine-tick-render.cpp
 M src/gamemode/map-config.cpp
 M src/gamemode/map-config.h
 M src/network/confirmed-damage-presentation.cpp
 M src/network/multiplayer-interpolation.cpp
 M src/network/server-projectiles.cpp
 M src/network/vehicle-networking.cpp
 M src/ragdoll/ragdoll-mode.cpp
 M src/vehicles/vehicle-config.cpp
 M src/vehicles/vehicle-system.cpp
 M src/vehicles/vehicle-types.h
?? assets/sound/weapon/ak47/
?? assets/sound/weapon/judgement/
?? devscripts/quality/test_game_build_failfast_priority.py
?? docs/changelog/2026-11-24/20261124_184329-map-editor-lights-issue.md
?? docs/changelog/2026-11-24/20261124_1845-rl-reference-vehicle-physics.md
?? docs/changelog/2026-11-24/20261124_1848-issue-0cdac76fb006-independent-review.md
?? docs/changelog/2026-11-24/20261124_1851-wait-review-a6017d163afb.md
?? docs/changelog/2026-11-24/20261124_1857-cpu-no-mimita-host-control.md
?? docs/changelog/2026-11-24/20261124_1916-issue-0539d967d696-independent-review.md
?? docs/changelog/2026-11-24/20261124_1920-cpu-loop-generation604-pressure-only.md
?? docs/changelog/2026-11-24/20261124_1930-cpu-next-candidate-review.md
?? docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a0fa88737830.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-f15a2daf6c16.md
?? docs/issues/feature/20261124-investigate-and-add-persistent-map-editor-lights-issue-1e01f275c858.md
?? docs/regressions/20261124-black-impact-ghost.md
?? docs/specs/effects/world-impact-feedback.md
?? docs/specs/weapons/reload-modes.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 config/gamemodes/bloodtest.json                    |  11 +-
 config/gamemodes/vehicle_arena.json                |   2 +-
 config/impact_decals.json                          |  11 +-
 config/lighting.json                               |  13 +-
 config/maps/arena1.json                            |  33 +++-
 config/vehicles/octane_rl.json                     |   6 +-
 config/weapon-tracers.json                         |   5 +-
 config/weapons/bloodtest_ak47/config.json          |  21 +-
 config/weapons/bloodtest_judgement/config.json     |  21 +-
 devscripts/blender-ai                              |   0
 devscripts/game_build_worker.py                    |  73 +++++++
 .../20261124_1740-header-fanout-reduction-plan.md  | 148 ++++++++++++++
 ...124_1746-sampler-setup-cost-and-failed-build.md |  18 ++
 docs/issues/README.md                              |  25 +--
 ...m-match-has-no-team-no-ho-issue-5d82fc58a875.md |   6 +
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   8 +
 ...e-dll-build-at-game-build-issue-a236165fa6fc.md |   6 +
 ...e-dll-build-at-game-build-issue-a6017d163afb.md |  46 +++++
 ...e-dll-build-at-game-build-issue-d82b02478dff.md |  28 +++
 ...physics-and-car-based-mod-issue-e5a086b0a441.md |  16 ++
 ...ient-release-gate-for-all-issue-c8290acca248.md |  51 +++++
 ...vehicle-end-to-end-proof--issue-0539d967d696.md |  41 ++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  15 ++
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  54 ++++++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 200 +++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 205 ++++++++++++++++++++
 docs/specs/dev-loop/build-time-observability.md    |  13 ++
 docs/specs/gamemodes/bloodtest.md                  |  47 ++++-
 docs/specs/vehicles/vehicles.md                    | 124 ++++++++++--
 src/combat/explosion-fx.cpp                        |  24 ++-
 src/combat/weapon-execution.cpp                    |  18 ++
 src/combat/weapon-json-config.cpp                  |   1 +
 src/combat/weapon-types.h                          |   1 +
 src/effects/effect-part-blood.cpp                  |  25 +--
 src/engine/engine-tick-render.cpp                  |  46 +++++
 src/gamemode/map-config.cpp                        |   4 +
 src/gamemode/map-config.h                          |   4 +
 src/network/confirmed-damage-presentation.cpp      |   8 +-
 src/network/multiplayer-interpolation.cpp          |  11 ++
 src/network/server-projectiles.cpp                 | 122 ++++++++----
 src/network/vehicle-networking.cpp                 |   6 +-
 src/ragdoll/ragdoll-mode.cpp                       |  14 +-
 src/vehicles/vehicle-config.cpp                    |  13 ++
 src/vehicles/vehicle-system.cpp                    | 215 ++++++++++++---------
 src/vehicles/vehicle-types.h                       |  13 ++
 47 files changed, 1562 insertions(+), 214 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/vehicle_arena.json
M	config/impact_decals.json
M	config/lighting.json
M	config/maps/arena1.json
M	config/vehicles/octane_rl.json
M	config/weapon-tracers.json
M	config/weapons/bloodtest_ak47/config.json
M	config/weapons/bloodtest_judgement/config.json
M	devscripts/blender-ai
M	devscripts/game_build_worker.py
M	docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md
M	docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md
M	docs/issues/README.md
M	docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
M	docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/specs/gamemodes/bloodtest.md
M	docs/specs/vehicles/vehicles.md
M	src/combat/explosion-fx.cpp
M	src/combat/weapon-execution.cpp
M	src/combat/weapon-json-config.cpp
M	src/combat/weapon-types.h
M	src/effects/effect-part-blood.cpp
M	src/engine/engine-tick-render.cpp
M	src/gamemode/map-config.cpp
M	src/gamemode/map-config.h
M	src/network/confirmed-damage-presentation.cpp
M	src/network/multiplayer-interpolation.cpp
M	src/network/server-projectiles.cpp
M	src/network/vehicle-networking.cpp
M	src/ragdoll/ragdoll-mode.cpp
M	src/vehicles/vehicle-config.cpp
M	src/vehicles/vehicle-system.cpp
M	src/vehicles/vehicle-types.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-24T19:27:02.742428Z",
    "latest_successful_build_id": "2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63",
    "latest_successful_source_generation": 40
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "6175ad523fc709fc0524f012a533a25df93da32dac808668380654095b51fa28",
    "dll": "build\\game\\generation-607\\game-g607.dll",
    "generation": 607,
    "revision": "8bf41f30ef15a3058e57f4553d8cd88ff53c07a022c194d9bec0f3c7e6124f45",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-607\\game-g607.dll",
    "generation": 607,
    "published_utc": "2026-11-24T19:34:34.734Z",
    "revision": "8bf41f30ef15a3058e57f4553d8cd88ff53c07a022c194d9bec0f3c7e6124f45",
    "status": "ready"
  },
  "progress": {
    "compiled": 13,
    "elapsed_s": 47.806,
    "phase": "build_complete",
    "result": "success",
    "skipped": 74,
    "updated_utc": "2026-11-24T19:26:59.275Z"
  }
}
```
