// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest TDM and bodycam specification

# Task

- Task ID: session-7c88be861154
- Session ID: session-7c88be861154
- Owner: bloodtest-bodycam-spec
- Focus: Specification-only planning; no implementation, build request, or runtime test.
- Summary: Revised Bloodtest specification for respawning first-to-50 TDM, mode-selected slowdm movement, bodycam visuals driven by head_world, physical aim smoothing, blood-only player hit feedback, material-aware bullet impacts, fall damage, and disabled world destruction. Expanded reusable bodycam presentation and acceptance using the user's visual references. Added a provider-neutral screenshot comparison capability issue.
- Status: in-progress
- Date, time, timezone: 2026-11-23T17:11:15+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0abd6231aea08bed8580bff0c11369c01fc40bde
- Final commit: 0abd6231aea08bed8580bff0c11369c01fc40bde

# Pre-existing changes

- At the start of specification editing, the two target spec files were clean.
  The shared checkout already contained extensive unrelated source, config,
  issue, changelog, and regression changes from concurrent work; the generated
  `Captured git state` below lists them. They were preserved.
- This session's repository paths are the two specs, the existing Bloodtest
  issue record, the new screenshot-comparison capability issue, and this one
  changelog. The agent-coordination user contexts are recorded in shared brain
  state and are not workspace source changes.

# Requested behavior

Expand the existing Bloodtest mode into a slow, bodycam-focused TDM with active
respawns, RED/BLUE rosters, first team to 50 kills within three minutes,
three-second intermission, repeated results, slowdm movement, forced first
person/competitive avatars, head-world physical/hybrid aim, configurable
camera/arm smoothing, projectile-only revolver/shotgun/rifle, no explosive
impact FX, zero firearm knockback, configurable live weapon overrides, blood
and body-part feedback without HUD damage confirmation, bullet-hole/debris
world impacts, fall damage, and world geometry destruction disabled. Extend the
spec to make the supplied bodycam references actionable and track provider-neutral
screenshot/reference comparison separately.

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/bloodtest.md`,
  `docs/specs/gamemodes/bodycam.md`,
  `docs/specs/gamemodes/teamdeathmatch.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/camera/camera.md`, and
  `docs/specs/dev-loop/visual-proof-screenshots.md`.
- Exact requirements: the latest user clarification says respawns are enabled
  for Bloodtest TDM and the first team to 50 wins. The original request sets a
  three-minute active limit, three-second intermission, movement-slowdm,
  competitive avatars, physical aiming, JSON weapon overrides, no world
  destruction, and body damage/gore. The latest message prioritizes bodycam
  visuals, `head_world`, smooth positions/aim, bodycam-style optics, no player
  outlines/healthbars/damage numbers, fall damage, and visual comparison to the
  supplied images.
- Why the change follows the specification: the existing Bloodtest spec
  described FFA, no score/timer, a different movement profile, and enabled world
  destruction. User clarification now establishes a TDM contract. The revised
  spec names desired shared-owner behavior and acceptance without asserting
  implementation or runtime proof.
- Conflicts or decisions: result display duration is unspecified; the spec uses
  five seconds as an initial tunable target based on the TDM spec. The file
  `movement-slowdm.json` declares internal name `heavy`; the required runtime ID
  mapping is left explicit. FOV, aim smoothing, fall-damage curve, and weapon
  values need screenshot/runtime tuning. The requested 25-per-team roster and
  first-to-50 rules are consistent because respawns are enabled.

# Exact implementation changes

## File: `docs/specs/gamemodes/bloodtest.md`

- Lines/functions/headings: `Mode contract`, `Weapons`, `Bodycam and hit
  presentation`, `Authored body, collision, and role movement`, `Acceptance`,
  and `Related specifications`.
- Old content or behavior: described Bloodtest as FFA with no team/goal/timer,
  a deliberate movement preset, Swordsword in the loadout, and persistent world
  destruction.
- New content or behavior: specifies respawning RED/BLUE TDM to 50 kills or
  three-minute stalemate, JSON lifecycle and TDM-compatible map spawns,
  slowdm/forced bodycam/competitive avatars, projectile rifle and other allowed
  guns, deep live mode weapon overrides, zero firearm knockback, no explosive
  impact visuals, bodycam/head/arm smoothing, fall damage, and disabled world
  destruction.
- Reason: align the mode contract with the user's latest direction while
  preserving shared gamemode, movement, camera, weapon, actor damage, and
  networking owners.
- Why unrelated behavior is preserved: body-part health, shared projectile
  simulation, fixed 60 Hz behavior, NPC/player shared systems, and existing
  gore acceptance sections remain the common foundation. No runtime source or
  global config was edited.

## File: `docs/specs/gamemodes/bodycam.md`

- Lines/functions/headings: `Presentation goal`, `Shared behavior`, and
  `Acceptance`.
- Old content or behavior: broad body-worn camera intent with no concrete
  head-pose path, smoothing boundaries, optics target, wall-occlusion check, or
  image-based acceptance.
- New content or behavior: requires living `head_world` camera attachment,
  visible and aligned arms/weapon, independently tunable JSON response,
  presentation-only smoothing, remote pose interpolation, restrained edge
  chromatic aberration/lens shading without black bars, correct distinction
  between `proximityFade` and world occlusion, and repeated screenshot review
  against user references.
- Reason: make the desired bodycam result observable and tuneable.
- Why unrelated behavior is preserved: the shared camera and replay owners
  remain authoritative; human visual acceptance is still required.

## File: `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md`

- Lines/functions/headings: two appended `Agent suggestion` entries at the end.
- Old content or behavior: the earlier floor-fall and projectile/gore report
  had no current source trace for the new camera/TDM request.
- New content or behavior: appended static source/config observations, the
  separate provider-neutral comparison issue reference, limits on causal
  claims about floor fall-through, and a next proof expectation.
- Reason: keep useful findings with the existing Bloodtest issue and preserve
  the user report's append-only history.
- Why unrelated behavior is preserved: original quote, plan, and evidence were
  not rewritten.

## File: `docs/issues/capability/20261123-provider-neutral-screenshot-comparison-against-g-issue-41f95565b65e.md`

- Lines/functions/headings: new capability issue `issue-41f95565b65e`.
- Old content or behavior: no tracked request for comparing run-identified
  game captures against user reference images across vision providers.
- New content or behavior: complete capability issue for run-local screenshot
  capture, stable provider-neutral comparison input/output, privacy controls,
  structured uncertainty, and human-review acceptance.
- Reason: track the requested general AI tooling separately from Bloodtest's
  gameplay contract.
- Why unrelated behavior is preserved: no tooling implementation or provider
  integration was added.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/specs/gamemodes/bloodtest.md` | FFA/no timer, deliberate movement, world destruction enabled in desired contract | Respawning first-to-50 TDM with slowdm, bodycam, projectile weapons, gore feedback, fall-damage and no-destruction contract | Align with user's clarified mode direction | Restore from base `0abd6231aea08bed8580bff0c11369c01fc40bde` only if no later edits depend on it |
