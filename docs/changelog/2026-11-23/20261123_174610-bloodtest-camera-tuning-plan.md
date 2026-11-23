// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest Camera Tuning Plan

# Task

- Task ID: session-e9c7bf7dcd7a
- Session ID: session-e9c7bf7dcd7a
- Owner: bloodtest-camera-tuning-plan
- Focus: Bodycam visual tuning and override plan
- Summary: Specified Bloodtest mode-owned camera, head and aim smoothing overrides with live JSON reload, tuning workflow and staged proof plan.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-23T17:46:06+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0abd6231aea08bed8580bff0c11369c01fc40bde
- Final commit: 0abd6231aea08bed8580bff0c11369c01fc40bde

# Pre-existing changes

The shared checkout contains many unrelated concurrent edits captured in the
automatic evidence below. This session intentionally modified only the
Bloodtest bodycam specification and this changelog; the existing Bloodtest
issue received an appended plan suggestion through agent coordination.

# Requested behavior

Give the user a practical way to start Bloodtest, edit mode-owned camera/head/
arm/weapon smoothing and bodycam visual values, and see them live. Recommend
where overrides live and define safe layering, reload behavior, tuning order,
rollback, and proof.

# Specification alignment

- Existing camera, aim-body, and PostFX config files are shared defaults with
  their own owners and reload paths.
- `config/gamemodes/masterlist/gamemode-masterlist.json` defines mode JSON
  schema; `GamemodeRegistry` parses and polls mode files.
- Existing `GamemodeRuntimeResolver::applyClient/clearClient` is the shared
  owner for mode-specific client overlays.
- The engine currently applies `head_world` orientation only when ragdoll or
  aim-body is active; ordinary living Bloodtest play needs an implementation
  path.
- The plan keeps the mode file as the user edit surface and leaves global
  defaults untouched.

# Exact implementation changes

- Updated `docs/specs/gamemodes/bodycam.md` with a Bloodtest live-tuning
  contract, proposed JSON location/shape, per-setting response controls,
  inheritance and validation, active-mode reload behavior, rollback, tuning
  stages, and evidence expectations.
- Appended a phased implementation/tuning plan and source evidence to
  `issue-2e7b0cd6f868`.

# Changed files and reverse steps

| Path | Change | Reverse |
|---|---|---|
| `docs/specs/gamemodes/bodycam.md` | Added mode-owned JSON tuning and live workflow contract | Revert this session's added section |
| `docs/changelog/2026-11-23/20261123_174610-bloodtest-camera-tuning-plan.md` | Session record | Keep as history |
| Coordination issue `issue-2e7b0cd6f868` | Appended plan suggestion | Preserve append-only history; append a correction if needed |

# Contract / ABI changes

No code or ABI changed. The spec proposes `presentation.bodycam`; it is not
implemented or schema-validated yet.

# State / schema changes

No runtime schema changed. Next implementation must add the nested block to
the gamemode masterlist and canonical Bloodtest JSON, then teach the shared
loader/runtime resolver to consume it.

# Deleted paths

None.

# Diagnostics

No runtime diagnostic changed. Planned resolved/reloaded/rejected revision
records are specified for the future implementation.

# Validation

