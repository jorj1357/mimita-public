// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Vehicle Free Play launch crash investigation

# Task

- Task ID: session-20261126-155826
- Session ID: session-20261126-155826
- Owner: vehicle-freeplay-launch-regression
- Focus: issue-d8ce1d0aaf3f
- Summary: Recorded the repeated server profile-registry crash, candidate fix evidence, launch provenance issue, and GUI-input findings; a connected candidate run is still alive.
- Status: in progress; runtime repeat-launch proof incomplete
- Date, time, timezone: 2026-11-26T15:58:28+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0794fe4ee3e0448286768b696d38e07f52bc4385
- Final commit: 0794fe4ee3e0448286768b696d38e07f52bc4385

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
| `config/aimbody.json` |  |  |  | git checkout -- config/aimbody.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/impact_decals.json` |  |  |  | git checkout -- config/impact_decals.json |
| `config/lighting.json` |  |  |  | git checkout -- config/lighting.json |
| `config/maps/arena1.json` |  |  |  | git checkout -- config/maps/arena1.json |
| `config/postfx.json` |  |  |  | git checkout -- config/postfx.json |
| `config/vehicle-profiles/rl_soccar.json` |  |  |  | git checkout -- config/vehicle-profiles/rl_soccar.json |
| `config/vehicle-profiles/rl_soccar_freeplay.json` |  |  |  | git checkout -- config/vehicle-profiles/rl_soccar_freeplay.json |
| `config/vehicles/octane_rl.json` |  |  |  | git checkout -- config/vehicles/octane_rl.json |
| `config/weapon-tracers.json` |  |  |  | git checkout -- config/weapon-tracers.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `config/weapons/bloodtest_judgement/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_judgement/config.json |
| `config/weapons/masterlist/weapon-masterlist.json` |  |  |  | git checkout -- config/weapons/masterlist/weapon-masterlist.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-launch-modes.json` |  |  |  | git checkout -- devscripts/dev-launch-modes.json |
| `docs/changelog/2026-11-26/20261126_015301-rocket-league-landing-and-orientation-sit-on-wheels-no-ground-overlap-no-ground-tilt-correct-air-axes.md` |  |  |  | git checkout -- docs/changelog/2026-11-26/20261126_015301-rocket-league-landing-and-orientation-sit-on-wheels-no-ground-overlap-no-ground-tilt-correct-air-axes.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md` |  |  |  | git checkout -- docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md |
| `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-121f867b430a.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-121f867b430a.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-f995213a15b0.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-f995213a15b0.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-75e49f67b48c.md` |  |  |  | git checkout -- docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-75e49f67b48c.md |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-be98253c272f.md` |  |  |  | git checkout -- docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-be98253c272f.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md` |  |  |  | git checkout -- docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md |
| `docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md` |  |  |  | git checkout -- docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/regressions/dev-loop/server-crash-frozen-REG-2026-09-27.md` |  |  |  | git checkout -- docs/regressions/dev-loop/server-crash-frozen-REG-2026-09-27.md |
| `docs/specs/gamemodes/bloodtest.md` |  |  |  | git checkout -- docs/specs/gamemodes/bloodtest.md |
| `docs/specs/gamemodes/vehicle-arena.md` |  |  |  | git checkout -- docs/specs/gamemodes/vehicle-arena.md |
| `shaders/post.frag` |  |  |  | git checkout -- shaders/post.frag |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/weapon-config.cpp` |  |  |  | git checkout -- src/combat/weapon-config.cpp |
| `src/combat/weapon-config.h` |  |  |  | git checkout -- src/combat/weapon-config.h |
| `src/combat/weapon-viewmodel.cpp` |  |  |  | git checkout -- src/combat/weapon-viewmodel.cpp |
| `src/combat/weapon-viewmodel.h` |  |  |  | git checkout -- src/combat/weapon-viewmodel.h |
| `src/engine/engine-tick-camera.cpp` |  |  |  | git checkout -- src/engine/engine-tick-camera.cpp |
| `src/gamemode/gamemode-runtime-resolver.cpp` |  |  |  | git checkout -- src/gamemode/gamemode-runtime-resolver.cpp |
| `src/gamemode/gamemode.cpp` |  |  |  | git checkout -- src/gamemode/gamemode.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/vehicle-networking.cpp` |  |  |  | git checkout -- src/network/vehicle-networking.cpp |
| `src/physics/physical-entity.cpp` |  |  |  | git checkout -- src/physics/physical-entity.cpp |
| `src/physics/physical-entity.h` |  |  |  | git checkout -- src/physics/physical-entity.h |
| `src/ragdoll/ragdoll-mode.cpp` |  |  |  | git checkout -- src/ragdoll/ragdoll-mode.cpp |
| `src/ragdoll/ragdoll-mode.h` |  |  |  | git checkout -- src/ragdoll/ragdoll-mode.h |
| `src/render/post-fx.cpp` |  |  |  | git checkout -- src/render/post-fx.cpp |
| `src/render/post-fx.h` |  |  |  | git checkout -- src/render/post-fx.h |
| `src/vehicles/vehicle-commands.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-commands.cpp |
| `src/vehicles/vehicle-hud.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-hud.cpp |
| `src/vehicles/vehicle-profile.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-profile.cpp |
| `src/vehicles/vehicle-profile.h` |  |  |  | git checkout -- src/vehicles/vehicle-profile.h |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `src/vehicles/vehicle-system.h` |  |  |  | git checkout -- src/vehicles/vehicle-system.h |
| `src/vehicles/vehicle-types.h` |  |  |  | git checkout -- src/vehicles/vehicle-types.h |
| `devscripts/scenarios/rocket_dodge_360_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_dodge_360_v1.json' |
| `docs/changelog/2026-11-26/20261126-bloodtest-presentation-pass2.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-26/20261126-bloodtest-presentation-pass2.md' |
| `docs/changelog/2026-11-26/20261126-survive-rpg-voice-creator-vision.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-26/20261126-survive-rpg-voice-creator-vision.md' |
| `docs/changelog/2026-11-26/20261126_0624-independent-build-wait-review-issue-a114980aee90.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-26/20261126_0624-independent-build-wait-review-issue-a114980aee90.md' |
| `docs/changelog/2026-11-26/20261126_1504-independent-build-wait-review-issue-3385f308560f.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-26/20261126_1504-independent-build-wait-review-issue-3385f308560f.md' |
| `docs/changelog/2026-11-26/20261126_1555-independent-build-wait-review-issue-28feee119403.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-26/20261126_1555-independent-build-wait-review-issue-28feee119403.md' |
| `docs/handoffs/20261126-survive-rpg-voice-creator-vision.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261126-survive-rpg-voice-creator-vision.md' |
| `docs/issues/bug/20261126-settling-fuzz-car-ends-on-its-side-after-a-rando-issue-a4958b2df5e5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261126-settling-fuzz-car-ends-on-its-side-after-a-rando-issue-a4958b2df5e5.md' |
| `docs/issues/bug/20261126-vehicle-free-play-client-times-out-after-server--issue-d8ce1d0aaf3f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261126-vehicle-free-play-client-times-out-after-server--issue-d8ce1d0aaf3f.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-28feee119403.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-28feee119403.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-3385f308560f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-3385f308560f.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-70a739edc6ae.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-70a739edc6ae.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-8b46d995a7a0.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-8b46d995a7a0.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-a114980aee90.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-a114980aee90.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-ab0d4daf980a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-ab0d4daf980a.md' |
| `docs/issues/improvement/20261126-mark-every-mimita-launch-with-initiator-and-reas-issue-f5f8af2fe6bd.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261126-mark-every-mimita-launch-with-initiator-and-reas-issue-f5f8af2fe6bd.md' |
| `temp_sync.json` |  |  |  | Remove-Item -LiteralPath 'temp_sync.json' |

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

