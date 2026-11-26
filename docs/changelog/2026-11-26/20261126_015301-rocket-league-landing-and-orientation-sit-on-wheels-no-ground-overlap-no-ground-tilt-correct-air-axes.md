// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Rocket League landing and orientation: sit on wheels, no ground overlap, no ground tilt, correct air axes

# Task

- Task ID: ses_deb36bbbeffeVJQ2EQpHx7j69w
- Session ID: ses_deb36bbbeffeVJQ2EQpHx7j69w
- Owner: rocket-league-landing
- Focus: vehicle ground contact, landing settle, air input direction, proof events
- Summary: Landing and orientation are now contact-driven and provable. The car's collision box is its full envelope with its bottom face on the wheel contact plane (collisionCenter in the template), so at rest no part of the car is below the surface; the wheel mesh follows suspension travel instead of sinking; the chassis is lifted out of any measured floor overlap. The old any-wheel angular damping and the ability to sleep while tilted are gone, so a car that lands on a corner tips over instead of freezing there. A profile ground.attitude block makes a grounded car rigid: it keeps only yaw and follows the averaged wheel contact normal, so throttle and reverse no longer pitch it. Air roll and yaw signs are flipped to match the stick, and the dodge horizontal axis with them. New evidence: vehicle.landing, vehicle.attitude (intent vs signed local rates, resting penetration, chassis z, contact normal), vehicle.penetration (sustained only), vehicle.rest.tilted, and two all-samples assertion types in the dev-loop. Proven on generation 664: flat jump landing up_dot 1.0000 with wheel_mask 15 and zero penetration (run 20261126_014117); hard landing plus wall ram with zero penetration events (013852); 3726 grounded samples with pitch and roll rates 0.000 under throttle and reverse (014657); roll and yaw intent matched measured rotation with zero mismatches (014900); 90-degree roll landing touches down on the chassis, no tilted rest, 0.005 m overlap (014423).
- Status: complete
- Date, time, timezone: 2026-11-26T01:52:59+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: f9d94c5bb3cffd8bb15899cdda30eea3264c4a66
- Final commit: f9d94c5bb3cffd8bb15899cdda30eea3264c4a66

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
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/maps/arena1.json` |  |  |  | git checkout -- config/maps/arena1.json |
| `config/onlinemodes.json` |  |  |  | git checkout -- config/onlinemodes.json |
| `config/vehicle-profiles/rl_soccar.json` |  |  |  | git checkout -- config/vehicle-profiles/rl_soccar.json |
| `config/vehicle-profiles/rl_soccar_freeplay.json` |  |  |  | git checkout -- config/vehicle-profiles/rl_soccar_freeplay.json |
| `config/vehicles/octane_rl.json` |  |  |  | git checkout -- config/vehicles/octane_rl.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-launch-modes.json` |  |  |  | git checkout -- devscripts/dev-launch-modes.json |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` |  |  |  | git checkout -- docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md |
| `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md` |  |  |  | git checkout -- docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-ba939787ffa3.md` |  |  |  | git checkout -- docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-ba939787ffa3.md |
| `docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md` |  |  |  | git checkout -- docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md |
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` |  |  |  | git checkout -- docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md |
| `docs/issues/bug/20261125-build-a-rocket-league-style-mode-through-staged--issue-41547751ca1c.md` |  |  |  | git checkout -- docs/issues/bug/20261125-build-a-rocket-league-style-mode-through-staged--issue-41547751ca1c.md |
| `docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md` |  |  |  | git checkout -- docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md |
| `docs/issues/bug/20261125-make-vehicle-freeplay-inputs-and-presentation-us-issue-60e46e12db9b.md` |  |  |  | git checkout -- docs/issues/bug/20261125-make-vehicle-freeplay-inputs-and-presentation-us-issue-60e46e12db9b.md |
| `docs/issues/bug/20261125-vehicle-mode-configs-reject-vehicle-possess-acto-issue-4e0dc0d71267.md` |  |  |  | git checkout -- docs/issues/bug/20261125-vehicle-mode-configs-reject-vehicle-possess-acto-issue-4e0dc0d71267.md |
| `docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md |
| `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9215d710021d.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9215d710021d.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md |
| `docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md` |  |  |  | git checkout -- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md |
| `docs/issues/capability/20261121-let-ai-inspect-existing-live-game-journals-as-pr-issue-9b5be6c88de0.md` |  |  |  | git checkout -- docs/issues/capability/20261121-let-ai-inspect-existing-live-game-journals-as-pr-issue-9b5be6c88de0.md |
| `docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md` |  |  |  | git checkout -- docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/performance/20261125-dev-loop-dashboard-should-cost-under-1-percent-c-issue-acf28dc22c85.md` |  |  |  | git checkout -- docs/issues/performance/20261125-dev-loop-dashboard-should-cost-under-1-percent-c-issue-acf28dc22c85.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/specs/gamemodes/bloodtest.md` |  |  |  | git checkout -- docs/specs/gamemodes/bloodtest.md |
| `docs/specs/gamemodes/vehicle-arena.md` |  |  |  | git checkout -- docs/specs/gamemodes/vehicle-arena.md |
| `docs/specs/vehicles/vehicles.md` |  |  |  | git checkout -- docs/specs/vehicles/vehicles.md |
| `src/engine/engine-tick-camera.cpp` |  |  |  | git checkout -- src/engine/engine-tick-camera.cpp |
| `src/engine/engine-tick.cpp` |  |  |  | git checkout -- src/engine/engine-tick.cpp |
| `src/entities/actor-body.cpp` |  |  |  | git checkout -- src/entities/actor-body.cpp |
| `src/gamemode/gamemode-runtime-resolver.cpp` |  |  |  | git checkout -- src/gamemode/gamemode-runtime-resolver.cpp |
| `src/gamemode/gamemode.cpp` |  |  |  | git checkout -- src/gamemode/gamemode.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `src/host/generated/packet-registry-v1.h` |  |  |  | git checkout -- src/host/generated/packet-registry-v1.h |
| `src/network/schema/packet-header.h` |  |  |  | git checkout -- src/network/schema/packet-header.h |
| `src/network/schema/packet-vehicle.h` |  |  |  | git checkout -- src/network/schema/packet-vehicle.h |
| `src/network/server-damage.cpp` |  |  |  | git checkout -- src/network/server-damage.cpp |
| `src/network/server-gamemode.cpp` |  |  |  | git checkout -- src/network/server-gamemode.cpp |
| `src/network/vehicle-networking.cpp` |  |  |  | git checkout -- src/network/vehicle-networking.cpp |
| `src/physics/physical-entity.cpp` |  |  |  | git checkout -- src/physics/physical-entity.cpp |
| `src/physics/physical-entity.h` |  |  |  | git checkout -- src/physics/physical-entity.h |
| `src/render/render-player.cpp` |  |  |  | git checkout -- src/render/render-player.cpp |
| `src/vehicles/vehicle-commands.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-commands.cpp |
| `src/vehicles/vehicle-config.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-config.cpp |
| `src/vehicles/vehicle-profile.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-profile.cpp |
| `src/vehicles/vehicle-profile.h` |  |  |  | git checkout -- src/vehicles/vehicle-profile.h |
| `src/vehicles/vehicle-render.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-render.cpp |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `src/vehicles/vehicle-system.h` |  |  |  | git checkout -- src/vehicles/vehicle-system.h |
| `src/vehicles/vehicle-types.h` |  |  |  | git checkout -- src/vehicles/vehicle-types.h |
| `devscripts/scenarios/rocket_airroll_dir_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_airroll_dir_v1.json' |
| `devscripts/scenarios/rocket_ballcam_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_ballcam_v1.json' |
| `devscripts/scenarios/rocket_ground_tilt_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_ground_tilt_v1.json' |
| `devscripts/scenarios/rocket_input_rotation_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_input_rotation_v1.json' |
| `devscripts/scenarios/rocket_jump_airroll_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_jump_airroll_v1.json' |
| `devscripts/scenarios/rocket_landing_flat_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_landing_flat_v1.json' |
| `devscripts/scenarios/rocket_landing_tilted_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_landing_tilted_v1.json' |
| `devscripts/scenarios/rocket_no_underground_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_no_underground_v1.json' |
| `docs/changelog/2026-11-25/20261125-bloodtest-mode6-gore2-camera.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125-bloodtest-mode6-gore2-camera.md' |
| `docs/changelog/2026-11-25/20261125-dashboard-cpu-process-purpose.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125-dashboard-cpu-process-purpose.md' |
| `docs/changelog/2026-11-25/20261125_2133-independent-build-wait-review-issue-f1de1fcd87a2.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2133-independent-build-wait-review-issue-f1de1fcd87a2.md' |
| `docs/changelog/2026-11-25/20261125_2146-independent-issue-review-issue-55bd775c8026.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2146-independent-issue-review-issue-55bd775c8026.md' |
| `docs/changelog/2026-11-25/20261125_220310-build-wait-review-issue-0843a94eae4b.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_220310-build-wait-review-issue-0843a94eae4b.md' |
| `docs/changelog/2026-11-25/20261125_2205-build-wait-review-issue-63ec094b8859.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2205-build-wait-review-issue-63ec094b8859.md' |
| `docs/changelog/2026-11-25/20261125_2233-build-wait-review-issue-bb51d96a1200.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2233-build-wait-review-issue-bb51d96a1200.md' |
| `docs/changelog/2026-11-25/20261125_233840-rocket-league-mode-rotation-on-the-wire-single-input-owner-ball-cam-toggle-launch-gate-fix.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_233840-rocket-league-mode-rotation-on-the-wire-single-input-owner-ball-cam-toggle-launch-gate-fix.md' |
| `docs/changelog/2026-11-25/20261125_2348-cpu-loop-vehicle-state-log-rate-and-landing-plan.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2348-cpu-loop-vehicle-state-log-rate-and-landing-plan.md' |
| `docs/changelog/2026-11-25/20261125_2350-independent-issue-review-issue-ba939787ffa3.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2350-independent-issue-review-issue-ba939787ffa3.md' |
| `docs/changelog/2026-11-25/20261125_rocket-handoff-status-and-landing-blocker.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_rocket-handoff-status-and-landing-blocker.md' |
| `docs/changelog/2026-11-25/20261126_0006-rocket-telemetry-rate-limit-and-handoff-refresh.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261126_0006-rocket-telemetry-rate-limit-and-handoff-refresh.md' |
| `docs/changelog/2026-11-26/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-26/' |
| `docs/handoffs/20261125_rocket-league-mode-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261125_rocket-league-mode-handoff.md' |
| `docs/issues/bug/20261125-dualshock-trigger-axes-at-rest-decode-to-0-5-thr-issue-4b39c4193cd7.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261125-dualshock-trigger-axes-at-rest-decode-to-0-5-thr-issue-4b39c4193cd7.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-4ab3b491ad6c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-4ab3b491ad6c.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-63ec094b8859.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-63ec094b8859.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-6d572fc03e97.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-6d572fc03e97.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8b7208f291a8.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8b7208f291a8.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-bb51d96a1200.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-bb51d96a1200.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-347500dfb032.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-347500dfb032.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-4a0450674f5f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-4a0450674f5f.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-55479477f3a7.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-55479477f3a7.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-ac23ab33d184.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-ac23ab33d184.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-af48c43ce6c5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-af48c43ce6c5.md' |
| `docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-fa0da68ca2b3.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-fa0da68ca2b3.md' |
| `docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md' |
| `docs/issues/launch-friction/20261125-vehicle-free-play-launch-always-reports-failure--issue-b5a35be7b0e0.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/launch-friction/20261125-vehicle-free-play-launch-always-reports-failure--issue-b5a35be7b0e0.md' |
| `docs/issues/tech-debt/20261125-arcade-arena-vehicle-profile-has-id-n-and-is-ref-issue-0c550dec9353.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/tech-debt/20261125-arcade-arena-vehicle-profile-has-id-n-and-is-ref-issue-0c550dec9353.md' |
| `docs/issues/tech-debt/20261126-vehicle-landing-repeats-for-one-touchdown-when-t-issue-16481f013537.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/tech-debt/20261126-vehicle-landing-repeats-for-one-touchdown-when-t-issue-16481f013537.md' |

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

- Owner: rocket-league-landing
- Focus: vehicle ground contact, landing settle, air input direction, proof events
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / f9d94c5bb3cffd8bb15899cdda30eea3264c4a66 / f9d94c5bb3cffd8bb15899cdda30eea3264c4a66
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-26T01:00:35.913746Z", "latest_successful_build_id": "2026-11-26T01:00:35.913746Z:e18288709713b7f38e81beeb0a3201ea4f85c6c710abe2c484103119cdfec70b", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "2b7c6a6cb1951a7f4e401e5ca05a3b76f995d00eb7ffa0a6124db7306d1a6504", "dll": "build\\game\\generation-664\\game-g664.dll", "generation": 664, "revision": "e782f1d11a9e04e6989989956d5ce172fc62f8cd5b5ba20fa9f46c3ac96b2f2e", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-664\\game-g664.dll", "generation": 664, "published_utc": "2026-11-26T01:37:03.145Z", "revision": "e782f1d11a9e04e6989989956d5ce172fc62f8cd5b5ba20fa9f46c3ac96b2f2e", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.249, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-26T01:48:58.636Z"}}
- Generation: 664
- Artifact: build\game\generation-664\game-g664.dll
- Journal path: C:\mimita-v9\logs\2026-11-26\20261126_014900\client-01\events.jsonl
- Session ID: ses_deb36bbbeffeVJQ2EQpHx7j69w

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
 M config/hot-reload/packet-ledger.json
 M config/maps/arena1.json
 M config/onlinemodes.json
 M config/vehicle-profiles/rl_soccar.json
 M config/vehicle-profiles/rl_soccar_freeplay.json
 M config/vehicles/octane_rl.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-ba939787ffa3.md
 M docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md
 M docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
 M docs/issues/bug/20261125-build-a-rocket-league-style-mode-through-staged--issue-41547751ca1c.md
 M docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md
 M docs/issues/bug/20261125-make-vehicle-freeplay-inputs-and-presentation-us-issue-60e46e12db9b.md
 M docs/issues/bug/20261125-vehicle-mode-configs-reject-vehicle-possess-acto-issue-4e0dc0d71267.md
 M docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9215d710021d.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/capability/20261121-let-ai-inspect-existing-live-game-journals-as-pr-issue-9b5be6c88de0.md
 M docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/performance/20261125-dev-loop-dashboard-should-cost-under-1-percent-c-issue-acf28dc22c85.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/specs/gamemodes/bloodtest.md
 M docs/specs/gamemodes/vehicle-arena.md
 M docs/specs/vehicles/vehicles.md
 M src/engine/engine-tick-camera.cpp
 M src/engine/engine-tick.cpp
 M src/entities/actor-body.cpp
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/host/generated/packet-registry-v1.h
 M src/network/schema/packet-header.h
 M src/network/schema/packet-vehicle.h
 M src/network/server-damage.cpp
 M src/network/server-gamemode.cpp
 M src/network/vehicle-networking.cpp
 M src/physics/physical-entity.cpp
 M src/physics/physical-entity.h
 M src/render/render-player.cpp
 M src/vehicles/vehicle-commands.cpp
 M src/vehicles/vehicle-config.cpp
 M src/vehicles/vehicle-profile.cpp
 M src/vehicles/vehicle-profile.h
 M src/vehicles/vehicle-render.cpp
 M src/vehicles/vehicle-system.cpp
 M src/vehicles/vehicle-system.h
 M src/vehicles/vehicle-types.h
?? devscripts/scenarios/rocket_airroll_dir_v1.json
?? devscripts/scenarios/rocket_ballcam_v1.json
?? devscripts/scenarios/rocket_ground_tilt_v1.json
?? devscripts/scenarios/rocket_input_rotation_v1.json
?? devscripts/scenarios/rocket_jump_airroll_v1.json
?? devscripts/scenarios/rocket_landing_flat_v1.json
?? devscripts/scenarios/rocket_landing_tilted_v1.json
?? devscripts/scenarios/rocket_no_underground_v1.json
?? docs/changelog/2026-11-25/20261125-bloodtest-mode6-gore2-camera.md
?? docs/changelog/2026-11-25/20261125-dashboard-cpu-process-purpose.md
?? docs/changelog/2026-11-25/20261125_2133-independent-build-wait-review-issue-f1de1fcd87a2.md
?? docs/changelog/2026-11-25/20261125_2146-independent-issue-review-issue-55bd775c8026.md
?? docs/changelog/2026-11-25/20261125_220310-build-wait-review-issue-0843a94eae4b.md
?? docs/changelog/2026-11-25/20261125_2205-build-wait-review-issue-63ec094b8859.md
?? docs/changelog/2026-11-25/20261125_2233-build-wait-review-issue-bb51d96a1200.md
?? docs/changelog/2026-11-25/20261125_233840-rocket-league-mode-rotation-on-the-wire-single-input-owner-ball-cam-toggle-launch-gate-fix.md
?? docs/changelog/2026-11-25/20261125_2348-cpu-loop-vehicle-state-log-rate-and-landing-plan.md
?? docs/changelog/2026-11-25/20261125_2350-independent-issue-review-issue-ba939787ffa3.md
?? docs/changelog/2026-11-25/20261125_rocket-handoff-status-and-landing-blocker.md
?? docs/changelog/2026-11-25/20261126_0006-rocket-telemetry-rate-limit-and-handoff-refresh.md
?? docs/changelog/2026-11-26/
?? docs/handoffs/20261125_rocket-league-mode-handoff.md
?? docs/issues/bug/20261125-dualshock-trigger-axes-at-rest-decode-to-0-5-thr-issue-4b39c4193cd7.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-4ab3b491ad6c.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-63ec094b8859.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-6d572fc03e97.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8b7208f291a8.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-bb51d96a1200.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-347500dfb032.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-4a0450674f5f.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-55479477f3a7.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-ac23ab33d184.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-af48c43ce6c5.md
?? docs/issues/build-friction/20261126-wait-over-budget-game-dll-build-at-game-build-issue-fa0da68ca2b3.md
?? docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md
?? docs/issues/launch-friction/20261125-vehicle-free-play-launch-always-reports-failure--issue-b5a35be7b0e0.md
?? docs/issues/tech-debt/20261125-arcade-arena-vehicle-profile-has-id-n-and-is-ref-issue-0c550dec9353.md
?? docs/issues/tech-debt/20261126-vehicle-landing-repeats-for-one-touchdown-when-t-issue-16481f013537.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/gamemodes/bloodtest.json                    |   1 +
 .../gamemodes/masterlist/gamemode-masterlist.json  |   7 +
 config/hot-reload/packet-ledger.json               |   8 +-
 config/maps/arena1.json                            |   4 +-
 config/onlinemodes.json                            |  18 +-
 config/vehicle-profiles/rl_soccar.json             |  26 +-
 config/vehicle-profiles/rl_soccar_freeplay.json    |  23 +-
 config/vehicles/octane_rl.json                     |   8 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-launch-modes.json                   |  13 +
 devscripts/dev-loop.py                             |  23 +
 devscripts/dev_loop/game_build.py                  |   2 +-
 devscripts/dev_loop/gui.py                         |  80 +++-
 docs/issues/README.md                              |  74 +--
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |  36 ++
 ...lision-and-mace-active-po-issue-503f3e410c01.md |  68 +++
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md | 179 ++++++++
 ...-actions-cause-frame-free-issue-ba939787ffa3.md |  54 +++
 ...collidable-in-arcade-mode-issue-87d8c9c7ffb1.md |  38 ++
 ...on-marks-float-away-from--issue-8e48cd0bc786.md |  51 +++
 ...tyle-mode-through-staged--issue-41547751ca1c.md |   8 +
 ...0-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md |  54 +++
 ...nputs-and-presentation-us-issue-60e46e12db9b.md |   8 +
 ...ject-vehicle-possess-acto-issue-4e0dc0d71267.md |  51 +++
 ...s-out-after-rebuilding-64-issue-a6890271d414.md |   6 +
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |  42 ++
 ...e-dll-build-at-game-build-issue-9215d710021d.md |  54 +++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  57 +++
 ...active-dashboard-runtime--issue-16e2cf31162b.md |  16 +
 ...e-dll-build-at-game-build-issue-0843a94eae4b.md |  45 ++
 ...e-dll-build-at-game-build-issue-2e5c794199f1.md |  46 ++
 ...e-dll-build-at-game-build-issue-40243f99b16c.md |  32 ++
 ...e-dll-build-at-game-build-issue-f1de1fcd87a2.md |  46 ++
 ...physics-and-car-based-mod-issue-e5a086b0a441.md | 155 +++++++
 ...-live-game-journals-as-pr-issue-9b5be6c88de0.md |   8 +
 ...-resource-use-and-reasons-issue-b9edd402e658.md |  50 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  56 +++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  38 ++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |   6 +
 ...-and-build-to-proof-links-issue-b396205aaecf.md |  32 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  20 +
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |  24 +
 ...ld-cost-under-1-percent-c-issue-acf28dc22c85.md |   6 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |  40 ++
 docs/specs/gamemodes/bloodtest.md                  |  20 +
 docs/specs/gamemodes/vehicle-arena.md              | 105 ++++-
 docs/specs/vehicles/vehicles.md                    |   6 +-
 src/engine/engine-tick-camera.cpp                  |  45 +-
 src/engine/engine-tick.cpp                         |  26 +-
 src/entities/actor-body.cpp                        |  50 +-
 src/gamemode/gamemode-runtime-resolver.cpp         |  36 ++
 src/gamemode/gamemode.cpp                          |   7 +
 src/gamemode/gamemode.h                            |   3 +
 src/host/generated/packet-registry-v1.h            |   2 +-
 src/network/schema/packet-header.h                 |   4 +-
 src/network/schema/packet-vehicle.h                |   9 +-
 src/network/server-damage.cpp                      |  11 +-
 src/network/server-gamemode.cpp                    |   3 +
 src/network/vehicle-networking.cpp                 |  20 +-
 src/physics/physical-entity.cpp                    |  15 +-
 src/physics/physical-entity.h                      |  12 +
 src/render/render-player.cpp                       |  19 +-
 src/vehicles/vehicle-commands.cpp                  | 121 ++++-
 src/vehicles/vehicle-config.cpp                    |   9 +
 src/vehicles/vehicle-profile.cpp                   |   9 +
 src/vehicles/vehicle-profile.h                     |  11 +
 src/vehicles/vehicle-render.cpp                    |  12 +-
 src/vehicles/vehicle-system.cpp                    | 505 +++++++++++++++++----
 src/vehicles/vehicle-system.h                      |   4 +
 src/vehicles/vehicle-types.h                       |  13 +-
 74 files changed, 2492 insertions(+), 210 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/hot-reload/packet-ledger.json
M	config/maps/arena1.json
M	config/onlinemodes.json
M	config/vehicle-profiles/rl_soccar.json
M	config/vehicle-profiles/rl_soccar_freeplay.json
M	config/vehicles/octane_rl.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-ba939787ffa3.md
M	docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md
M	docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
M	docs/issues/bug/20261125-build-a-rocket-league-style-mode-through-staged--issue-41547751ca1c.md
M	docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md
M	docs/issues/bug/20261125-make-vehicle-freeplay-inputs-and-presentation-us-issue-60e46e12db9b.md
M	docs/issues/bug/20261125-vehicle-mode-configs-reject-vehicle-possess-acto-issue-4e0dc0d71267.md
M	docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9215d710021d.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/capability/20261121-let-ai-inspect-existing-live-game-journals-as-pr-issue-9b5be6c88de0.md
M	docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/performance/20261125-dev-loop-dashboard-should-cost-under-1-percent-c-issue-acf28dc22c85.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/specs/gamemodes/bloodtest.md
M	docs/specs/gamemodes/vehicle-arena.md
M	docs/specs/vehicles/vehicles.md
M	src/engine/engine-tick-camera.cpp
M	src/engine/engine-tick.cpp
M	src/entities/actor-body.cpp
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/host/generated/packet-registry-v1.h
M	src/network/schema/packet-header.h
M	src/network/schema/packet-vehicle.h
M	src/network/server-damage.cpp
M	src/network/server-gamemode.cpp
M	src/network/vehicle-networking.cpp
M	src/physics/physical-entity.cpp
M	src/physics/physical-entity.h
M	src/render/render-player.cpp
M	src/vehicles/vehicle-commands.cpp
M	src/vehicles/vehicle-config.cpp
M	src/vehicles/vehicle-profile.cpp
M	src/vehicles/vehicle-profile.h
M	src/vehicles/vehicle-render.cpp
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
    "latest_successful_build": "2026-11-26T01:00:35.913746Z",
    "latest_successful_build_id": "2026-11-26T01:00:35.913746Z:e18288709713b7f38e81beeb0a3201ea4f85c6c710abe2c484103119cdfec70b",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "2b7c6a6cb1951a7f4e401e5ca05a3b76f995d00eb7ffa0a6124db7306d1a6504",
    "dll": "build\\game\\generation-664\\game-g664.dll",
    "generation": 664,
    "revision": "e782f1d11a9e04e6989989956d5ce172fc62f8cd5b5ba20fa9f46c3ac96b2f2e",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-664\\game-g664.dll",
    "generation": 664,
    "published_utc": "2026-11-26T01:37:03.145Z",
    "revision": "e782f1d11a9e04e6989989956d5ce172fc62f8cd5b5ba20fa9f46c3ac96b2f2e",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 0.249,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-26T01:48:58.636Z"
  }
}
```


