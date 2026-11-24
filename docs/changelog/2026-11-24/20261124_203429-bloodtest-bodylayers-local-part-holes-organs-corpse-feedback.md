// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest bodylayers, local part holes, organs, corpse feedback

# Task

- Task ID: session-20261124-203356
- Session ID: session-20261124-203356
- Owner: opencode-deepseek-v4-1
- Focus: Build evidence: game DLL generations 609, 616, and 617 published. Runtime evidence on isolated run 20261124_202802 (generation 617): bloodtest.body.layer.ready reports parts=6, meat_layers=6, bone_layers=6 for both the local player and NPC-1000. Not yet proven: a live body-part hole from a projectile (the automated physical aim missed the NPC), organ visibility, corpse shooting, and visual acceptance. The link failure was caused by stale network objects/packet ledger, not the bodylayer code; regenerating packet-ledger and recompiling fixed it.
- Summary: Loaded the authored meat and bone bodylayer GLBs per part with pivot alignment and fallback, added per-part local damage holes with shader discards, added colored organ primitives for high-force torso/head hits, added client-only corpse bullet feedback, and added body-layer/hole evidence. Fixed the build cache link failure by regenerating the packet ledger and recompiling the stale network translation units.
- Status: built; bodylayer loading and readiness proven, hole/organ/corpse code landed, visual and hit proof pending
- Date, time, timezone: 2026-11-24T20:34:20+00:00
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
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/impact_decals.json` |  |  |  | git checkout -- config/impact_decals.json |
| `config/lighting.json` |  |  |  | git checkout -- config/lighting.json |
| `config/maps/arena1.json` |  |  |  | git checkout -- config/maps/arena1.json |
| `config/vehicles/octane_rl.json` |  |  |  | git checkout -- config/vehicles/octane_rl.json |
| `config/weapon-tracers.json` |  |  |  | git checkout -- config/weapon-tracers.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `config/weapons/bloodtest_judgement/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_judgement/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md |
| `docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` |  |  |  | git checkout -- docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md` |  |  |  | git checkout -- docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md |
| `docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md` |  |  |  | git checkout -- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md |
| `docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md` |  |  |  | git checkout -- docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md |
| `docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md` |  |  |  | git checkout -- docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `docs/specs/dev-loop/dev-loop.md` |  |  |  | git checkout -- docs/specs/dev-loop/dev-loop.md |
| `docs/specs/gamemodes/bloodtest.md` |  |  |  | git checkout -- docs/specs/gamemodes/bloodtest.md |
| `docs/specs/vehicles/vehicles.md` |  |  |  | git checkout -- docs/specs/vehicles/vehicles.md |
| `shaders/basic.frag` |  |  |  | git checkout -- shaders/basic.frag |
| `shaders/basic.vert` |  |  |  | git checkout -- shaders/basic.vert |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/weapon-execution.cpp` |  |  |  | git checkout -- src/combat/weapon-execution.cpp |
| `src/combat/weapon-json-config.cpp` |  |  |  | git checkout -- src/combat/weapon-json-config.cpp |
| `src/combat/weapon-types.h` |  |  |  | git checkout -- src/combat/weapon-types.h |
| `src/effects/effect-part-blood.cpp` |  |  |  | git checkout -- src/effects/effect-part-blood.cpp |
| `src/engine/engine-tick-render.cpp` |  |  |  | git checkout -- src/engine/engine-tick-render.cpp |
| `src/entities/player-loader.cpp` |  |  |  | git checkout -- src/entities/player-loader.cpp |
| `src/entities/player-render.cpp` |  |  |  | git checkout -- src/entities/player-render.cpp |
| `src/entities/player.cpp` |  |  |  | git checkout -- src/entities/player.cpp |
| `src/entities/player.h` |  |  |  | git checkout -- src/entities/player.h |
| `src/gamemode/map-config.cpp` |  |  |  | git checkout -- src/gamemode/map-config.cpp |
| `src/gamemode/map-config.h` |  |  |  | git checkout -- src/gamemode/map-config.h |
| `src/network/confirmed-damage-presentation.cpp` |  |  |  | git checkout -- src/network/confirmed-damage-presentation.cpp |
| `src/network/multiplayer-interpolation.cpp` |  |  |  | git checkout -- src/network/multiplayer-interpolation.cpp |
| `src/network/multiplayer-packets.cpp` |  |  |  | git checkout -- src/network/multiplayer-packets.cpp |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/multiplayer-shots.cpp` |  |  |  | git checkout -- src/network/multiplayer-shots.cpp |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/network/vehicle-networking.cpp` |  |  |  | git checkout -- src/network/vehicle-networking.cpp |
| `src/physics/physical-entity.cpp` |  |  |  | git checkout -- src/physics/physical-entity.cpp |
| `src/ragdoll/ragdoll-mode.cpp` |  |  |  | git checkout -- src/ragdoll/ragdoll-mode.cpp |
| `src/ragdoll/ragdoll-mode.h` |  |  |  | git checkout -- src/ragdoll/ragdoll-mode.h |
| `src/vehicles/vehicle-config.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-config.cpp |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `src/vehicles/vehicle-types.h` |  |  |  | git checkout -- src/vehicles/vehicle-types.h |
| `assets/sound/weapon/ak47/` |  |  |  | Remove-Item -LiteralPath 'assets/sound/weapon/ak47/' |
| `assets/sound/weapon/judgement/` |  |  |  | Remove-Item -LiteralPath 'assets/sound/weapon/judgement/' |
| `devscripts/quality/test_game_build_failfast_priority.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_game_build_failfast_priority.py' |
| `docs/changelog/2026-11-24/20261124-fail-fast-game-dll-build-timeout.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124-fail-fast-game-dll-build-timeout.md' |
| `docs/changelog/2026-11-24/20261124_1444-wait-review-issue-f15a2daf6c16.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1444-wait-review-issue-f15a2daf6c16.md' |
| `docs/changelog/2026-11-24/20261124_184329-map-editor-lights-issue.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_184329-map-editor-lights-issue.md' |
| `docs/changelog/2026-11-24/20261124_1845-rl-reference-vehicle-physics.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1845-rl-reference-vehicle-physics.md' |
| `docs/changelog/2026-11-24/20261124_1848-issue-0cdac76fb006-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1848-issue-0cdac76fb006-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1851-wait-review-a6017d163afb.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1851-wait-review-a6017d163afb.md' |
| `docs/changelog/2026-11-24/20261124_1857-cpu-no-mimita-host-control.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1857-cpu-no-mimita-host-control.md' |
| `docs/changelog/2026-11-24/20261124_1916-issue-0539d967d696-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1916-issue-0539d967d696-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1920-cpu-loop-generation604-pressure-only.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1920-cpu-loop-generation604-pressure-only.md' |
| `docs/changelog/2026-11-24/20261124_1930-cpu-next-candidate-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1930-cpu-next-candidate-review.md' |
| `docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md' |
| `docs/changelog/2026-11-24/20261124_193533-bloodtest-vision-pass-world-impact-damage-evidence-specs.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_193533-bloodtest-vision-pass-world-impact-damage-evidence-specs.md' |
| `docs/changelog/2026-11-24/20261124_1948-issue-5d82fc58a875-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1948-issue-5d82fc58a875-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_2000-cpu-gen609-pressure-and-replay-stall.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2000-cpu-gen609-pressure-and-replay-stall.md' |
| `docs/changelog/2026-11-24/20261124_200800-issue-536424f63590-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_200800-issue-536424f63590-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_2025-cpu-iteration-gate-and-history-lead.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2025-cpu-iteration-gate-and-history-lead.md' |
| `docs/issues/build-friction/20261124-game-dll-link-fails-on-undefined-mimitanet-mp-sy-issue-2e6440940e32.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-game-dll-link-fails-on-undefined-mimitanet-mp-sy-issue-2e6440940e32.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-536424f63590.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-536424f63590.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-74f939fb99fc.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-74f939fb99fc.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-7ef5043ce95c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-7ef5043ce95c.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a0fa88737830.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a0fa88737830.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a4eea8fe2246.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a4eea8fe2246.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-b18b7d8cf193.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-b18b7d8cf193.md' |
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
- Focus: Build evidence: game DLL generations 609, 616, and 617 published. Runtime evidence on isolated run 20261124_202802 (generation 617): bloodtest.body.layer.ready reports parts=6, meat_layers=6, bone_layers=6 for both the local player and NPC-1000. Not yet proven: a live body-part hole from a projectile (the automated physical aim missed the NPC), organ visibility, corpse shooting, and visual acceptance. The link failure was caused by stale network objects/packet ledger, not the bodylayer code; regenerating packet-ledger and recompiling fixed it.
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874 / c6eb8e5e8b7c58ab9b02a3764e36693ef4df7874
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-24T19:27:02.742428Z", "latest_successful_build_id": "2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63", "latest_successful_source_generation": 40}, "game_current": {"build_mode": "debug", "code_hash": "d4b2aec095a0dc4360e066c22a2193619f99e25d8cae3dc27cce9ea3fa2e10d0", "dll": "build\\game\\generation-617\\game-g617.dll", "generation": 617, "revision": "c7d650e35721fcd630da0c641c5de322d5f87a16d294d62c80808ff75d61a935", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-617\\game-g617.dll", "generation": 617, "published_utc": "2026-11-24T20:26:28.347Z", "revision": "c7d650e35721fcd630da0c641c5de322d5f87a16d294d62c80808ff75d61a935", "status": "ready"}, "progress": {"compiled": 13, "elapsed_s": 47.806, "phase": "build_complete", "result": "success", "skipped": 74, "updated_utc": "2026-11-24T19:26:59.275Z"}}
- Generation: 617
- Artifact: build\game\generation-617\game-g617.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_202802\server-01\events.jsonl
- Session ID: session-20261124-203356

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
 M config/hot-reload/packet-ledger.json
 M config/impact_decals.json
 M config/lighting.json
 M config/maps/arena1.json
 M config/vehicles/octane_rl.json
 M config/weapon-tracers.json
 M config/weapons/bloodtest_ak47/config.json
 M config/weapons/bloodtest_judgement/config.json
 m devscripts/blender-ai
 M devscripts/dev_loop/game_build.py
 M devscripts/game_build_worker.py
 M docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md
 M docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
 M docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/specs/gamemodes/bloodtest.md
 M docs/specs/vehicles/vehicles.md
 M shaders/basic.frag
 M shaders/basic.vert
 M src/combat/explosion-fx.cpp
 M src/combat/weapon-execution.cpp
 M src/combat/weapon-json-config.cpp
 M src/combat/weapon-types.h
 M src/effects/effect-part-blood.cpp
 M src/engine/engine-tick-render.cpp
 M src/entities/player-loader.cpp
 M src/entities/player-render.cpp
 M src/entities/player.cpp
 M src/entities/player.h
 M src/gamemode/map-config.cpp
 M src/gamemode/map-config.h
 M src/network/confirmed-damage-presentation.cpp
 M src/network/multiplayer-interpolation.cpp
 M src/network/multiplayer-packets.cpp
 M src/network/multiplayer-projectiles.cpp
 M src/network/multiplayer-shots.cpp
 M src/network/multiplayer-tick.cpp
 M src/network/server-projectiles.cpp
 M src/network/vehicle-networking.cpp
 M src/physics/physical-entity.cpp
 M src/ragdoll/ragdoll-mode.cpp
 M src/ragdoll/ragdoll-mode.h
 M src/vehicles/vehicle-config.cpp
 M src/vehicles/vehicle-system.cpp
 M src/vehicles/vehicle-types.h
?? assets/sound/weapon/ak47/
?? assets/sound/weapon/judgement/
?? devscripts/quality/test_game_build_failfast_priority.py
?? docs/changelog/2026-11-24/20261124-fail-fast-game-dll-build-timeout.md
?? docs/changelog/2026-11-24/20261124_1444-wait-review-issue-f15a2daf6c16.md
?? docs/changelog/2026-11-24/20261124_184329-map-editor-lights-issue.md
?? docs/changelog/2026-11-24/20261124_1845-rl-reference-vehicle-physics.md
?? docs/changelog/2026-11-24/20261124_1848-issue-0cdac76fb006-independent-review.md
?? docs/changelog/2026-11-24/20261124_1851-wait-review-a6017d163afb.md
?? docs/changelog/2026-11-24/20261124_1857-cpu-no-mimita-host-control.md
?? docs/changelog/2026-11-24/20261124_1916-issue-0539d967d696-independent-review.md
?? docs/changelog/2026-11-24/20261124_1920-cpu-loop-generation604-pressure-only.md
?? docs/changelog/2026-11-24/20261124_1930-cpu-next-candidate-review.md
?? docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md
?? docs/changelog/2026-11-24/20261124_193533-bloodtest-vision-pass-world-impact-damage-evidence-specs.md
?? docs/changelog/2026-11-24/20261124_1948-issue-5d82fc58a875-independent-review.md
?? docs/changelog/2026-11-24/20261124_2000-cpu-gen609-pressure-and-replay-stall.md
?? docs/changelog/2026-11-24/20261124_200800-issue-536424f63590-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_2025-cpu-iteration-gate-and-history-lead.md
?? docs/issues/build-friction/20261124-game-dll-link-fails-on-undefined-mimitanet-mp-sy-issue-2e6440940e32.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-536424f63590.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-74f939fb99fc.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-7ef5043ce95c.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a0fa88737830.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a4eea8fe2246.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-b18b7d8cf193.md
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
 config/hot-reload/packet-ledger.json               | 164 ++++-----
 config/impact_decals.json                          |  11 +-
 config/lighting.json                               |  13 +-
 config/maps/arena1.json                            |  35 +-
 config/vehicles/octane_rl.json                     |  14 +-
 config/weapon-tracers.json                         |   5 +-
 config/weapons/bloodtest_ak47/config.json          |  21 +-
 config/weapons/bloodtest_judgement/config.json     |  21 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/game_build.py                  |   5 +-
 devscripts/game_build_worker.py                    | 136 +++++++-
 .../20261124_1740-header-fanout-reduction-plan.md  | 148 +++++++++
 ...124_1746-sampler-setup-cost-and-failed-build.md |  18 +
 docs/issues/README.md                              |  43 ++-
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |  45 +++
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |  16 +
 ...m-match-has-no-team-no-ho-issue-5d82fc58a875.md |  37 +++
 ...e-dll-build-at-game-build-issue-f9ea4f30ad18.md |  45 +++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  16 +
 ...e-dll-build-at-game-build-issue-a236165fa6fc.md |   6 +
 ...e-dll-build-at-game-build-issue-a6017d163afb.md |  46 +++
 ...e-dll-build-at-game-build-issue-d82b02478dff.md |  28 ++
 ...physics-and-car-based-mod-issue-e5a086b0a441.md |  16 +
 ...ient-release-gate-for-all-issue-c8290acca248.md |  51 +++
 ...vehicle-end-to-end-proof--issue-0539d967d696.md |  41 +++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  15 +
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  14 +
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  54 +++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |   6 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 350 +++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 369 +++++++++++++++++++++
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |  24 ++
 ...gh-inclusion-headers-npc--issue-196c3045b4cb.md |   6 +
 docs/specs/dev-loop/build-time-observability.md    |  13 +
 docs/specs/dev-loop/dev-loop.md                    |  11 +-
 docs/specs/gamemodes/bloodtest.md                  |  47 ++-
 docs/specs/vehicles/vehicles.md                    | 137 ++++++--
 shaders/basic.frag                                 |  12 +
 shaders/basic.vert                                 |   2 +
 src/combat/explosion-fx.cpp                        |  24 +-
 src/combat/weapon-execution.cpp                    |  18 +
 src/combat/weapon-json-config.cpp                  |   1 +
 src/combat/weapon-types.h                          |   1 +
 src/effects/effect-part-blood.cpp                  |  60 +++-
 src/engine/engine-tick-render.cpp                  |  46 +++
 src/entities/player-loader.cpp                     | 105 +++++-
 src/entities/player-render.cpp                     |  50 ++-
 src/entities/player.cpp                            |  49 +++
 src/entities/player.h                              |   9 +
 src/gamemode/map-config.cpp                        |   4 +
 src/gamemode/map-config.h                          |   4 +
 src/network/confirmed-damage-presentation.cpp      |  34 +-
 src/network/multiplayer-interpolation.cpp          |  13 +
 src/network/multiplayer-packets.cpp                |   1 +
 src/network/multiplayer-projectiles.cpp            |  26 ++
 src/network/multiplayer-shots.cpp                  |   2 +
 src/network/multiplayer-tick.cpp                   |   2 +
 src/network/server-projectiles.cpp                 | 122 +++++--
 src/network/vehicle-networking.cpp                 |   6 +-
 src/physics/physical-entity.cpp                    | 250 +++++++++-----
 src/ragdoll/ragdoll-mode.cpp                       |  37 ++-
 src/ragdoll/ragdoll-mode.h                         |   4 +
 src/vehicles/vehicle-config.cpp                    |  13 +
 src/vehicles/vehicle-system.cpp                    | 215 +++++++-----
 src/vehicles/vehicle-types.h                       |  13 +
 69 files changed, 2776 insertions(+), 391 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/vehicle_arena.json
M	config/hot-reload/packet-ledger.json
M	config/impact_decals.json
M	config/lighting.json
M	config/maps/arena1.json
M	config/vehicles/octane_rl.json
M	config/weapon-tracers.json
M	config/weapons/bloodtest_ak47/config.json
M	config/weapons/bloodtest_judgement/config.json
M	devscripts/blender-ai
M	devscripts/dev_loop/game_build.py
M	devscripts/game_build_worker.py
M	docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md
M	docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a6017d163afb.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
M	docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/gamemodes/bloodtest.md
M	docs/specs/vehicles/vehicles.md
M	shaders/basic.frag
M	shaders/basic.vert
M	src/combat/explosion-fx.cpp
M	src/combat/weapon-execution.cpp
M	src/combat/weapon-json-config.cpp
M	src/combat/weapon-types.h
M	src/effects/effect-part-blood.cpp
M	src/engine/engine-tick-render.cpp
M	src/entities/player-loader.cpp
M	src/entities/player-render.cpp
M	src/entities/player.cpp
M	src/entities/player.h
M	src/gamemode/map-config.cpp
M	src/gamemode/map-config.h
M	src/network/confirmed-damage-presentation.cpp
M	src/network/multiplayer-interpolation.cpp
M	src/network/multiplayer-packets.cpp
M	src/network/multiplayer-projectiles.cpp
M	src/network/multiplayer-shots.cpp
M	src/network/multiplayer-tick.cpp
M	src/network/server-projectiles.cpp
M	src/network/vehicle-networking.cpp
M	src/physics/physical-entity.cpp
M	src/ragdoll/ragdoll-mode.cpp
M	src/ragdoll/ragdoll-mode.h
M	src/vehicles/vehicle-config.cpp
M	src/vehicles/vehicle-system.cpp
M	src/vehicles/vehicle-types.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-24T19:27:02.742428Z",
    "latest_successful_build_id": "2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63",
    "latest_successful_source_generation": 40
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "d4b2aec095a0dc4360e066c22a2193619f99e25d8cae3dc27cce9ea3fa2e10d0",
    "dll": "build\\game\\generation-617\\game-g617.dll",
    "generation": 617,
    "revision": "c7d650e35721fcd630da0c641c5de322d5f87a16d294d62c80808ff75d61a935",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-617\\game-g617.dll",
    "generation": 617,
    "published_utc": "2026-11-24T20:26:28.347Z",
    "revision": "c7d650e35721fcd630da0c641c5de322d5f87a16d294d62c80808ff75d61a935",
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
