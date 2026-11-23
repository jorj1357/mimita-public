// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest Projectile Weapon Specs

# Task

- Task ID: session-8e2cdc43601b
- Session ID: session-8e2cdc43601b
- Owner: bloodtest-projectile-specs
- Focus: Specification-only Bloodtest weapon and bodycam planning
- Summary: Added individual physical-projectile firearm specs and shared ballistic networking contract; updated Bloodtest bodycam and tuning targets.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-23T17:29:08+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0abd6231aea08bed8580bff0c11369c01fc40bde
- Final commit: 0abd6231aea08bed8580bff0c11369c01fc40bde

# Pre-existing changes

The shared checkout contained numerous unrelated edits from concurrent tasks at
session start. The generated capture below preserves that state. This session
owns only the Bloodtest/bodycam/weapon specs listed below, this changelog, and
its issue suggestion; no unrelated path was changed intentionally.

# Requested behavior

Create separate specs for the named projectile firearms and define shared
ballistics/networking, never hitscan, with gravity-driven drop and individual
server-authoritative contacts. Add two-handed momentum melee and bat contracts
for later Bloodtest melee work. Set initial bodycam FOV to 70, force first
person, tune head/body/arms/weapon response in JSON, results duration to five
seconds, and record provisional fall-damage samples. Clarify whether the current
bodycam implementation is sufficient to begin a visual prototype.

# Specification alignment

- Current paths: `docs/specs/gamemodes/bloodtest.md`,
  `docs/specs/gamemodes/bodycam.md`, `docs/specs/weapons/weapons.md`,
  `docs/specs/weapons/projectile/*.md`, and
  `docs/specs/weapons/melee/{bat,momentum-two-handed}.md`.
- Existing direction: shared projectile and contact authority at fixed 60 Hz;
  shared camera and aim-body owners; mode behavior in JSON.
- Findings: current Bloodtest config is FFA with `camera_fov: 0`,
  `force_first_person: false`, 15-second intermission, 8-second results, and no
  teams. `head_world` is implemented but camera application currently requires
  ragdoll or aim-body active. PostFX chromatic aberration is currently zero.
- Decisions: initial FOV=70 and results=5 seconds. Fall damage points are
  explicitly provisional. Full weapon roster is specified, but the user has
  not chosen the subset for the first playable slice. Each weapon's final
  damage, cadence, ballistics, recoil, and reload tuning remain open.

# Exact implementation changes

- `docs/specs/weapons/projectile/projectile-firearms.md`: defines physical
  ballistic projectiles, drop, prediction/reconciliation, server damage
  authority, batching constraints, and concurrent-fire evidence.
- Individual firearm specs: `ak47.md`, `m4a1.md`, `famas.md`,
  `desert-eagle.md`, `judgement.md`, `glock-17.md`,
  `sawed-off-double-barrel.md`, `aa12.md`, and `uzi.md`.
- `docs/specs/weapons/melee/momentum-two-handed.md` and `bat.md`: define
  animation/physics blending and reserve editor/IK/model authoring as later
  capabilities.
- `docs/specs/gamemodes/bloodtest.md`: records no-hitscan firearm policy,
  weapon switch delays, selected starting ammo values, FOV70, five-second
  results, and the provisional fall-damage samples.
- `docs/specs/gamemodes/bodycam.md`: makes FOV70 the initial Bloodtest value.
- `docs/specs/weapons/weapons.md`: adds the new weapon document map.

# Changed files and reverse steps

Only restore these session-owned documentation changes to reverse this work;
do not use the generated full-checkout status as a list of files to revert.

| File | Change | Reverse |
|---|---|---|
| `docs/specs/gamemodes/bloodtest.md` | Updated mode firearm/tuning rules | Restore this session's documented section edits |
| `docs/specs/gamemodes/bodycam.md` | Set initial Bloodtest FOV to 70 | Restore this session's line |
| `docs/specs/weapons/weapons.md` | Added spec links | Remove this session's document-map entries |
| `docs/specs/weapons/projectile/ak47.md` | Replaced shorthand note with full contract | Restore prior note if needed |
| `docs/specs/weapons/projectile/{projectile-firearms,m4a1,famas,desert-eagle,judgement,glock-17,sawed-off-double-barrel,aa12,uzi}.md` | Added shared and individual projectile contracts | Remove these newly added docs |
| `docs/specs/weapons/melee/{bat,momentum-two-handed}.md` | Added later-scope melee contracts | Remove these newly added docs |
| `docs/changelog/2026-11-23/20261123_172909-bloodtest-projectile-weapon-specs.md` | Session record | Retain as repository history |

# Contract / ABI changes

None. No code, packet, ABI, or runtime configuration schema changed.

# State / schema changes

None. The requested mode/weapon overlays remain normative spec work; JSON
schemas/configs are not yet modified.

# Deleted paths

None.

# Diagnostics

No runtime diagnostics were added. Static source inspection confirmed the
current bodycam gaps above. Floor fall-through remains unknown and was not
investigated by this documentation task.

# Validation