## Follow-up in the same session: input calibration

The car was undrivable with a physical pad: both trigger axes rested at 0, and
`mode: auto` treated a trigger as a signed -1..1 axis, so "released" decoded as
0.578 for throttle *and* brake. The two cancelled and the car could not move
(server journal showed `throttle 0.5783 brake 0.5783` for the whole run).

- `GamepadInput` now measures each trigger axis's rest value and travel when a
  device connects (`AxisCalibration`), decodes `(raw - rest) / travel`, and
  reports 0 until calibration completes, so a phantom press is impossible.
  `mode: axis01|combined|signed` remains a manual override.
- `input.gamepad.calibrated` records the rest, lo/hi and inferred convention per
  axis; `input.gamepad.state` adds the raw trigger axes and the calibration flag.
- `input.focus.changed` records game-window focus transitions, because keyboard
  and mouse are ignored while unfocused.
- Proven at generation 665, run `20261126_023207`: pad "Wireless Controller",
  trigger rest 1.5e-05, convention axis01, decoded `throttle 0.0 / brake 0.0`;
  1,218 server state samples with `max speed 0.08 m/s`, `max throttle_input 0`,
  `max brake_input 0`. Before the fix the same pad produced 0.5783/0.5783.

## Follow-up: ball harness, solo 1v1 arena, and a build unblock