- Owner: vehicle-freeplay-launch-regression
- Focus: issue-d8ce1d0aaf3f
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 0794fe4ee3e0448286768b696d38e07f52bc4385 / 0794fe4ee3e0448286768b696d38e07f52bc4385
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-26T15:45:04.211415Z", "latest_successful_build_id": "2026-11-26T15:45:04.211415Z:eb693fe4cf2b0d599e58af4b9ffabf7d1857612d6ea10eb88edb05c3e5eb30cd", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "2c329a09cb026a503bce837e71755d361bb9e431d62d981fda566dbb9b8706ab", "dll": "build\\game\\generation-692\\game-g692.dll", "generation": 692, "revision": "f8b7a03ddb79e75f4de797806072412c4c84605b4eaafb47c4705ae0bf780b7e", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-692\\game-g692.dll", "generation": 692, "published_utc": "2026-11-26T15:50:00.869Z", "revision": "f8b7a03ddb79e75f4de797806072412c4c84605b4eaafb47c4705ae0bf780b7e", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.142, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-26T15:53:01.137Z"}}
- Generation: 692
- Artifact: build\game\generation-692\game-g692.dll
- Journal path: C:\mimita-v9\logs\2026-11-26\20261126_155302\server-01\events.jsonl
- Session ID: session-20261126-155826

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
 M config/aimbody.json
 M config/analytics.json
 M config/gamemodes/bloodtest.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/impact_decals.json
 M config/lighting.json
 M config/maps/arena1.json
 M config/postfx.json
 M config/vehicle-profiles/rl_soccar.json
 M config/vehicle-profiles/rl_soccar_freeplay.json
 M config/vehicles/octane_rl.json
 M config/weapon-tracers.json
 M config/weapons/bloodtest_ak47/config.json
 M config/weapons/bloodtest_judgement/config.json
 M config/weapons/masterlist/weapon-masterlist.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M docs/changelog/2026-11-26/20261126_015301-rocket-league-landing-and-orientation-sit-on-wheels-no-ground-overlap-no-ground-tilt-correct-air-axes.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-121f867b430a.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-f995213a15b0.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-75e49f67b48c.md
 M docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-be98253c272f.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md
 M docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/regressions/dev-loop/server-crash-frozen-REG-2026-09-27.md
 M docs/specs/gamemodes/bloodtest.md
 M docs/specs/gamemodes/vehicle-arena.md
 M shaders/post.frag
 M src/combat/explosion-fx.cpp
 M src/combat/weapon-config.cpp
 M src/combat/weapon-config.h
 M src/combat/weapon-viewmodel.cpp
 M src/combat/weapon-viewmodel.h
 M src/engine/engine-tick-camera.cpp
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/network/multiplayer-projectiles.cpp
 M src/network/vehicle-networking.cpp
 M src/physics/physical-entity.cpp
 M src/physics/physical-entity.h
 M src/ragdoll/ragdoll-mode.cpp
 M src/ragdoll/ragdoll-mode.h
 M src/render/post-fx.cpp
 M src/render/post-fx.h
 M src/vehicles/vehicle-commands.cpp
 M src/vehicles/vehicle-hud.cpp
 M src/vehicles/vehicle-profile.cpp
 M src/vehicles/vehicle-profile.h
 M src/vehicles/vehicle-system.cpp
 M src/vehicles/vehicle-system.h
 M src/vehicles/vehicle-types.h
