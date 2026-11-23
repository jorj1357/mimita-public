// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Independent review: launch client_reconciliation failures

# Task

- Task ID: session-dd109bec837c
- Session ID: session-dd109bec837c
- Owner: issue-review-sweeper
- Focus: issue-651bb3982693
- Summary: Appended one evidence-grounded review to issue-651bb3982693 separating the original PowerShell/WMI scan timeout from the 2026-11-22 taskkill/recheck race, with contrary success reports and a next discriminating exit-wait check; added a measurable wake-prompt delivery proposal to issue-62ab3daa615d. No source, build, or close performed.
- Status: reviewed
- Date, time, timezone: 2026-11-23T10:41:52+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0abd6231aea08bed8580bff0c11369c01fc40bde
- Final commit: 0abd6231aea08bed8580bff0c11369c01fc40bde

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
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `src/gui/ui-system-buttons.cpp` |  |  |  | git checkout -- src/gui/ui-system-buttons.cpp |
| `src/gui/ui-system.cpp` |  |  |  | git checkout -- src/gui/ui-system.cpp |
| `src/gui/ui-text-input.cpp` |  |  |  | git checkout -- src/gui/ui-text-input.cpp |
| `src/gui/ui-tooltip.cpp` |  |  |  | git checkout -- src/gui/ui-tooltip.cpp |
| `src/map/map-geometry-module.cpp` |  |  |  | git checkout -- src/map/map-geometry-module.cpp |
| `src/mapeditor/map-editor-module.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor-module.cpp |
| `src/navigation/navigation-policy-module.cpp` |  |  |  | git checkout -- src/navigation/navigation-policy-module.cpp |
| `src/network/network-policy-module.cpp` |  |  |  | git checkout -- src/network/network-policy-module.cpp |
| `src/npc/npc-behavior-module.cpp` |  |  |  | git checkout -- src/npc/npc-behavior-module.cpp |
| `src/npc/npc-perception-module.cpp` |  |  |  | git checkout -- src/npc/npc-perception-module.cpp |
| `src/physics/movement/actor-contact-policy-module.cpp` |  |  |  | git checkout -- src/physics/movement/actor-contact-policy-module.cpp |
| `src/render/actor-rendering-module.cpp` |  |  |  | git checkout -- src/render/actor-rendering-module.cpp |
| `src/render/world-rendering-module.cpp` |  |  |  | git checkout -- src/render/world-rendering-module.cpp |
| `src/world/chunk-collision-module.cpp` |  |  |  | git checkout -- src/world/chunk-collision-module.cpp |
| `src/worldgen/infinite-worldgen-module.cpp` |  |  |  | git checkout -- src/worldgen/infinite-worldgen-module.cpp |
| `devscripts/quality/test_game_build_cache_marker_fast_path.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_game_build_cache_marker_fast_path.py' |
| `docs/changelog/2026-11-23/20261123_0805-cpu-phase-evidence-source-mapping.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0805-cpu-phase-evidence-source-mapping.md' |
| `docs/changelog/2026-11-23/20261123_0816-build-15file-latency-and-resource-coupling.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0816-build-15file-latency-and-resource-coupling.md' |
| `docs/changelog/2026-11-23/20261123_0835-wpr-sampled-function-attribution.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0835-wpr-sampled-function-attribution.md' |
| `docs/changelog/2026-11-23/20261123_0846-wpr-workload-runtime-alignment.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0846-wpr-workload-runtime-alignment.md' |
| `docs/changelog/2026-11-23/20261123_0847-issue-6413477d361d-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0847-issue-6413477d361d-independent-review.md' |
| `docs/changelog/2026-11-23/20261123_0902-mode3-pch-off-and-generation554.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0902-mode3-pch-off-and-generation554.md' |
| `docs/changelog/2026-11-23/20261123_0910-cpu-wpr-evidence-crosslink.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0910-cpu-wpr-evidence-crosslink.md' |
| `docs/changelog/2026-11-23/20261123_0920-structured-log-quota-pressure-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0920-structured-log-quota-pressure-evidence.md' |
| `docs/changelog/2026-11-23/20261123_092247-independent-review-of-game-dll-progress-json-publication-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_092247-independent-review-of-game-dll-progress-json-publication-failure.md' |
| `docs/changelog/2026-11-23/20261123_094103-source-only-object-marker-fast-path.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_094103-source-only-object-marker-fast-path.md' |
| `docs/changelog/2026-11-23/20261123_0947-marker-validation-microbenchmark.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0947-marker-validation-microbenchmark.md' |
| `docs/changelog/2026-11-23/20261123_0954-offline-wpr-symbol-resolution.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_0954-offline-wpr-symbol-resolution.md' |
| `docs/changelog/2026-11-23/20261123_1000-live-resource-sample-and-profiler-threading.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_1000-live-resource-sample-and-profiler-threading.md' |
| `docs/changelog/2026-11-23/20261123_1012-exe-build-queue-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_1012-exe-build-queue-wait-review.md' |
| `docs/changelog/2026-11-23/20261123_1035-resource-sample-pre-build-post-build-comparison.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_1035-resource-sample-pre-build-post-build-comparison.md' |
| `docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md' |
| `docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md' |
| `docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md' |

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