- Ball test surface owned by `arena-ball`: `ball_set <x> <y> <z> [vx vy vz]`
  and `ball_status`. The request is deferred until the arena ball exists, so a
  server startup command cannot be lost to map-load ordering.
- `arena.ball.state` now samples position, velocity, linear and angular speed at
  ~10 Hz (was 1 Hz position only).
- `vehicle-arena.md` §9.0 records the ball measurement table (reference ->
  measured -> delta); measured cells are filled by the scenarios, not guessed.
- New dev launch modes: `62` ball physics probe, `64` single-client
  `vehicle_arena` (1v1 vs NPC). Mode 50 spawns two clients, so the NPC never got
  a car.
- The arena NPC now seats only **after** a human is seated; before this the bot
  claimed the blue car at server start and the joining human was left unseated
  (run 20261126_032157). Proven in run 20261126_032457: player 1 auto-seated in
  vehicle 1 at 20.05 m, NPC driver 1002 in vehicle 2.
- `vehicle.input.rejected` now records the reason (seat_mismatch / not_driver /
  server_mirror / dead) instead of silently dropping a driver packet.
- Build unblock: after the 02:51 cold cutover the server aborted with
  `missing-server-policy` because the game DLL did not export
  `GetMimitaGameServerPolicyV1`. Forcing the hot module to relink restored the
  export (generation 669) and launches work again.