| `docs/specs/gamemodes/bodycam.md` | General bodycam goal without explicit head-world and visual comparison behavior | Head-attached live view, physical arms/weapon, smoothing/occlusion/optics requirements, screenshot acceptance | Make bodycam target testable | Restore from base `0abd6231aea08bed8580bff0c11369c01fc40bde` only if no later edits depend on it |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` | Existing append-only Bloodtest report/plan | Two appended source/config observations and follow-up evidence | Keep current findings with the existing issue | Issue history is append-only; supersede with a new correction/evidence entry rather than deleting history |
| `docs/issues/capability/20261123-provider-neutral-screenshot-comparison-against-g-issue-41f95565b65e.md` | No issue for reference-based screenshot comparison | New tracked capability request with source observations and acceptance criteria | Track tooling independently from game behavior | Leave the issue record intact; close or defer with a resolution if the user changes direction |
| `docs/changelog/2026-11-23/20261123_171116-bloodtest-tdm-and-bodycam-specification.md` | Did not exist | This final session record | Required per AI session | Preserve as audit history |

# Contract / ABI changes
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

- Owner: bloodtest-bodycam-spec
- Focus: Specification-only planning; no implementation, build request, or runtime test.
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 0abd6231aea08bed8580bff0c11369c01fc40bde / 0abd6231aea08bed8580bff0c11369c01fc40bde
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-23T13:26:39.822193Z", "latest_successful_build_id": "2026-11-23T13:26:39.822193Z:97b78d2ae01ed2dd665e39c9f5ac97ebf4acef801ee3597c4ecb0f7a584f7a8d", "latest_successful_source_generation": 27}, "game_current": {"build_mode": "debug", "code_hash": "74b0b8911904a6332c691de1885885ecb7904e46d01e5dbab35df0585a4e81f7", "dll": "build\\game\\generation-565\\game-g565.dll", "generation": 565, "revision": "36fc24817fc1c8834d9e5ab946d2f432ae5a0e78e4374be24ea671bcaff28b9b", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-565\\game-g565.dll", "generation": 565, "published_utc": "2026-11-23T16:56:02.573Z", "revision": "36fc24817fc1c8834d9e5ab946d2f432ae5a0e78e4374be24ea671bcaff28b9b", "status": "ready"}, "progress": {"compiled": 2, "elapsed_s": 33.618, "phase": "build_complete", "result": "success", "skipped": 85, "updated_utc": "2026-11-23T13:26:35.518Z"}}
- Generation: 565
- Artifact: build\game\generation-565\game-g565.dll
- Journal path: C:\mimita-v9\logs\2026-11-23\20261123_165703\client-01\events.jsonl
- Session ID: session-7c88be861154

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
M .gitignore
 M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/hot-reload/packet-ledger.json
 M config/movement/movement-source.json
 M config/video-settings.json
 M config/weapons/rocket_launcher/config.json
 m devscripts/blender-ai
 M devscripts/cpu-wpr-capture.ps1
 M devscripts/dev-loop.py
 M devscripts/dev_loop/build.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/dev_loop/runtimes.py
 M devscripts/game_build_worker.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-79e68f63fb01.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b7216286bdcc.md
 M docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b91438ac2972.md
 M docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-1bf97a27238f.md
 M docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-7d013b4a6c77.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-3dad54c7224d.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-25b14f436a3e.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-40dfaa2a803c.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-9a9807fa0128.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-b0ba8698acd7.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-bb273ac1386c.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md
 M docs/specs/gamemodes/bloodtest.md
 M docs/specs/gamemodes/bodycam.md
 M docs/specs/gamemodes/gamemodes.md
 M docs/specs/gamemodes/teamdeathmatch.md
 M src/devtools/terminal.cpp
 M src/engine/engine-tick.cpp
 M src/gui/ui-system-buttons.cpp
 M src/gui/ui-system.cpp
 M src/gui/ui-text-input.cpp
 M src/gui/ui-tooltip.cpp
 M src/map/map-geometry-module.cpp
 M src/mapeditor/map-editor-module.cpp
 M src/navigation/navigation-policy-module.cpp
 M src/network/network-policy-module.cpp
 M src/network/server-gamemode.cpp
 M src/network/server-gamemode.h
 M src/network/server-npcs.cpp
 M src/network/server-packet-chat.cpp
 M src/npc/npc-behavior-module.cpp
 M src/npc/npc-perception-module.cpp
 M src/npc/npc.cpp
 M src/npc/npc.h
 M src/perf/perf-frame.cpp
 M src/physics/movement/actor-contact-policy-module.cpp
 M src/physics/movement/physics-collision-dispatch.cpp
 M src/render/actor-rendering-module.cpp
 M src/render/world-rendering-module.cpp
 M src/terminal/debug-commands.cpp
 M src/world/chunk-collision-module.cpp
 M src/worldgen/infinite-worldgen-module.cpp
?? config/dev-superadmin.example.json
?? config/movement/movement-slowdm.json
?? devscripts/dev_loop/batch_targets.py
?? devscripts/quality/npc-match-report.py
?? devscripts/quality/test_game_build_cache_marker_fast_path.py
?? devscripts/quality/test_mode3_batch_net_delta.py
?? devscripts/quality/test_runtime_command_routing.py
?? docs/changelog/2026-11-23/20261123_0805-cpu-phase-evidence-source-mapping.md
?? docs/changelog/2026-11-23/20261123_0816-build-15file-latency-and-resource-coupling.md
?? docs/changelog/2026-11-23/20261123_0835-wpr-sampled-function-attribution.md
?? docs/changelog/2026-11-23/20261123_0846-wpr-workload-runtime-alignment.md
?? docs/changelog/2026-11-23/20261123_0847-issue-6413477d361d-independent-review.md
?? docs/changelog/2026-11-23/20261123_0902-mode3-pch-off-and-generation554.md
?? docs/changelog/2026-11-23/20261123_0910-cpu-wpr-evidence-crosslink.md
?? docs/changelog/2026-11-23/20261123_0920-structured-log-quota-pressure-evidence.md
?? docs/changelog/2026-11-23/20261123_092247-independent-review-of-game-dll-progress-json-publication-failure.md
?? docs/changelog/2026-11-23/20261123_094103-source-only-object-marker-fast-path.md
?? docs/changelog/2026-11-23/20261123_0947-marker-validation-microbenchmark.md
?? docs/changelog/2026-11-23/20261123_0954-offline-wpr-symbol-resolution.md
?? docs/changelog/2026-11-23/20261123_1000-live-resource-sample-and-profiler-threading.md
?? docs/changelog/2026-11-23/20261123_1012-exe-build-queue-wait-review.md
?? docs/changelog/2026-11-23/20261123_1035-coordination-unread-and-closure-audit.md
?? docs/changelog/2026-11-23/20261123_1035-resource-sample-pre-build-post-build-comparison.md
?? docs/changelog/2026-11-23/20261123_104153-independent-review-launch-client-reconciliation-failures.md
?? docs/changelog/2026-11-23/20261123_1042-mode3-cache-marker-live-activation-and-blink-compatibility.md
?? docs/changelog/2026-11-23/20261123_1105-resource-profile-thread-and-tick-overload.md
?? docs/changelog/2026-11-23/20261123_112000-issue-7d013b4a6c77-independent-review.md
?? docs/changelog/2026-11-23/20261123_1153-issue-25b14f436a3e-independent-review.md
?? docs/changelog/2026-11-23/20261123_1200-corrected-matched-cpu-evidence.md
?? docs/changelog/2026-11-23/20261123_1239-independent-review-issue-9a9807fa0128.md
?? docs/changelog/2026-11-23/20261123_130354-issue-b0ba8698acd7-independent-review.md
?? docs/changelog/2026-11-23/20261123_1325-mode3-pair-under-pressure-generation560-latency.md
?? docs/changelog/2026-11-23/20261123_1404-runtime-scoped-cpu-probe.md
?? docs/changelog/2026-11-23/20261123_1434-resource-capture-and-logging-ab.md
?? docs/changelog/2026-11-23/20261123_1435-mode3-reverted-target-filter.md
?? docs/changelog/2026-11-23/20261123_1545-issue-safe-close-reopen-controls.md
?? docs/changelog/2026-11-23/20261123_1556-issue-3dad54c7224d-bounded-review.md
?? docs/changelog/2026-11-23/20261123_1605-local-dev-superadmin-unlock.md
?? docs/changelog/2026-11-23/20261123_1620-cpu-auth-coordination.md
?? docs/changelog/2026-11-23/20261123_1630-issue-266c2a856cfc-bounded-wait-review.md
?? docs/changelog/2026-11-23/20261123_1640-issue-8a2e0ac7236b-build-wait-review.md
?? docs/changelog/2026-11-23/20261123_1645-generation564-build-fanout-and-activation-crash.md
?? docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md
?? docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md
?? docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
?? docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-65993acdc8b9.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-266c2a856cfc.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-3d02ab0de320.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-8a2e0ac7236b.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-991c632f1ca0.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-ec790f30f5ad.md
?? docs/issues/capability/20261123-provider-neutral-screenshot-comparison-against-g-issue-41f95565b65e.md
?? docs/issues/improvement/20261123-require-safe-issue-closure-and-reopening-for-hum-issue-11efafcb7122.md
?? docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md
?? docs/specs/weapons/projectile/ak47.md
?? src/perf/npc-cpu-probe.h
```