?? devscripts/scenarios/rocket_dodge_360_v1.json
?? docs/changelog/2026-11-26/20261126-bloodtest-presentation-pass2.md
?? docs/changelog/2026-11-26/20261126-survive-rpg-voice-creator-vision.md
?? docs/changelog/2026-11-26/20261126_0624-independent-build-wait-review-issue-a114980aee90.md
?? docs/changelog/2026-11-26/20261126_1504-independent-build-wait-review-issue-3385f308560f.md
?? docs/changelog/2026-11-26/20261126_1555-independent-build-wait-review-issue-28feee119403.md
?? docs/handoffs/20261126-survive-rpg-voice-creator-vision.md
?? docs/issues/bug/20261126-settling-fuzz-car-ends-on-its-side-after-a-rando-issue-a4958b2df5e5.md
?? docs/issues/bug/20261126-vehicle-free-play-client-times-out-after-server--issue-d8ce1d0aaf3f.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-28feee119403.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-3385f308560f.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-70a739edc6ae.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-8b46d995a7a0.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-a114980aee90.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-ab0d4daf980a.md
?? docs/issues/improvement/20261126-mark-every-mimita-launch-with-initiator-and-reas-issue-f5f8af2fe6bd.md
?? temp_sync.json
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/aimbody.json                                |  16 +-
 config/analytics.json                              |   2 +-
 config/gamemodes/bloodtest.json                    |   3 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |   1 +
 config/impact_decals.json                          |  10 +-
 config/lighting.json                               |  20 +-
 config/maps/arena1.json                            |   4 +-
 config/postfx.json                                 |   1 +
 config/vehicle-profiles/rl_soccar.json             |   6 +-
 config/vehicle-profiles/rl_soccar_freeplay.json    |   6 +-
 config/vehicles/octane_rl.json                     |   8 +-
 config/weapon-tracers.json                         |  28 ++-
 config/weapons/bloodtest_ak47/config.json          |  48 ++--
 config/weapons/bloodtest_judgement/config.json     |  50 ++--
 config/weapons/masterlist/weapon-masterlist.json   |   8 +
 devscripts/blender-ai                              |   0
 devscripts/dev-launch-modes.json                   |  12 +
 ...ound-overlap-no-ground-tilt-correct-air-axes.md |  31 +++
 docs/issues/README.md                              |  43 ++--
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  40 ++++
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |  55 +++++
 ...es-desktop-content-instea-issue-130ee745052a.md |  12 +
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |  34 +++
 ...move-avoidable-build-and--issue-e044a931c54b.md |  16 ++
 ...e-dll-build-at-game-build-issue-121f867b430a.md |  45 ++++
 ...e-dll-build-at-game-build-issue-f995213a15b0.md |  42 ++++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  51 ++++
 ...e-dll-build-at-game-build-issue-75e49f67b48c.md |  42 ++++
 ...e-dll-build-at-game-build-issue-be98253c272f.md |  37 +++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |   8 +
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  52 +++++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  14 ++
 ...lity-needs-loud-diagnosab-issue-a5ca672b6628.md |  10 +
 ...eeds-a-deterministic-high-issue-235e26db08aa.md |   6 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   6 +
 .../dev-loop/server-crash-frozen-REG-2026-09-27.md |  36 +++
 docs/specs/gamemodes/bloodtest.md                  |  24 ++
 docs/specs/gamemodes/vehicle-arena.md              |  19 ++
 shaders/post.frag                                  |  18 +-
 src/combat/explosion-fx.cpp                        |  11 +-
 src/combat/weapon-config.cpp                       |  12 +
 src/combat/weapon-config.h                         |   9 +
 src/combat/weapon-viewmodel.cpp                    |  50 ++++
 src/combat/weapon-viewmodel.h                      |   2 +
 src/engine/engine-tick-camera.cpp                  |   7 +-
 src/gamemode/gamemode-runtime-resolver.cpp         |   3 +
 src/gamemode/gamemode.cpp                          |   6 +
 src/gamemode/gamemode.h                            |   3 +
 src/network/multiplayer-projectiles.cpp            |  69 +++---
 src/network/vehicle-networking.cpp                 |  11 +-
 src/physics/physical-entity.cpp                    |   6 +-
 src/physics/physical-entity.h                      |   3 +
 src/ragdoll/ragdoll-mode.cpp                       | 101 ++++++++
 src/ragdoll/ragdoll-mode.h                         |   8 +
 src/render/post-fx.cpp                             |  11 +
 src/render/post-fx.h                               |   4 +
 src/vehicles/vehicle-commands.cpp                  |  56 +++++
 src/vehicles/vehicle-hud.cpp                       |  18 +-
 src/vehicles/vehicle-profile.cpp                   |  28 ++-
 src/vehicles/vehicle-profile.h                     |  16 +-
 src/vehicles/vehicle-system.cpp                    | 259 +++++++++++++++++++--
 src/vehicles/vehicle-system.h                      |  22 +-
 src/vehicles/vehicle-types.h                       |   8 +
 64 files changed, 1404 insertions(+), 185 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/aimbody.json