## Follow-up: why you could not drive, and the ball roll model

**Input.** The game window never had OS focus (`vehicle.input.local` idle samples
showed `focused=False`), and keyboard/mouse are focus-gated by design, so WASD
could do nothing while the pad looked connected. Evidence added: a 1 Hz
unconditional local-input sample carrying decoded throttle/brake/steer,
`gamepad_connected`, raw trigger axes, `focused` and `idle`. The pad's raw axes
are 0 at rest and input packets are accepted (13 accepted, 0 rejected in run
20261126_030913). A new `focus_window` command takes focus using
AttachThreadInput so a launched game accepts keys without a manual click; proven
in run 20261126_035851 (`focused=True`).

**Virtual controller.** `gamepad_virtual 1 0 0 0 0 0 0` drives the car through the
real pad path: run 20261126_035030 shows `throttle_input 1.0` and 8.95 m/s from
nothing but that command, which isolates a device problem from a physics one.

**Ball.** The arena ball now replaces the global object damping with its authored
0.03/s (`linearDampingOverride`), because the old code added the global 0.08 on
top. Measured decay stayed ~4.0 m/s^2 (k 0.175) instead of the reference 0.03/s:
the 10 Hz samples show the ball spinning to the 6 rad/s cap while pure rolling at
25 m/s needs 27 rad/s, so it slides for the whole roll and friction eats the
speed. Filed with the numbers; the next change is the roll/friction model, not
damping.