- Read `docs/specs/gamemodes/bodycam.md`,
  `docs/specs/camera/camera.md`, `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/hotreload/hotreload.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `src/gamemode/gamemode-runtime-resolver.cpp`, and camera/gamemode source.
- Local Markdown links in the modified bodycam spec: passed.
- `git diff --check -- docs/specs/gamemodes/bodycam.md`: passed, with expected
  LF-to-CRLF warning.
- No tests/build/runtime run; this is a plan/spec change only.

# Measured evidence

No new runtime measurements. Source inspection confirms mode JSON polling,
mode overlay resolver, global camera/aim/PostFX reload owners, and the current
ragdoll/aim-body gate for `head_world`.

# Regression review

No confirmed regression and no regression entry added.

# Human acceptance

The plan is ready for review. The bodycam look, live behavior, smoothing feel,
wall visibility, and multiplayer interpolation have not been visually or
runtime-accepted.

# Agent handoff context

- Issue: `issue-2e7b0cd6f868`.
- Next exact work: implement the masterlist and mode JSON schema, resolve its
  overlay through `GamemodeRuntimeResolver`, enable living head-world camera
  use through existing owners, and add revision diagnostics. Then launch a
  pinned Bloodtest run and tune against screenshots.
- Do not create a second camera config owner or edit shared defaults for
  Bloodtest-only changes.

# How to reverse

Revert only the added bodycam specification section and retain the append-only
issue discussion and changelog. Preserve all concurrent checkout edits.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 0abd6231aea08bed8580bff0c11369c01fc40bde / 0abd6231aea08bed8580bff0c11369c01fc40bde
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-23T13:26:39.822193Z", "latest_successful_build_id": "2026-11-23T13:26:39.822193Z:97b78d2ae01ed2dd665e39c9f5ac97ebf4acef801ee3597c4ecb0f7a584f7a8d", "latest_successful_source_generation": 27}, "game_current": {"build_mode": "debug", "code_hash": "3ce3673f656373f1c5b1c8b5206cb40434143bdd09336c91021f828ef95e092b", "dll": "build\\game\\generation-567\\game-g567.dll", "generation": 567, "revision": "aead82d9c0ec61f0f3708717f17195a91d406711ca72a316337df0059555f865", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-567\\game-g567.dll", "generation": 567, "published_utc": "2026-11-23T17:29:35.900Z", "revision": "aead82d9c0ec61f0f3708717f17195a91d406711ca72a316337df0059555f865", "status": "ready"}, "progress": {"compiled": 2, "elapsed_s": 33.618, "phase": "build_complete", "result": "success", "skipped": 85, "updated_utc": "2026-11-23T13:26:35.518Z"}}
- Generation: 567
- Artifact: build\game\generation-567\game-g567.dll
- Journal path: C:\mimita-v9\logs\2026-11-23\20261123_173613\client-02\events.jsonl
- Session ID: session-e9c7bf7dcd7a

# Related feature record

- Feature path: `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md`

## Captured git state
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
 M config/behavior-profiles.json
 M config/hot-reload/packet-ledger.json
 M config/movement/movement-source.json
 M config/npc-difficulty.json
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
 M docs/specs/weapons/weapons.md
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
?? docs/changelog/2026-11-23/20261123-1721-postbuild-mitita-resource-window.md
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
?? docs/changelog/2026-11-23/20261123_171116-bloodtest-tdm-and-bodycam-specification.md
?? docs/changelog/2026-11-23/20261123_1723-mode3-late-join-review-and-build-resource-coupling.md
?? docs/changelog/2026-11-23/20261123_1724-generation566-incremental-build-evidence.md
?? docs/changelog/2026-11-23/20261123_172909-bloodtest-projectile-weapon-specs.md
?? docs/changelog/2026-11-23/20261123_1735-issue-3d02ab0de320-build-wait-review.md
?? docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md
?? docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md
?? docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
?? docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-65993acdc8b9.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-266c2a856cfc.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-3d02ab0de320.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-8a2e0ac7236b.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-8fdc520a07c5.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-991c632f1ca0.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-a17177a48737.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-ec790f30f5ad.md
?? docs/issues/capability/20261123-provider-neutral-screenshot-comparison-against-g-issue-41f95565b65e.md
?? docs/issues/improvement/20261123-require-safe-issue-closure-and-reopening-for-hum-issue-11efafcb7122.md
?? docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md
?? docs/specs/weapons/melee/bat.md
?? docs/specs/weapons/melee/momentum-two-handed.md
?? docs/specs/weapons/projectile/aa12.md
?? docs/specs/weapons/projectile/ak47.md
?? docs/specs/weapons/projectile/desert-eagle.md
?? docs/specs/weapons/projectile/famas.md
?? docs/specs/weapons/projectile/glock-17.md
?? docs/specs/weapons/projectile/judgement.md
?? docs/specs/weapons/projectile/m4a1.md
?? docs/specs/weapons/projectile/projectile-firearms.md
?? docs/specs/weapons/projectile/sawed-off-double-barrel.md
?? docs/specs/weapons/projectile/uzi.md
?? src/perf/npc-cpu-probe.h
```

## Captured diff stat

```text
.gitignore                                         |    1 +
 config/accounts/default.json                       |    4 +-
 config/analytics.json                              |    2 +-
 config/audio/music-settings.json                   |    2 +-
 config/behavior-profiles.json                      |   37 +-
 config/hot-reload/packet-ledger.json               |    6 +-
 config/movement/movement-source.json               |   11 +-
 config/npc-difficulty.json                         |    2 +-
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
 docs/issues/README.md                              |   63 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |   48 +
 ...re-journal-masks-first-st-issue-79e68f63fb01.md |    6 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |   22 +
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   24 +
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
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   48 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |   14 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  125 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |   47 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  531 +++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 2451 ++++++++++++++------
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   16 +
 .../fcs-objective-speed-defuse-REG-2026-11-22.md   |  132 ++
 docs/specs/gamemodes/bloodtest.md                  |  206 +-
 docs/specs/gamemodes/bodycam.md                    |  195 +-
 docs/specs/gamemodes/gamemodes.md                  |   22 +
 docs/specs/gamemodes/teamdeathmatch.md             |   21 +
 docs/specs/weapons/weapons.md                      |   12 +
 src/devtools/terminal.cpp                          |   16 +-
 src/engine/engine-tick.cpp                         |   10 +-
 src/network/server-gamemode.cpp                    |  379 ++-
 src/network/server-gamemode.h                      |    3 +
 src/network/server-npcs.cpp                        |   14 +
 src/network/server-packet-chat.cpp                 |  125 +-
 src/npc/npc.cpp                                    |  245 +-
 src/npc/npc.h                                      |   21 +-
 src/perf/perf-frame.cpp                            |  270 +++
 .../movement/physics-collision-dispatch.cpp        |    3 +
 src/terminal/debug-commands.cpp                    |   74 +
 63 files changed, 5314 insertions(+), 975 deletions(-)
```

## Captured changed paths

```text
M	.gitignore
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/behavior-profiles.json
M	config/hot-reload/packet-ledger.json
M	config/movement/movement-source.json
M	config/npc-difficulty.json
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
M	docs/specs/weapons/weapons.md
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
    "code_hash": "3ce3673f656373f1c5b1c8b5206cb40434143bdd09336c91021f828ef95e092b",
    "dll": "build\\game\\generation-567\\game-g567.dll",
    "generation": 567,
    "revision": "aead82d9c0ec61f0f3708717f17195a91d406711ca72a316337df0059555f865",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-567\\game-g567.dll",
    "generation": 567,
    "published_utc": "2026-11-23T17:29:35.900Z",
    "revision": "aead82d9c0ec61f0f3708717f17195a91d406711ca72a316337df0059555f865",
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
