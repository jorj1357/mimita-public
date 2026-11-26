// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Rocket League mode: rotation on the wire, single input owner, ball cam toggle, launch gate fix

# Task

- Task ID: session-20261125-233838
- Session ID: session-20261125-233838
- Owner: rocket-league-input
- Focus: vehicle input, networking wire format, ball cam, dev-loop launch readiness
- Summary: The networked car could never pitch, yaw, roll, dodge, or stall because VehicleInputPacket carried no rotational axes; the authoritative server therefore simulated a car that only drove on the ground while the client rendered its mirror pose. VehicleInputPacket now carries normalized pitch/yaw/roll (protocol 53), the vehicle input path reads only the profile bindings plus the pad device mapping (space toggles ball cam, right mouse jumps, left mouse boosts, Shift is powerslide/air-roll), and ball cam is polled as local presentation state so it works while the client owns a server mirror. Added vehicle.ballcam evidence, vehicle_input rotation/jump/boost args, an id 'local' alias, three rocket_* scenarios, and the vehicle-arena dream + milestone ladder. Also appended vehicle_freeplay to onlinemodes.json so modestart numbering keeps its old meaning, and stopped requiring a match-phase start for the endless free-play launch mode. Proven on game generation 656: run 20261125_233342 (jump + air roll, air_control in 184 samples, angular speed capped at 5.5, up_dot min -1.0), run 20261125_233039 (server input pitch 0.9 / yaw 0.8 / roll 0.9), run 20261125_233213 (vehicle.ballcam on/off/on). File issue-b5a35be7b0e0 for the launch gate and a bug issue for the trigger axes decoding to ~0.5 at rest.
- Status: complete
- Date, time, timezone: 2026-11-25T23:38:40+00:00
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
| `config/onlinemodes.json` |  |  |  | git checkout -- config/onlinemodes.json |
| `config/vehicle-profiles/rl_soccar.json` |  |  |  | git checkout -- config/vehicle-profiles/rl_soccar.json |
| `config/vehicle-profiles/rl_soccar_freeplay.json` |  |  |  | git checkout -- config/vehicle-profiles/rl_soccar_freeplay.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-launch-modes.json` |  |  |  | git checkout -- devscripts/dev-launch-modes.json |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` |  |  |  | git checkout -- docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md |
| `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md` |  |  |  | git checkout -- docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md` |  |  |  | git checkout -- docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md |
| `docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md |
| `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md` |  |  |  | git checkout -- docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md |
| `docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md` |  |  |  | git checkout -- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md |
| `docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md` |  |  |  | git checkout -- docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md |
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
| `src/render/render-player.cpp` |  |  |  | git checkout -- src/render/render-player.cpp |
| `src/vehicles/vehicle-commands.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-commands.cpp |
| `src/vehicles/vehicle-profile.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-profile.cpp |
| `src/vehicles/vehicle-profile.h` |  |  |  | git checkout -- src/vehicles/vehicle-profile.h |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `src/vehicles/vehicle-system.h` |  |  |  | git checkout -- src/vehicles/vehicle-system.h |
| `src/vehicles/vehicle-types.h` |  |  |  | git checkout -- src/vehicles/vehicle-types.h |
| `devscripts/scenarios/rocket_ballcam_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_ballcam_v1.json' |
| `devscripts/scenarios/rocket_input_rotation_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_input_rotation_v1.json' |
| `devscripts/scenarios/rocket_jump_airroll_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/rocket_jump_airroll_v1.json' |
| `docs/changelog/2026-11-25/20261125-bloodtest-mode6-gore2-camera.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125-bloodtest-mode6-gore2-camera.md' |
| `docs/changelog/2026-11-25/20261125-dashboard-cpu-process-purpose.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125-dashboard-cpu-process-purpose.md' |
| `docs/changelog/2026-11-25/20261125_2133-independent-build-wait-review-issue-f1de1fcd87a2.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2133-independent-build-wait-review-issue-f1de1fcd87a2.md' |
| `docs/changelog/2026-11-25/20261125_2146-independent-issue-review-issue-55bd775c8026.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2146-independent-issue-review-issue-55bd775c8026.md' |
| `docs/changelog/2026-11-25/20261125_220310-build-wait-review-issue-0843a94eae4b.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_220310-build-wait-review-issue-0843a94eae4b.md' |
| `docs/changelog/2026-11-25/20261125_2205-build-wait-review-issue-63ec094b8859.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2205-build-wait-review-issue-63ec094b8859.md' |
| `docs/changelog/2026-11-25/20261125_2233-build-wait-review-issue-bb51d96a1200.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_2233-build-wait-review-issue-bb51d96a1200.md' |
| `docs/handoffs/20261125_rocket-league-mode-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261125_rocket-league-mode-handoff.md' |
| `docs/issues/bug/20261125-dualshock-trigger-axes-at-rest-decode-to-0-5-thr-issue-4b39c4193cd7.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261125-dualshock-trigger-axes-at-rest-decode-to-0-5-thr-issue-4b39c4193cd7.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-4ab3b491ad6c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-4ab3b491ad6c.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-63ec094b8859.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-63ec094b8859.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8b7208f291a8.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8b7208f291a8.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-bb51d96a1200.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-bb51d96a1200.md' |
| `docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md' |
| `docs/issues/launch-friction/20261125-vehicle-free-play-launch-always-reports-failure--issue-b5a35be7b0e0.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/launch-friction/20261125-vehicle-free-play-launch-always-reports-failure--issue-b5a35be7b0e0.md' |
| `docs/issues/tech-debt/20261125-arcade-arena-vehicle-profile-has-id-n-and-is-ref-issue-0c550dec9353.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/tech-debt/20261125-arcade-arena-vehicle-profile-has-id-n-and-is-ref-issue-0c550dec9353.md' |

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

- Owner: rocket-league-input
- Focus: vehicle input, networking wire format, ball cam, dev-loop launch readiness
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / f9d94c5bb3cffd8bb15899cdda30eea3264c4a66 / f9d94c5bb3cffd8bb15899cdda30eea3264c4a66
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-25T23:04:22.955793Z", "latest_successful_build_id": "2026-11-25T23:04:22.955793Z:a646bdd4a20edbb0756d961271bf4aeb31c451fce0ba663a7ce15d64413657c0", "latest_successful_source_generation": 25}, "game_current": {"build_mode": "debug", "code_hash": "720c2ae3ca5aff1bb699730bb6a879615f209ccadfb9b811ed6551567f36282c", "dll": "build\\game\\generation-656\\game-g656.dll", "generation": 656, "revision": "7bd11b30ddde25c78e573c55a81e2f6000e53e5684857c4fe6ea163ece8b5de6", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-656\\game-g656.dll", "generation": 656, "published_utc": "2026-11-25T23:28:40.589Z", "revision": "7bd11b30ddde25c78e573c55a81e2f6000e53e5684857c4fe6ea163ece8b5de6", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 3.633, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-25T23:37:47.408Z"}}
- Generation: 656
- Artifact: build\game\generation-656\game-g656.dll
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_233748\server-01\events.jsonl
- Session ID: session-20261125-233838

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
 M config/onlinemodes.json
 M config/vehicle-profiles/rl_soccar.json
 M config/vehicle-profiles/rl_soccar_freeplay.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md
 M docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md
 M docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
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
 M src/render/render-player.cpp
 M src/vehicles/vehicle-commands.cpp
 M src/vehicles/vehicle-profile.cpp
 M src/vehicles/vehicle-profile.h
 M src/vehicles/vehicle-system.cpp
 M src/vehicles/vehicle-system.h
 M src/vehicles/vehicle-types.h
?? devscripts/scenarios/rocket_ballcam_v1.json
?? devscripts/scenarios/rocket_input_rotation_v1.json
?? devscripts/scenarios/rocket_jump_airroll_v1.json
?? docs/changelog/2026-11-25/20261125-bloodtest-mode6-gore2-camera.md
?? docs/changelog/2026-11-25/20261125-dashboard-cpu-process-purpose.md
?? docs/changelog/2026-11-25/20261125_2133-independent-build-wait-review-issue-f1de1fcd87a2.md
?? docs/changelog/2026-11-25/20261125_2146-independent-issue-review-issue-55bd775c8026.md
?? docs/changelog/2026-11-25/20261125_220310-build-wait-review-issue-0843a94eae4b.md
?? docs/changelog/2026-11-25/20261125_2205-build-wait-review-issue-63ec094b8859.md
?? docs/changelog/2026-11-25/20261125_2233-build-wait-review-issue-bb51d96a1200.md
?? docs/handoffs/20261125_rocket-league-mode-handoff.md
?? docs/issues/bug/20261125-dualshock-trigger-axes-at-rest-decode-to-0-5-thr-issue-4b39c4193cd7.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-4ab3b491ad6c.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-63ec094b8859.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8b7208f291a8.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-bb51d96a1200.md
?? docs/issues/improvement/20261125-bloodtest-gore-mode-2-needs-a-deterministic-high-issue-235e26db08aa.md
?? docs/issues/launch-friction/20261125-vehicle-free-play-launch-always-reports-failure--issue-b5a35be7b0e0.md
?? docs/issues/tech-debt/20261125-arcade-arena-vehicle-profile-has-id-n-and-is-ref-issue-0c550dec9353.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/gamemodes/bloodtest.json                    |   1 +
 .../gamemodes/masterlist/gamemode-masterlist.json  |   7 +
 config/hot-reload/packet-ledger.json               |   8 +-
 config/onlinemodes.json                            |  18 +-
 config/vehicle-profiles/rl_soccar.json             |  16 +-
 config/vehicle-profiles/rl_soccar_freeplay.json    |  13 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-launch-modes.json                   |  13 ++
 devscripts/dev_loop/game_build.py                  |   2 +-
 devscripts/dev_loop/gui.py                         |  80 ++++++++-
 docs/issues/README.md                              |  44 +++--
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |  36 ++++
 ...lision-and-mace-active-po-issue-503f3e410c01.md |  68 ++++++++
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |  52 ++++++
 ...0-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md |  54 ++++++
 ...s-out-after-rebuilding-64-issue-a6890271d414.md |   6 +
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |  32 ++++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  31 ++++
 ...active-dashboard-runtime--issue-16e2cf31162b.md |  16 ++
 ...e-dll-build-at-game-build-issue-0843a94eae4b.md |  45 +++++
 ...e-dll-build-at-game-build-issue-2e5c794199f1.md |  46 +++++
 ...e-dll-build-at-game-build-issue-40243f99b16c.md |  32 ++++
 ...e-dll-build-at-game-build-issue-f1de1fcd87a2.md |  46 +++++
 ...physics-and-car-based-mod-issue-e5a086b0a441.md |  48 ++++++
 ...-resource-use-and-reasons-issue-b9edd402e658.md |  50 ++++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  14 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  30 ++++
 ...-and-build-to-proof-links-issue-b396205aaecf.md |  20 +++
 ...ld-cost-under-1-percent-c-issue-acf28dc22c85.md |   6 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |  18 ++
 docs/specs/gamemodes/bloodtest.md                  |  20 +++
 docs/specs/gamemodes/vehicle-arena.md              |  49 +++++-
 docs/specs/vehicles/vehicles.md                    |   6 +-
 src/engine/engine-tick-camera.cpp                  |  45 ++++-
 src/engine/engine-tick.cpp                         |  26 ++-
 src/entities/actor-body.cpp                        |  50 +++++-
 src/gamemode/gamemode-runtime-resolver.cpp         |  36 ++++
 src/gamemode/gamemode.cpp                          |   7 +
 src/gamemode/gamemode.h                            |   3 +
 src/host/generated/packet-registry-v1.h            |   2 +-
 src/network/schema/packet-header.h                 |   4 +-
 src/network/schema/packet-vehicle.h                |   9 +-
 src/network/server-damage.cpp                      |  11 +-
 src/network/server-gamemode.cpp                    |   3 +
 src/network/vehicle-networking.cpp                 |  20 ++-
 src/render/render-player.cpp                       |  19 ++-
 src/vehicles/vehicle-commands.cpp                  |  98 +++++++++--
 src/vehicles/vehicle-profile.cpp                   |   2 +
 src/vehicles/vehicle-profile.h                     |   3 +
 src/vehicles/vehicle-system.cpp                    | 189 +++++++++++++++------
 src/vehicles/vehicle-system.h                      |   4 +
 src/vehicles/vehicle-types.h                       |   1 -
 56 files changed, 1336 insertions(+), 135 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/hot-reload/packet-ledger.json
M	config/onlinemodes.json
M	config/vehicle-profiles/rl_soccar.json
M	config/vehicle-profiles/rl_soccar_freeplay.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md
M	docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-0843a94eae4b.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md
M	docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
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
M	src/render/render-player.cpp
M	src/vehicles/vehicle-commands.cpp
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
    "latest_successful_build": "2026-11-25T23:04:22.955793Z",
    "latest_successful_build_id": "2026-11-25T23:04:22.955793Z:a646bdd4a20edbb0756d961271bf4aeb31c451fce0ba663a7ce15d64413657c0",
    "latest_successful_source_generation": 25
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "720c2ae3ca5aff1bb699730bb6a879615f209ccadfb9b811ed6551567f36282c",
    "dll": "build\\game\\generation-656\\game-g656.dll",
    "generation": 656,
    "revision": "7bd11b30ddde25c78e573c55a81e2f6000e53e5684857c4fe6ea163ece8b5de6",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-656\\game-g656.dll",
    "generation": 656,
    "published_utc": "2026-11-25T23:28:40.589Z",
    "revision": "7bd11b30ddde25c78e573c55a81e2f6000e53e5684857c4fe6ea163ece8b5de6",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 3.633,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-25T23:37:47.408Z"
  }
}
```