## Follow-up: dodge 360, general surface clamp, ball roll, presentation toggles

**Dodge.** The air-control clamp was applied during flips, so 5.5 rad/s over the
0.65 s flip window capped every dodge at ~205 degrees. Flips now use their own
`flip.maxAngularVelocity` (24 rad/s) and `angularAccel` 40 rad/s^2, and the
chassis replaces the global object angular damping (1.5/s) with 0.2/s. The
measured cause of a short flip was worse than the clamp: the flip-cancel check
compared the stick against a *world* vector, so once the car had rotated ~90
degrees the check fired by itself (measured `against = -0.12` against a 0.1
threshold, `rotation_deg_total = 187`). It now compares the stick against the
direction captured in the car's own input axes at dodge start, and logs
`vehicle.flip.cancel`.

**World contact.** The floor-only penetration lift now pushes the chassis out
along any contact normal, so a car pressed into a wall or ceiling is recovered the
same way it was for the floor.

**Ball.** Friction 0.35 -> 0.03 and restitution 0.6 -> 0.45 in `arena1.json`, with
the ball already replacing the global object damping: the ball now rolls instead
of decelerating 4-6 m/s^2. Car-vs-world restitution raised to 0.25 so a high-speed
impact reflects about a quarter of the normal speed.

**Presentation.** F exits the vehicle and logs `vehicle.exit.key`.
`hud_car_assist` and `vehicle_exit_prompt` are console toggles, **both disabled by
default**, and the defaults are written at startup as `vehicle.preferences`.
Documented in `vehicle-arena.md` §15.