- Owner: issue-review-sweeper
- Focus: issue-651bb3982693
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 0abd6231aea08bed8580bff0c11369c01fc40bde / 0abd6231aea08bed8580bff0c11369c01fc40bde
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-22T21:32:48.195707Z", "latest_successful_build_id": "2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "b24977ff5b2140a18cf378c46c3c874a2d1e4a0595611e52463ed5c395f2bf28", "dll": "build\\game\\generation-555\\game-g555.dll", "generation": 555, "revision": "61205557d1586bcf2c59728be3c2ec65786bab3888a53aea0660dab4ec17d6b7", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-555\\game-g555.dll", "generation": 555, "published_utc": "2026-11-23T10:22:18.458Z", "revision": "61205557d1586bcf2c59728be3c2ec65786bab3888a53aea0660dab4ec17d6b7", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 10.222, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-23T10:10:24.589Z"}}
- Generation: 555
- Artifact: build\game\generation-555\game-g555.dll
- Journal path: C:\mimita-v9\logs\2026-11-23\20261123_101135\server-01\events.jsonl
- Session ID: session-dd109bec837c

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
M config/analytics.json
 m devscripts/blender-ai
 M devscripts/game_build_worker.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M src/gui/ui-system-buttons.cpp
 M src/gui/ui-system.cpp
 M src/gui/ui-text-input.cpp
 M src/gui/ui-tooltip.cpp
 M src/map/map-geometry-module.cpp
 M src/mapeditor/map-editor-module.cpp
 M src/navigation/navigation-policy-module.cpp
 M src/network/network-policy-module.cpp
 M src/npc/npc-behavior-module.cpp
 M src/npc/npc-perception-module.cpp
 M src/physics/movement/actor-contact-policy-module.cpp
 M src/render/actor-rendering-module.cpp
 M src/render/world-rendering-module.cpp
 M src/world/chunk-collision-module.cpp
 M src/worldgen/infinite-worldgen-module.cpp
?? devscripts/quality/test_game_build_cache_marker_fast_path.py
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
?? docs/changelog/2026-11-23/20261123_1035-resource-sample-pre-build-post-build-comparison.md
?? docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md
?? docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md
?? docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md
```

## Captured diff stat

```text
config/analytics.json                              |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/game_build_worker.py                    | 197 ++++++++++-
 docs/issues/README.md                              |  22 +-
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  12 +
 ...ion-fails-replacing-share-issue-61b5edc937d9.md |  42 +++
 ...budget-exe-build-at-setup-issue-dff7e8c0f758.md |  50 +++
 ...ild-queue-at-build-pendin-issue-e9f3b2727a9a.md |  44 +++
 ...e-dll-build-at-game-build-issue-6413477d361d.md |  34 ++
 ...e-dll-build-at-game-build-issue-dd54a79216ad.md |  39 +++
 ...move-avoidable-build-and--issue-e044a931c54b.md |  36 ++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  14 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  25 ++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  16 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 244 +++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 377 ++++++++++++++++++++-
 src/gui/ui-system-buttons.cpp                      |   1 +
 src/gui/ui-system.cpp                              |   1 +
 src/gui/ui-text-input.cpp                          |   1 +
 src/gui/ui-tooltip.cpp                             |   1 +
 src/map/map-geometry-module.cpp                    |   1 +
 src/mapeditor/map-editor-module.cpp                |   1 +
 src/navigation/navigation-policy-module.cpp        |   1 +
 src/network/network-policy-module.cpp              |   1 +
 src/npc/npc-behavior-module.cpp                    |   1 +
 src/npc/npc-perception-module.cpp                  |   1 +
 .../movement/actor-contact-policy-module.cpp       |   1 +
 src/render/actor-rendering-module.cpp              |   1 +
 src/render/world-rendering-module.cpp              |   1 +
 src/world/chunk-collision-module.cpp               |   1 +
 src/worldgen/infinite-worldgen-module.cpp          |   1 +
 31 files changed, 1150 insertions(+), 19 deletions(-)
```

## Captured changed paths

```text
M	config/analytics.json
M	devscripts/blender-ai
M	devscripts/game_build_worker.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	src/gui/ui-system-buttons.cpp
M	src/gui/ui-system.cpp
M	src/gui/ui-text-input.cpp
M	src/gui/ui-tooltip.cpp
M	src/map/map-geometry-module.cpp
M	src/mapeditor/map-editor-module.cpp
M	src/navigation/navigation-policy-module.cpp
M	src/network/network-policy-module.cpp
M	src/npc/npc-behavior-module.cpp
M	src/npc/npc-perception-module.cpp
M	src/physics/movement/actor-contact-policy-module.cpp
M	src/render/actor-rendering-module.cpp
M	src/render/world-rendering-module.cpp
M	src/world/chunk-collision-module.cpp
M	src/worldgen/infinite-worldgen-module.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-22T21:32:48.195707Z",
    "latest_successful_build_id": "2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "b24977ff5b2140a18cf378c46c3c874a2d1e4a0595611e52463ed5c395f2bf28",
    "dll": "build\\game\\generation-555\\game-g555.dll",
    "generation": 555,
    "revision": "61205557d1586bcf2c59728be3c2ec65786bab3888a53aea0660dab4ec17d6b7",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-555\\game-g555.dll",
    "generation": 555,
    "published_utc": "2026-11-23T10:22:18.458Z",
    "revision": "61205557d1586bcf2c59728be3c2ec65786bab3888a53aea0660dab4ec17d6b7",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 10.222,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-23T10:10:24.589Z"
  }
}
```
