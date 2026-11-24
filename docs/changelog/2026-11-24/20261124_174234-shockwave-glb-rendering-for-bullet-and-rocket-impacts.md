// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Shockwave GLB rendering for bullet and rocket impacts

# Task

- Task ID: session-20261124-174228
- Session ID: session-20261124-174228
- Owner: opencode-deepseek-v4-1
- Focus: Runtime evidence on game generation 595, run 20261124_170535: effect.model.load for assets/objects/things/effects-objects/shockwave-v1.glb with 288 vertices, 1 batch, normalized=true; explosion.shockwave.draw with render_path=model_mesh, alpha 0.5 at age 0.0, scale 0.5, then alpha 0.4167 and scale 0.75 at age 0.1667. Build 595 published. The follow-up shader-state cleanup and shockwaveModelNormalize toggle are source-complete but the next batch build is blocked by another agent's in-progress vehicle rename (src/vehicles/vehicle-system.cpp still references removed VehicleJumpProfile fields); our translation units compiled. Visual acceptance and the final tick-6 frame remain human/runtime checks.
- Summary: EffectPart models are now rendered with normalized scale, surface-normal alignment and spin, color/alpha interpolation, and optional weapon texture override. Bullet impacts use the authored shockwave-v1.glb instead of falling through to a generic sphere, and rocket shockwaves prefer the model over the PNG fallback. Global shader opacity/proximity uniforms are restored after each model draw to prevent state leaking into later actors.
- Status: built and runtime proven on generation 595; shader-state cleanup build blocked by unrelated vehicle preflight
- Date, time, timezone: 2026-11-24T17:42:32+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d1ec3050f3275c83d302a83349f0a8db64ea3d74
- Final commit: d1ec3050f3275c83d302a83349f0a8db64ea3d74

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
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/gamemodes/vehicle_arena.json` |  |  |  | git checkout -- config/gamemodes/vehicle_arena.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/impact_decals.json` |  |  |  | git checkout -- config/impact_decals.json |
| `config/input/gamepad.json` |  |  |  | git checkout -- config/input/gamepad.json |
| `config/maps/arena1.json` |  |  |  | git checkout -- config/maps/arena1.json |
| `config/ragdoll.json` |  |  |  | git checkout -- config/ragdoll.json |
| `config/tips.json` |  |  |  | git checkout -- config/tips.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `config/weapons/bloodtest_judgement/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_judgement/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/build_resource_sampler.py` |  |  |  | git checkout -- devscripts/dev_loop/build_resource_sampler.py |
| `devscripts/quality/test_build_resource_sampler.py` |  |  |  | git checkout -- devscripts/quality/test_build_resource_sampler.py |
| `docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md |
| `docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md |
| `docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md` |  |  |  | git checkout -- docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md |
| `docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md |
| `docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` |  |  |  | git checkout -- docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md |
| `docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md` |  |  |  | git checkout -- docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md |
| `docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md` |  |  |  | git checkout -- docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md |
| `docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md |
| `docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md` |  |  |  | git checkout -- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md |
| `docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md` |  |  |  | git checkout -- docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/networking/networking.md` |  |  |  | git checkout -- docs/specs/networking/networking.md |
| `docs/specs/vehicles/vehicles.md` |  |  |  | git checkout -- docs/specs/vehicles/vehicles.md |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/weapon-execution.cpp` |  |  |  | git checkout -- src/combat/weapon-execution.cpp |
| `src/combat/weapon-json-config.cpp` |  |  |  | git checkout -- src/combat/weapon-json-config.cpp |
| `src/combat/weapon-types.h` |  |  |  | git checkout -- src/combat/weapon-types.h |
| `src/config/camera-config.cpp` |  |  |  | git checkout -- src/config/camera-config.cpp |
| `src/config/camera-config.h` |  |  |  | git checkout -- src/config/camera-config.h |
| `src/debug/debug-visuals-labels.cpp` |  |  |  | git checkout -- src/debug/debug-visuals-labels.cpp |
| `src/debug/debug-visuals.h` |  |  |  | git checkout -- src/debug/debug-visuals.h |
| `src/effects/effect-part-render.cpp` |  |  |  | git checkout -- src/effects/effect-part-render.cpp |
| `src/effects/effect-part.h` |  |  |  | git checkout -- src/effects/effect-part.h |
| `src/engine/engine-tick-ui-game-hud.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui-game-hud.cpp |
| `src/engine/engine-tick-ui.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui.cpp |
| `src/engine/engine-tick-ui.h` |  |  |  | git checkout -- src/engine/engine-tick-ui.h |
| `src/engine/engine-tick.cpp` |  |  |  | git checkout -- src/engine/engine-tick.cpp |
| `src/gamemode/gamemode-runtime-resolver.cpp` |  |  |  | git checkout -- src/gamemode/gamemode-runtime-resolver.cpp |
| `src/gamemode/gamemode.cpp` |  |  |  | git checkout -- src/gamemode/gamemode.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `src/gui/gui-main.cpp` |  |  |  | git checkout -- src/gui/gui-main.cpp |
| `src/gui/ui-system-internal.h` |  |  |  | git checkout -- src/gui/ui-system-internal.h |
| `src/gui/ui-system-render.cpp` |  |  |  | git checkout -- src/gui/ui-system-render.cpp |
| `src/gui/ui-system.cpp` |  |  |  | git checkout -- src/gui/ui-system.cpp |
| `src/gui/ui-system.h` |  |  |  | git checkout -- src/gui/ui-system.h |
| `src/input/gamepad-input.cpp` |  |  |  | git checkout -- src/input/gamepad-input.cpp |
| `src/input/gamepad-input.h` |  |  |  | git checkout -- src/input/gamepad-input.h |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/terminal/debug-commands.cpp` |  |  |  | git checkout -- src/terminal/debug-commands.cpp |
| `src/vehicles/vehicle-profile.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-profile.cpp |
| `src/vehicles/vehicle-profile.h` |  |  |  | git checkout -- src/vehicles/vehicle-profile.h |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `src/vehicles/vehicle-types.h` |  |  |  | git checkout -- src/vehicles/vehicle-types.h |
| `assets/entity/player/default/bodylayers/` |  |  |  | Remove-Item -LiteralPath 'assets/entity/player/default/bodylayers/' |
| `config/input/gamecontrollerdb.LICENSE.txt` |  |  |  | Remove-Item -LiteralPath 'config/input/gamecontrollerdb.LICENSE.txt' |
| `config/input/gamecontrollerdb.txt` |  |  |  | Remove-Item -LiteralPath 'config/input/gamecontrollerdb.txt' |
| `config/vehicle-profiles/rl_soccar.json` |  |  |  | Remove-Item -LiteralPath 'config/vehicle-profiles/rl_soccar.json' |
| `config/vehicles/octane_rl.json` |  |  |  | Remove-Item -LiteralPath 'config/vehicles/octane_rl.json' |
| `docs/changelog/2026-11-24/20261124_0845-issue-3af736bb9337-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_0845-issue-3af736bb9337-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_0915-mode3-latency-ownership-and-runtime-recheck.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_0915-mode3-latency-ownership-and-runtime-recheck.md' |
| `docs/changelog/2026-11-24/20261124_0946-issue-816c05804889-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_0946-issue-816c05804889-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_1045-nonblocking-identity-and-controller-db.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1045-nonblocking-identity-and-controller-db.md' |
| `docs/changelog/2026-11-24/20261124_1230-issue-9a301a157e7c-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1230-issue-9a301a157e7c-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_1236-issue-8576955c4fb5-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1236-issue-8576955c4fb5-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_123857-bloodtest-presentation-slice-json-hud-policy-ui-ca-bodycam-fov-bullet-shockwaves-gore-tuning.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_123857-bloodtest-presentation-slice-json-hud-policy-ui-ca-bodycam-fov-bullet-shockwaves-gore-tuning.md' |
| `docs/changelog/2026-11-24/20261124_1239-issue-b18e63894585-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1239-issue-b18e63894585-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1250-mode3-module-generation-refresh-source-trace.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1250-mode3-module-generation-refresh-source-trace.md' |
| `docs/changelog/2026-11-24/20261124_1257-issue-0322fa45b0e8-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1257-issue-0322fa45b0e8-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_1329-issue-13d080318e1c-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1329-issue-13d080318e1c-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_135058-mode3-current-resource-gate.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_135058-mode3-current-resource-gate.md' |
| `docs/changelog/2026-11-24/20261124_140354-cpu-profile-generation-gate.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_140354-cpu-profile-generation-gate.md' |
| `docs/changelog/2026-11-24/20261124_141555-runtime-mode-alignment-freshness.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_141555-runtime-mode-alignment-freshness.md' |
| `docs/changelog/2026-11-24/20261124_143200-resource-pair-mode-alignment.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_143200-resource-pair-mode-alignment.md' |
| `docs/changelog/2026-11-24/20261124_143809-resource-optimization-runtime-gate.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_143809-resource-optimization-runtime-gate.md' |
| `docs/changelog/2026-11-24/20261124_1450-root-runtime-recovery-blocker.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1450-root-runtime-recovery-blocker.md' |
| `docs/changelog/2026-11-24/20261124_1452-issue-0e981ef03d32-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1452-issue-0e981ef03d32-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1500-bounded-server-only-resource-sample.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1500-bounded-server-only-resource-sample.md' |
| `docs/changelog/2026-11-24/20261124_1518-bounded-callsite-probe-bloodtest-pair.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1518-bounded-callsite-probe-bloodtest-pair.md' |
| `docs/changelog/2026-11-24/20261124_1533-issue-58ecf44ac956-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1533-issue-58ecf44ac956-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1542-mode3-multifile-latency-evidence-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1542-mode3-multifile-latency-evidence-audit.md' |
| `docs/changelog/2026-11-24/20261124_1547-candidate-module-build-authority-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1547-candidate-module-build-authority-audit.md' |
| `docs/changelog/2026-11-24/20261124_1602-issue-80dfb9ab9c5c-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1602-issue-80dfb9ab9c5c-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1630-collision-logger-config-ab-result.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1630-collision-logger-config-ab-result.md' |
| `docs/changelog/2026-11-24/20261124_1645-issue-6415397775a2-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1645-issue-6415397775a2-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_1648-candidate-dll-builder-path-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1648-candidate-dll-builder-path-review.md' |
| `docs/changelog/2026-11-24/20261124_1730-build-sampler-pressure-snapshot.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1730-build-sampler-pressure-snapshot.md' |
| `docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md' |
| `docs/coordination/decisions/20261124_102412-conflict-c1dde7354156-decision-b4ccee465b05.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_102412-conflict-c1dde7354156-decision-b4ccee465b05.md' |
| `docs/coordination/decisions/20261124_111458-retain-gen590-changelog-append-decision-33d62425dbfd.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_111458-retain-gen590-changelog-append-decision-33d62425dbfd.md' |
| `docs/coordination/decisions/20261124_115516-retain-gen591-593-evidence-decision-777163429a6f.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_115516-retain-gen591-593-evidence-decision-777163429a6f.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-5fe3dcd5826b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-5fe3dcd5826b.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-3472692a57c2.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-3472692a57c2.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-6613d3354c65.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-6613d3354c65.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8576955c4fb5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8576955c4fb5.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8b5c92a76d97.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8b5c92a76d97.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-9a301a157e7c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-9a301a157e7c.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-c7d9edb5e68b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-c7d9edb5e68b.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee437972915e.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee437972915e.md' |

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
- Focus: Runtime evidence on game generation 595, run 20261124_170535: effect.model.load for assets/objects/things/effects-objects/shockwave-v1.glb with 288 vertices, 1 batch, normalized=true; explosion.shockwave.draw with render_path=model_mesh, alpha 0.5 at age 0.0, scale 0.5, then alpha 0.4167 and scale 0.75 at age 0.1667. Build 595 published. The follow-up shader-state cleanup and shockwaveModelNormalize toggle are source-complete but the next batch build is blocked by another agent's in-progress vehicle rename (src/vehicles/vehicle-system.cpp still references removed VehicleJumpProfile fields); our translation units compiled. Visual acceptance and the final tick-6 frame remain human/runtime checks.
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d1ec3050f3275c83d302a83349f0a8db64ea3d74 / d1ec3050f3275c83d302a83349f0a8db64ea3d74
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-24T17:05:20.935679Z", "latest_successful_build_id": "2026-11-24T17:05:20.935679Z:c8f5b6b4f9d0ca8a0c2400890dd2cba972bd66fa0a23cd323077460a21de516d", "latest_successful_source_generation": 12}, "game_current": {"build_mode": "debug", "code_hash": "8e21512502190db3918ec57c752c7ec4c5edaf3fc52429473d71014a09f14ba1", "dll": "build\\game\\generation-595\\game-g595.dll", "generation": 595, "revision": "d6a3f7ccf7adda8de602710e1adf00c7d81565fb7ff0ea930afc0c6bc83ca84d", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-595\\game-g595.dll", "generation": 595, "published_utc": "2026-11-24T17:00:47.096Z", "revision": "d6a3f7ccf7adda8de602710e1adf00c7d81565fb7ff0ea930afc0c6bc83ca84d", "status": "ready"}, "progress": {"compiled": 12, "elapsed_s": 105.626, "phase": "build_complete", "result": "success", "skipped": 75, "updated_utc": "2026-11-24T17:05:08.989Z"}}
- Generation: 595
- Artifact: build\game\generation-595\game-g595.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_170535\server-01\events.jsonl
- Session ID: session-20261124-174228

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
 M config/gamemodes/bloodtest.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/gamemodes/vehicle_arena.json
 M config/hot-reload/packet-ledger.json
 M config/impact_decals.json
 M config/input/gamepad.json
 M config/maps/arena1.json
 M config/ragdoll.json
 M config/tips.json
 M config/video-settings.json
 M config/weapons/bloodtest_ak47/config.json
 M config/weapons/bloodtest_judgement/config.json
 m devscripts/blender-ai
 M devscripts/dev_loop/build_resource_sampler.py
 M devscripts/quality/test_build_resource_sampler.py
 M docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
 M docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md
 M docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md
 M docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md
 M docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
 M docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
 M docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md
 M docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
 M docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/networking/networking.md
 M docs/specs/vehicles/vehicles.md
 M src/combat/explosion-fx.cpp
 M src/combat/weapon-execution.cpp
 M src/combat/weapon-json-config.cpp
 M src/combat/weapon-types.h
 M src/config/camera-config.cpp
 M src/config/camera-config.h
 M src/debug/debug-visuals-labels.cpp
 M src/debug/debug-visuals.h
 M src/effects/effect-part-render.cpp
 M src/effects/effect-part.h
 M src/engine/engine-tick-ui-game-hud.cpp
 M src/engine/engine-tick-ui.cpp
 M src/engine/engine-tick-ui.h
 M src/engine/engine-tick.cpp
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/gui/gui-main.cpp
 M src/gui/ui-system-internal.h
 M src/gui/ui-system-render.cpp
 M src/gui/ui-system.cpp
 M src/gui/ui-system.h
 M src/input/gamepad-input.cpp
 M src/input/gamepad-input.h
 M src/network/multiplayer-tick.cpp
 M src/network/server-projectiles.cpp
 M src/terminal/debug-commands.cpp
 M src/vehicles/vehicle-profile.cpp
 M src/vehicles/vehicle-profile.h
 M src/vehicles/vehicle-system.cpp
 M src/vehicles/vehicle-types.h
?? assets/entity/player/default/bodylayers/
?? config/input/gamecontrollerdb.LICENSE.txt
?? config/input/gamecontrollerdb.txt
?? config/vehicle-profiles/rl_soccar.json
?? config/vehicles/octane_rl.json
?? docs/changelog/2026-11-24/20261124_0845-issue-3af736bb9337-independent-review.md
?? docs/changelog/2026-11-24/20261124_0915-mode3-latency-ownership-and-runtime-recheck.md
?? docs/changelog/2026-11-24/20261124_0946-issue-816c05804889-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_1045-nonblocking-identity-and-controller-db.md
?? docs/changelog/2026-11-24/20261124_1230-issue-9a301a157e7c-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_1236-issue-8576955c4fb5-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_123857-bloodtest-presentation-slice-json-hud-policy-ui-ca-bodycam-fov-bullet-shockwaves-gore-tuning.md
?? docs/changelog/2026-11-24/20261124_1239-issue-b18e63894585-independent-review.md
?? docs/changelog/2026-11-24/20261124_1250-mode3-module-generation-refresh-source-trace.md
?? docs/changelog/2026-11-24/20261124_1257-issue-0322fa45b0e8-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_1329-issue-13d080318e1c-independent-review.md
?? docs/changelog/2026-11-24/20261124_135058-mode3-current-resource-gate.md
?? docs/changelog/2026-11-24/20261124_140354-cpu-profile-generation-gate.md
?? docs/changelog/2026-11-24/20261124_141555-runtime-mode-alignment-freshness.md
?? docs/changelog/2026-11-24/20261124_143200-resource-pair-mode-alignment.md
?? docs/changelog/2026-11-24/20261124_143809-resource-optimization-runtime-gate.md
?? docs/changelog/2026-11-24/20261124_1450-root-runtime-recovery-blocker.md
?? docs/changelog/2026-11-24/20261124_1452-issue-0e981ef03d32-independent-review.md
?? docs/changelog/2026-11-24/20261124_1500-bounded-server-only-resource-sample.md
?? docs/changelog/2026-11-24/20261124_1518-bounded-callsite-probe-bloodtest-pair.md
?? docs/changelog/2026-11-24/20261124_1533-issue-58ecf44ac956-independent-review.md
?? docs/changelog/2026-11-24/20261124_1542-mode3-multifile-latency-evidence-audit.md
?? docs/changelog/2026-11-24/20261124_1547-candidate-module-build-authority-audit.md
?? docs/changelog/2026-11-24/20261124_1602-issue-80dfb9ab9c5c-independent-review.md
?? docs/changelog/2026-11-24/20261124_1630-collision-logger-config-ab-result.md
?? docs/changelog/2026-11-24/20261124_1645-issue-6415397775a2-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_1648-candidate-dll-builder-path-review.md
?? docs/changelog/2026-11-24/20261124_1730-build-sampler-pressure-snapshot.md
?? docs/changelog/2026-11-24/20261124_1740-header-fanout-reduction-plan.md
?? docs/coordination/decisions/20261124_102412-conflict-c1dde7354156-decision-b4ccee465b05.md
?? docs/coordination/decisions/20261124_111458-retain-gen590-changelog-append-decision-33d62425dbfd.md
?? docs/coordination/decisions/20261124_115516-retain-gen591-593-evidence-decision-777163429a6f.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-5fe3dcd5826b.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-3472692a57c2.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-6613d3354c65.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8576955c4fb5.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8b5c92a76d97.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-9a301a157e7c.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-c7d9edb5e68b.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-d82b02478dff.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee437972915e.md
```