Verification note: the dodge proof scenario `rocket_dodge_360_v1` is written but
its run failed at setup because the client did not auto-seat in that launch, so
the 360 is implemented and instrumented but not yet proven in a journal.


## Follow-up: server died instantly at launch (0xC0000005) — fixed

`VehicleProfileRegistry::loadDirectory` published with
`mProfiles = std::move(profiles)`, destroying every live element of the map that
`VehicleSystem::profileFor` hands out pointers into. `serverTickVehicles` called
`VehicleSystem::instance().pollReload()` whenever `tick % 60 == 0`,
**and `tick 0` satisfies that**, so every server start rebuilt the registry on its
first server tick while the main thread was still loading the same singleton. The
recorded fault (`std::_Hash_bytes` reading `0x6c` in `libstdc++-6.dll`) is the
freed bucket array.

The registry now stores `std::shared_ptr<const VehicleProfile>` behind a
`std::shared_mutex`, publishes a new snapshot under the write lock, and `find`
returns a snapshot under the read lock. `profileFor`, the HUD and networking hold
that snapshot for the duration of use. The server-tick reload is kept (a
dedicated server never runs the engine config-polling path, so it is the only
reload) and is now safe. New `vehicle.profile.registry` event records revision,
count and directory on every publish.

One self-inflicted regression is recorded deliberately: the first version read
`profile.id` in the same expression that did `std::move(profile)`, collapsing all
three profiles onto the empty key (`count=1`, no profile applied, car
undrivable). Fixed by capturing the id before the move.

Proof, game generation 693: five consecutive launches (`20261126_161031`,
`161111`, `161152`, `161232`, `161313`) each show 0 occurrences of
`0xC0000005`, 0 `dev-loop.instance.exited`, registry count 3, profile applied 2,
client entered 1. Reload stress in `20261126_161313`: touching a profile file
mid-run produced registry revision 2 with count 3, zero crashes, server still
running. Full cause, fix and prevention rules are in
`docs/regressions/dev-loop/server-crash-frozen-REG-2026-09-27.md` Occurrence 4.