## Captured diff stat

```text
.gitignore                                         |    1 +
 config/accounts/default.json                       |    4 +-
 config/analytics.json                              |    2 +-
 config/audio/music-settings.json                   |    2 +-
 config/hot-reload/packet-ledger.json               |    6 +-
 config/movement/movement-source.json               |   11 +-
 config/video-settings.json                         |    2 +-
 config/weapons/rocket_launcher/config.json         |    4 +-
 devscripts/blender-ai                              |    0
 devscripts/cpu-wpr-capture.ps1                     |   16 +-
 devscripts/dev-loop.py                             |  196 +-
 devscripts/dev_loop/build.py                       |    3 +-
 devscripts/dev_loop/cli.py                         |    2 +-
 devscripts/dev_loop/parallel_test_runs.py          |    7 +-
 devscripts/dev_loop/runtimes.py                    |   25 +
 devscripts/game_build_worker.py                    |  197 +-
 docs/issues/README.md                              |   61 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |   34 +
 ...re-journal-masks-first-st-issue-79e68f63fb01.md |    6 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |   22 +
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   12 +
 ...s-compile-groundstate-has-issue-b7216286bdcc.md |   33 +
 ...s-compile-groundstate-has-issue-b91438ac2972.md |   35 +
 ...s-out-after-rebuilding-64-issue-1bf97a27238f.md |   42 +
 ...ion-fails-replacing-share-issue-61b5edc937d9.md |   42 +
 ...budget-exe-build-at-setup-issue-7d013b4a6c77.md |   38 +
 ...budget-exe-build-at-setup-issue-dff7e8c0f758.md |   50 +
 ...ild-queue-at-build-pendin-issue-3dad54c7224d.md |   38 +
 ...ild-queue-at-build-pendin-issue-e9f3b2727a9a.md |   44 +
 ...e-dll-build-at-game-build-issue-25b14f436a3e.md |   34 +
 ...e-dll-build-at-game-build-issue-40dfaa2a803c.md |   39 +
 ...e-dll-build-at-game-build-issue-6413477d361d.md |   34 +
 ...e-dll-build-at-game-build-issue-9a9807fa0128.md |   37 +
 ...e-dll-build-at-game-build-issue-b0ba8698acd7.md |   49 +
 ...e-dll-build-at-game-build-issue-bb273ac1386c.md |   39 +
 ...e-dll-build-at-game-build-issue-dd54a79216ad.md |   39 +
 ...move-avoidable-build-and--issue-e044a931c54b.md |   36 +
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   40 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |   14 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  108 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |   47 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  471 ++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 2443 ++++++++++++++------
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |    8 +
 .../fcs-objective-speed-defuse-REG-2026-11-22.md   |   93 +
 docs/specs/gamemodes/bloodtest.md                  |  173 +-
 docs/specs/gamemodes/bodycam.md                    |   89 +-
 docs/specs/gamemodes/gamemodes.md                  |   22 +
 docs/specs/gamemodes/teamdeathmatch.md             |   21 +
 src/devtools/terminal.cpp                          |   16 +-
 src/engine/engine-tick.cpp                         |   10 +-
 src/network/server-gamemode.cpp                    |  333 ++-
 src/network/server-gamemode.h                      |    3 +
 src/network/server-npcs.cpp                        |   13 +
 src/network/server-packet-chat.cpp                 |  125 +-
 src/npc/npc.cpp                                    |  170 +-
 src/npc/npc.h                                      |   18 +-
 src/perf/perf-frame.cpp                            |  270 +++
 .../movement/physics-collision-dispatch.cpp        |    3 +
 src/terminal/debug-commands.cpp                    |   74 +
 60 files changed, 4841 insertions(+), 965 deletions(-)
```