## Captured diff stat

```text
config/accounts/default.json                       |    6 +-
 config/analytics.json                              |    2 +-
 config/audio/music-settings.json                   |    2 +-
 config/gamemodes/bloodtest.json                    |   19 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |   15 +
 config/gamemodes/vehicle_arena.json                |    2 +-
 config/hot-reload/packet-ledger.json               |  160 +-
 config/impact_decals.json                          |   11 +-
 config/input/gamepad.json                          |   33 +-
 config/maps/arena1.json                            | 1153 +++-
 config/ragdoll.json                                |   13 +-
 config/tips.json                                   |    2 +-
 config/video-settings.json                         |    2 +-
 config/weapons/bloodtest_ak47/config.json          |   57 +-
 config/weapons/bloodtest_judgement/config.json     |   39 +-
 devscripts/blender-ai                              |    0
 devscripts/dev_loop/build_resource_sampler.py      |   74 +-
 devscripts/quality/test_build_resource_sampler.py  |  123 +
 ...source-correction-and-blocked-paired-profile.md |   99 +
 docs/issues/README.md                              |   55 +-
 ...rs-fail-to-consistently-e-issue-b95574b63f29.md |   62 +
 ...-not-fire-at-opposing-act-issue-1b2463da5666.md |    6 +
 ...ge-repeatedly-times-out-b-issue-0e981ef03d32.md |   40 +
 ...e-but-its-control-endpoin-issue-80dfb9ab9c5c.md |   53 +
 ...launch-requests-from-live-issue-c481ab36713a.md |   42 +
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |   24 +
 ...igation-regresses-into-co-issue-b18e63894585.md |   43 +
 ...time-journal-events-wait--issue-6e2d7b91e7d9.md |    8 +
 ...es-desktop-content-instea-issue-130ee745052a.md |    6 +
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |   41 +
 ...e-dll-build-at-game-build-issue-6415397775a2.md |   38 +
 ...e-dll-build-at-game-build-issue-dad513d47108.md |   34 +
 ...e-dll-build-at-game-build-issue-e96486c8cc3e.md |    6 +
 ...rocess-identity-lookup-ex-issue-58ecf44ac956.md |   44 +
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   56 +
 ...budget-exe-build-at-setup-issue-128ca34b21c6.md |   39 +
 ...e-dll-build-at-game-build-issue-816c05804889.md |   22 +
 ...physics-and-car-based-mod-issue-e5a086b0a441.md |    8 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |   70 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |   15 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |   32 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  748 ++
 ...ry-client-frame-below-240-issue-13d080318e1c.md |   31 +
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 7284 +++++++++++---------
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |   47 +
 ...lity-needs-loud-diagnosab-issue-a5ca672b6628.md |   19 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |    6 +
 docs/specs/dev-loop/agent-coordination.md          |   42 +
 docs/specs/networking/networking.md                |   36 +-
 docs/specs/vehicles/vehicles.md                    |   37 +
 src/combat/explosion-fx.cpp                        |   61 +-
 src/combat/weapon-execution.cpp                    |    9 +
 src/combat/weapon-json-config.cpp                  |    3 +
 src/combat/weapon-types.h                          |    3 +
 src/config/camera-config.cpp                       |    1 +
 src/config/camera-config.h                         |    1 +
 src/debug/debug-visuals-labels.cpp                 |    1 +
 src/debug/debug-visuals.h                          |    1 +
 src/effects/effect-part-render.cpp                 |  128 +-
 src/effects/effect-part.h                          |    7 +
 src/engine/engine-tick-ui-game-hud.cpp             |    9 +-
 src/engine/engine-tick-ui.cpp                      |    5 +-
 src/engine/engine-tick-ui.h                        |    3 +-
 src/engine/engine-tick.cpp                         |   22 +-
 src/gamemode/gamemode-runtime-resolver.cpp         |   11 +
 src/gamemode/gamemode.cpp                          |   15 +
 src/gamemode/gamemode.h                            |   17 +
 src/gui/gui-main.cpp                               |   17 +-
 src/gui/ui-system-internal.h                       |    1 +
 src/gui/ui-system-render.cpp                       |   20 +-
 src/gui/ui-system.cpp                              |   32 +-
 src/gui/ui-system.h                                |   13 +-
 src/input/gamepad-input.cpp                        |  182 +-
 src/input/gamepad-input.h                          |   14 +-
 src/network/multiplayer-tick.cpp                   |   95 +-
 src/network/server-projectiles.cpp                 |   38 +-
 src/terminal/debug-commands.cpp                    |   25 +
 src/vehicles/vehicle-profile.cpp                   |  267 +-
 src/vehicles/vehicle-profile.h                     |  120 +-
 src/vehicles/vehicle-system.cpp                    |  610 +-
 src/vehicles/vehicle-types.h                       |   41 +
 81 files changed, 8675 insertions(+), 3903 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/gamemodes/vehicle_arena.json
M	config/hot-reload/packet-ledger.json
M	config/impact_decals.json
M	config/input/gamepad.json
M	config/maps/arena1.json
M	config/ragdoll.json
M	config/tips.json
M	config/video-settings.json
M	config/weapons/bloodtest_ak47/config.json
M	config/weapons/bloodtest_judgement/config.json
M	devscripts/blender-ai
M	devscripts/dev_loop/build_resource_sampler.py
M	devscripts/quality/test_build_resource_sampler.py
M	docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
M	docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md
M	docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md
M	docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md
M	docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
M	docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
M	docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md
M	docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
M	docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/networking/networking.md
M	docs/specs/vehicles/vehicles.md
M	src/combat/explosion-fx.cpp
M	src/combat/weapon-execution.cpp
M	src/combat/weapon-json-config.cpp
M	src/combat/weapon-types.h
M	src/config/camera-config.cpp
M	src/config/camera-config.h
M	src/debug/debug-visuals-labels.cpp
M	src/debug/debug-visuals.h
M	src/effects/effect-part-render.cpp
M	src/effects/effect-part.h
M	src/engine/engine-tick-ui-game-hud.cpp
M	src/engine/engine-tick-ui.cpp
M	src/engine/engine-tick-ui.h
M	src/engine/engine-tick.cpp
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/gui/gui-main.cpp
M	src/gui/ui-system-internal.h
M	src/gui/ui-system-render.cpp
M	src/gui/ui-system.cpp
M	src/gui/ui-system.h
M	src/input/gamepad-input.cpp
M	src/input/gamepad-input.h
M	src/network/multiplayer-tick.cpp
M	src/network/server-projectiles.cpp
M	src/terminal/debug-commands.cpp
M	src/vehicles/vehicle-profile.cpp
M	src/vehicles/vehicle-profile.h
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
    "latest_successful_build": "2026-11-24T17:05:20.935679Z",
    "latest_successful_build_id": "2026-11-24T17:05:20.935679Z:c8f5b6b4f9d0ca8a0c2400890dd2cba972bd66fa0a23cd323077460a21de516d",
    "latest_successful_source_generation": 12
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "8e21512502190db3918ec57c752c7ec4c5edaf3fc52429473d71014a09f14ba1",
    "dll": "build\\game\\generation-595\\game-g595.dll",
    "generation": 595,
    "revision": "d6a3f7ccf7adda8de602710e1adf00c7d81565fb7ff0ea930afc0c6bc83ca84d",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-595\\game-g595.dll",
    "generation": 595,
    "published_utc": "2026-11-24T17:00:47.096Z",
    "revision": "d6a3f7ccf7adda8de602710e1adf00c7d81565fb7ff0ea930afc0c6bc83ca84d",
    "status": "ready"
  },
  "progress": {
    "compiled": 12,
    "elapsed_s": 105.626,
    "phase": "build_complete",
    "result": "success",
    "skipped": 75,
    "updated_utc": "2026-11-24T17:05:08.989Z"
  }
}
```