- Skills read: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/spec-writing-v1.md`, `docs/skills/documentation-checker-v1.md`.
- Routed docs read: `docs/ROUTER.md`, Bloodtest/bodycam/weapon/networking
  specs, shared build/coordination/proof rules, and task-completion workflow.
- Markdown relative-link check over 19 changed/relevant documents: passed.
- `git diff --check -- docs/specs/gamemodes/bloodtest.md docs/specs/gamemodes/bodycam.md docs/specs/weapons`: passed; Git reported expected LF-to-CRLF conversion warnings.
- Build: none requested; documentation only.
- Runtime/visual proof: none. Automatically captured build identity/journal below
  belongs to shared checkout context and does not validate this work.

# Measured evidence

No new gameplay or performance measurements. The Uzi's 0.5-second magazine
emptying target and fall-damage examples require runtime tuning. No concurrent
fire load claim is made.

# Regression review

- Regression entry appended: no.
- Reason: no behavior changed and no confirmed regression was found.
- Related issue: `issue-2e7b0cd6f868`.

# Human acceptance

- Visual/bodycam review: pending screenshot and in-game tuning.
- Gameplay/multiplayer review: pending first implemented, pinned Bloodtest run.
- Still unverified: bodycam appearance, wall occlusion, weapon balance, projectile
  drop, contact networking, and load at intended roster.

# Agent handoff context

- Owner: bloodtest-projectile-specs.
- Focus: Bloodtest physical projectile weapons and bodycam specifications.
- Primary issue: `issue-2e7b0cd6f868`.
- Do not redo: nine per-weapon specs, shared projectile/network contract, and
  bodycam feasibility source inspection.
- Next exact step: after the user selects the first weapon subset, implement the
  bodycam vertical slice and core projectile firearm through shared owners and
  validate it with the exact dev-loop artifact, per-process journal, and
  screenshots. Record later weapon/animation/map-editor work as staged tasks.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 0abd6231aea08bed8580bff0c11369c01fc40bde / 0abd6231aea08bed8580bff0c11369c01fc40bde
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-23T13:26:39.822193Z", "latest_successful_build_id": "2026-11-23T13:26:39.822193Z:97b78d2ae01ed2dd665e39c9f5ac97ebf4acef801ee3597c4ecb0f7a584f7a8d", "latest_successful_source_generation": 27}, "game_current": {"build_mode": "debug", "code_hash": "33757790fa2b8a15385193c8126b0304863232d2fe3b1f4d69a0506ddd0638bc", "dll": "build\\game\\generation-566\\game-g566.dll", "generation": 566, "revision": "fedcff5f5f2cb54d005dac057de283ade96eb36924f47c90a1bf83a29c5e53ee", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-566\\game-g566.dll", "generation": 566, "published_utc": "2026-11-23T17:15:03.419Z", "revision": "fedcff5f5f2cb54d005dac057de283ade96eb36924f47c90a1bf83a29c5e53ee", "status": "ready"}, "progress": {"compiled": 2, "elapsed_s": 33.618, "phase": "build_complete", "result": "success", "skipped": 85, "updated_utc": "2026-11-23T13:26:35.518Z"}}
- Generation: 566
- Artifact: build\game\generation-566\game-g566.dll
- Journal path: C:\mimita-v9\logs\2026-11-23\20261123_171553\server-01\events.jsonl
- Session ID: session-8e2cdc43601b

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md`

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
 docs/issues/README.md                              |   63 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |   40 +
 ...re-journal-masks-first-st-issue-79e68f63fb01.md |    6 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |   22 +
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   18 +
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
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  493 ++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 2453 ++++++++++++++------
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |    8 +
 .../fcs-objective-speed-defuse-REG-2026-11-22.md   |   93 +
 docs/specs/gamemodes/bloodtest.md                  |  206 +-
 docs/specs/gamemodes/bodycam.md                    |   90 +-
 docs/specs/gamemodes/gamemodes.md                  |   22 +
 docs/specs/gamemodes/teamdeathmatch.md             |   21 +
 docs/specs/weapons/weapons.md                      |   12 +
 src/devtools/terminal.cpp                          |   16 +-
 src/engine/engine-tick.cpp                         |   10 +-
 src/network/server-gamemode.cpp                    |  371 ++-
 src/network/server-gamemode.h                      |    3 +
 src/network/server-npcs.cpp                        |   13 +
 src/network/server-packet-chat.cpp                 |  125 +-
 src/npc/npc.cpp                                    |  207 +-
 src/npc/npc.h                                      |   18 +-
 src/perf/perf-frame.cpp                            |  270 +++
 .../movement/physics-collision-dispatch.cpp        |    3 +
 src/terminal/debug-commands.cpp                    |   74 +
 61 files changed, 5013 insertions(+), 960 deletions(-)
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
    "code_hash": "33757790fa2b8a15385193c8126b0304863232d2fe3b1f4d69a0506ddd0638bc",
    "dll": "build\\game\\generation-566\\game-g566.dll",
    "generation": 566,
    "revision": "fedcff5f5f2cb54d005dac057de283ade96eb36924f47c90a1bf83a29c5e53ee",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-566\\game-g566.dll",
    "generation": 566,
    "published_utc": "2026-11-23T17:15:03.419Z",
    "revision": "fedcff5f5f2cb54d005dac057de283ade96eb36924f47c90a1bf83a29c5e53ee",
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