M	config/analytics.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/impact_decals.json
M	config/lighting.json
M	config/maps/arena1.json
M	config/postfx.json
M	config/vehicle-profiles/rl_soccar.json
M	config/vehicle-profiles/rl_soccar_freeplay.json
M	config/vehicles/octane_rl.json
M	config/weapon-tracers.json
M	config/weapons/bloodtest_ak47/config.json
M	config/weapons/bloodtest_judgement/config.json
M	config/weapons/masterlist/weapon-masterlist.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	docs/changelog/2026-11-26/20261126_015301-rocket-league-landing-and-orientation-sit-on-wheels-no-ground-overlap-no-ground-tilt-correct-air-axes.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-121f867b430a.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-f995213a15b0.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-75e49f67b48c.md
M	docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-be98253c272f.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md
M	docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/regressions/dev-loop/server-crash-frozen-REG-2026-09-27.md
M	docs/specs/gamemodes/bloodtest.md
M	docs/specs/gamemodes/vehicle-arena.md
M	shaders/post.frag
M	src/combat/explosion-fx.cpp
M	src/combat/weapon-config.cpp
M	src/combat/weapon-config.h
M	src/combat/weapon-viewmodel.cpp
M	src/combat/weapon-viewmodel.h
M	src/engine/engine-tick-camera.cpp
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/network/multiplayer-projectiles.cpp
M	src/network/vehicle-networking.cpp
M	src/physics/physical-entity.cpp
M	src/physics/physical-entity.h
M	src/ragdoll/ragdoll-mode.cpp
M	src/ragdoll/ragdoll-mode.h
M	src/render/post-fx.cpp
M	src/render/post-fx.h
M	src/vehicles/vehicle-commands.cpp
M	src/vehicles/vehicle-hud.cpp
M	src/vehicles/vehicle-profile.cpp
M	src/vehicles/vehicle-profile.h
M	src/vehicles/vehicle-system.cpp
M	src/vehicles/vehicle-system.h
M	src/vehicles/vehicle-types.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-26T15:45:04.211415Z",
    "latest_successful_build_id": "2026-11-26T15:45:04.211415Z:eb693fe4cf2b0d599e58af4b9ffabf7d1857612d6ea10eb88edb05c3e5eb30cd",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "2c329a09cb026a503bce837e71755d361bb9e431d62d981fda566dbb9b8706ab",
    "dll": "build\\game\\generation-692\\game-g692.dll",
    "generation": 692,
    "revision": "f8b7a03ddb79e75f4de797806072412c4c84605b4eaafb47c4705ae0bf780b7e",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-692\\game-g692.dll",
    "generation": 692,
    "published_utc": "2026-11-26T15:50:00.869Z",
    "revision": "f8b7a03ddb79e75f4de797806072412c4c84605b4eaafb47c4705ae0bf780b7e",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 0.142,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-26T15:53:01.137Z"
  }
}
```