## Captured changed paths

```text
M	.gitignore
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/hot-reload/packet-ledger.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	config/weapons/rocket_launcher/config.json
M	devscripts/blender-ai
M	devscripts/cpu-wpr-capture.ps1
M	devscripts/dev-loop.py
M	devscripts/dev_loop/build.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/dev_loop/runtimes.py
M	devscripts/game_build_worker.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-79e68f63fb01.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b7216286bdcc.md
M	docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b91438ac2972.md
M	docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-1bf97a27238f.md
M	docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-7d013b4a6c77.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-3dad54c7224d.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-25b14f436a3e.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-40dfaa2a803c.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-9a9807fa0128.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-b0ba8698acd7.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-bb273ac1386c.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md
M	docs/specs/gamemodes/bloodtest.md
M	docs/specs/gamemodes/bodycam.md
M	docs/specs/gamemodes/gamemodes.md
M	docs/specs/gamemodes/teamdeathmatch.md
M	src/devtools/terminal.cpp
M	src/engine/engine-tick.cpp
M	src/network/server-gamemode.cpp
M	src/network/server-gamemode.h
M	src/network/server-npcs.cpp
M	src/network/server-packet-chat.cpp
M	src/npc/npc.cpp
M	src/npc/npc.h
M	src/perf/perf-frame.cpp
M	src/physics/movement/physics-collision-dispatch.cpp
M	src/terminal/debug-commands.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-23T13:26:39.822193Z",
    "latest_successful_build_id": "2026-11-23T13:26:39.822193Z:97b78d2ae01ed2dd665e39c9f5ac97ebf4acef801ee3597c4ecb0f7a584f7a8d",
    "latest_successful_source_generation": 27
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "74b0b8911904a6332c691de1885885ecb7904e46d01e5dbab35df0585a4e81f7",
    "dll": "build\\game\\generation-565\\game-g565.dll",
    "generation": 565,
    "revision": "36fc24817fc1c8834d9e5ab946d2f432ae5a0e78e4374be24ea671bcaff28b9b",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-565\\game-g565.dll",
    "generation": 565,
    "published_utc": "2026-11-23T16:56:02.573Z",
    "revision": "36fc24817fc1c8834d9e5ab946d2f432ae5a0e78e4374be24ea671bcaff28b9b",
    "status": "ready"
  },
  "progress": {
    "compiled": 2,
    "elapsed_s": 33.618,
    "phase": "build_complete",
    "result": "success",
    "skipped": 85,
    "updated_utc": "2026-11-23T13:26:35.518Z"
  }
}
```
