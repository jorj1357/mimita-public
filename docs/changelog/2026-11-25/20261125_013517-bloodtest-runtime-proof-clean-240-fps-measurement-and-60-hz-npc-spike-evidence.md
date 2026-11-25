// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest runtime proof, clean 240 FPS measurement, and 60 Hz NPC spike evidence

# Task

- Task ID: session-20261125-013512
- Session ID: session-20261125-013512
- Owner: opencode-deepseek-v4-1
- Focus: Build evidence: generation 632 published. Runtime evidence: run 20261125_013015 (clean) and 20261125_004550 (confirmed NPC projectile hit). Client 240 FPS is met in the clean run; server 60 Hz is close but NPC spikes break 99.99%. Not proven: shooter-side hole on a remote NPC, organ chunks (damage 22 below the 0.60 force threshold), corpse shooting, and visual/human acceptance. Issues: issue-ffcc6b95dec6 (NPC 60 Hz spikes), evidence appended to issue-2e7b0cd6f868 (aim/damage/hole) and issue-16500baeb0cc (FPS).
- Summary: Validated the Bloodtest bodylayer, gore, and projectile-hit paths on generation 632 and measured the clean-environment frame/tick behavior. In an isolated run with no competing game pair or build (20261125_013015), the client held 267-312 FPS with p50 2.9-3.4 ms, p95 4.6-5.6 ms, and p99 5.9-6.7 ms, meeting the 4.167 ms 240 FPS budget in steady windows. The server averaged 60.08 ticks/window overall and 60.73 in the last 40 windows, but NPC simulation spikes of up to 102 ms caused 5/40 steady windows to exceed 16.67 ms, so 60 Hz is not yet 99.99%. A logged player AK projectile on NPC-1000 produced combat.hit.resolved (rightLeg, 3.47 m, damage 22) and client gore, proving projectile body-part classification and confirmed gore, but the shooter-side path still did not emit bloodtest.body.hole for the remote NPC and organ_chunks stayed 0. The new spawn diagnostic confirms definition_id=bloodtest_ak47 and configured/definition speed 520 m/s, so the old speed_mps=3 field is mislabeled, not a slow projectile. Fixed the vehicle-camera glm::remainder compile error with std::remainder and forced a fresh compile of the stale engine-tick-net object; regenerated the packet ledger and freed disk space to publish generation 632.
- Status: built; clean 240 FPS measured, 60 Hz NPC spikes recorded, projectile body-part/gore proven, hole/organ/corpse proof still blocked
- Date, time, timezone: 2026-11-25T01:35:16+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Final commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a

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
| `config/gamemodes/vehicle_arena.json` |  |  |  | git checkout -- config/gamemodes/vehicle_arena.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/onlinemodes.json` |  |  |  | git checkout -- config/onlinemodes.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` |  |  |  | git checkout -- docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md` |  |  |  | git checkout -- docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md |
| `docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md` |  |  |  | git checkout -- docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md |
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` |  |  |  | git checkout -- docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md |
| `docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md` |  |  |  | git checkout -- docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md |
| `docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md` |  |  |  | git checkout -- docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md |
| `docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md` |  |  |  | git checkout -- docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-739845c7624a.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-739845c7624a.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md |
| `docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md` |  |  |  | git checkout -- docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md |
| `docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md` |  |  |  | git checkout -- docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md |
| `src/engine/engine-tick-camera.cpp` |  |  |  | git checkout -- src/engine/engine-tick-camera.cpp |
| `src/engine/engine-tick-combat.cpp` |  |  |  | git checkout -- src/engine/engine-tick-combat.cpp |
| `src/engine/engine-tick-net.cpp` |  |  |  | git checkout -- src/engine/engine-tick-net.cpp |
| `src/entities/player-render.cpp` |  |  |  | git checkout -- src/entities/player-render.cpp |
| `src/entities/player.h` |  |  |  | git checkout -- src/entities/player.h |
| `src/gamemode/gamemode.cpp` |  |  |  | git checkout -- src/gamemode/gamemode.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `src/network/multiplayer-physical-entities.cpp` |  |  |  | git checkout -- src/network/multiplayer-physical-entities.cpp |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/network/vehicle-networking.cpp` |  |  |  | git checkout -- src/network/vehicle-networking.cpp |
| `src/physics/physical-entity.cpp` |  |  |  | git checkout -- src/physics/physical-entity.cpp |
| `src/physics/physical-entity.h` |  |  |  | git checkout -- src/physics/physical-entity.h |
| `src/sim/simulate-tick.cpp` |  |  |  | git checkout -- src/sim/simulate-tick.cpp |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `src/vehicles/vehicle-types.h` |  |  |  | git checkout -- src/vehicles/vehicle-types.h |
| `config/cameras/` |  |  |  | Remove-Item -LiteralPath 'config/cameras/' |
| `config/gamemodes/vehicle_freeplay.json` |  |  |  | Remove-Item -LiteralPath 'config/gamemodes/vehicle_freeplay.json' |
| `config/netcode/` |  |  |  | Remove-Item -LiteralPath 'config/netcode/' |
| `config/vehicle-profiles/rl_soccar_freeplay.json` |  |  |  | Remove-Item -LiteralPath 'config/vehicle-profiles/rl_soccar_freeplay.json' |
| `devscripts/quality/test_game_build_fast_diagnostic_abort.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_game_build_fast_diagnostic_abort.py' |
| `docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md' |
| `docs/changelog/2026-11-24/20261124_2230-rl-code-pass-freeplay-npc-camera.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2230-rl-code-pass-freeplay-npc-camera.md' |
| `docs/changelog/2026-11-24/20261124_2248-fullscreen6-ui-stretch-evidence-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2248-fullscreen6-ui-stretch-evidence-review.md' |
| `docs/changelog/2026-11-24/20261124_224925-bodycam-config-limits-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_224925-bodycam-config-limits-evidence.md' |
| `docs/changelog/2026-11-24/20261124_2250-rl-code-pass-2-prediction-possession-ccd.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2250-rl-code-pass-2-prediction-possession-ccd.md' |
| `docs/changelog/2026-11-24/20261124_225219-bloodtest-choppiness-evidence-review-issue-16500baeb0cc.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_225219-bloodtest-choppiness-evidence-review-issue-16500baeb0cc.md' |
| `docs/changelog/2026-11-24/20261124_225352-issue-ee3e906c8273-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_225352-issue-ee3e906c8273-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_2254-gen627-cpu-resource-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2254-gen627-cpu-resource-evidence.md' |
| `docs/changelog/2026-11-24/20261124_2255-issue-a2de003a1900-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2255-issue-a2de003a1900-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_2255-issue-b9edd402e658-resource-context-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2255-issue-b9edd402e658-resource-context-review.md' |
| `docs/changelog/2026-11-24/20261124_2300-build-wait-reviewer-resource-gaps-and-context-ack.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2300-build-wait-reviewer-resource-gaps-and-context-ack.md' |
| `docs/changelog/2026-11-24/20261124_2300-issue-739845c7624a-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2300-issue-739845c7624a-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_2310-gen627-live-npc-phase-profile.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2310-gen627-live-npc-phase-profile.md' |
| `docs/changelog/2026-11-24/20261124_2310-resource-accountability-live-proof.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2310-resource-accountability-live-proof.md' |
| `docs/changelog/2026-11-24/20261124_2310-rl-code-pass-3-weapons-jump-npc-prediction.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2310-rl-code-pass-3-weapons-jump-npc-prediction.md' |
| `docs/changelog/2026-11-24/20261124_2313-issue-3c1894ca50ec-opaque-black-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2313-issue-3c1894ca50ec-opaque-black-review.md' |
| `docs/changelog/2026-11-24/20261124_2320-issue-0567fb769b04-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_2320-issue-0567fb769b04-build-wait-review.md' |
| `docs/changelog/2026-11-25/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/' |
| `docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-e3eb52067a38.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-e3eb52067a38.md' |
| `docs/issues/build-friction/20261125-game-dll-build-fails-compile-glm-remainder-is-no-issue-fb9f3f56f1f1.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-game-dll-build-fails-compile-glm-remainder-is-no-issue-fb9f3f56f1f1.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-a36ab17255e9.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-a36ab17255e9.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-e6c253b2fa9f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-e6c253b2fa9f.md' |
| `docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md' |

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
- Focus: Build evidence: generation 632 published. Runtime evidence: run 20261125_013015 (clean) and 20261125_004550 (confirmed NPC projectile hit). Client 240 FPS is met in the clean run; server 60 Hz is close but NPC spikes break 99.99%. Not proven: shooter-side hole on a remote NPC, organ chunks (damage 22 below the 0.60 force threshold), corpse shooting, and visual/human acceptance. Issues: issue-ffcc6b95dec6 (NPC 60 Hz spikes), evidence appended to issue-2e7b0cd6f868 (aim/damage/hole) and issue-16500baeb0cc (FPS).
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / eb92b02b21e62fd5ec496eee4dcad832f901bb7a / eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-25T01:21:28.987918Z", "latest_successful_build_id": "2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b", "latest_successful_source_generation": 58}, "game_current": {"build_mode": "debug", "code_hash": "309ad58da6a8b8fc01c066792a76d75d25a9f312820c777c504eba8d7c0e878a", "dll": "build\\game\\generation-632\\game-g632.dll", "generation": 632, "revision": "f605d6dfbd4d8759fd10e63ff97dc49f417eb09d4d63c2fa4847f63a9ceb0d39", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-632\\game-g632.dll", "generation": 632, "published_utc": "2026-11-25T01:29:46.686Z", "revision": "f605d6dfbd4d8759fd10e63ff97dc49f417eb09d4d63c2fa4847f63a9ceb0d39", "status": "ready"}, "progress": {"compiled": 2, "elapsed_s": 13.827, "phase": "build_complete", "result": "success", "skipped": 85, "updated_utc": "2026-11-25T01:21:26.486Z"}}
- Generation: 632
- Artifact: build\game\generation-632\game-g632.dll
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_013233\dev-loop\events.jsonl
- Session ID: session-20261125-013512

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
 M config/gamemodes/vehicle_arena.json
 M config/hot-reload/packet-ledger.json
 M config/onlinemodes.json
 m devscripts/blender-ai
 M devscripts/game_build_worker.py
 M docs/issues/README.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md
 M docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md
 M docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
 M docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
 M docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md
 M docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-739845c7624a.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md
 M docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
 M docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
 M src/engine/engine-tick-camera.cpp
 M src/engine/engine-tick-combat.cpp
 M src/engine/engine-tick-net.cpp
 M src/entities/player-render.cpp
 M src/entities/player.h
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/network/multiplayer-physical-entities.cpp
 M src/network/server-projectiles.cpp
 M src/network/vehicle-networking.cpp
 M src/physics/physical-entity.cpp
 M src/physics/physical-entity.h
 M src/sim/simulate-tick.cpp
 M src/vehicles/vehicle-system.cpp
 M src/vehicles/vehicle-types.h
?? config/cameras/
?? config/gamemodes/vehicle_freeplay.json
?? config/netcode/
?? config/vehicle-profiles/rl_soccar_freeplay.json
?? devscripts/quality/test_game_build_fast_diagnostic_abort.py
?? docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md
?? docs/changelog/2026-11-24/20261124_2230-rl-code-pass-freeplay-npc-camera.md
?? docs/changelog/2026-11-24/20261124_2248-fullscreen6-ui-stretch-evidence-review.md
?? docs/changelog/2026-11-24/20261124_224925-bodycam-config-limits-evidence.md
?? docs/changelog/2026-11-24/20261124_2250-rl-code-pass-2-prediction-possession-ccd.md
?? docs/changelog/2026-11-24/20261124_225219-bloodtest-choppiness-evidence-review-issue-16500baeb0cc.md
?? docs/changelog/2026-11-24/20261124_225352-issue-ee3e906c8273-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_2254-gen627-cpu-resource-evidence.md
?? docs/changelog/2026-11-24/20261124_2255-issue-a2de003a1900-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_2255-issue-b9edd402e658-resource-context-review.md
?? docs/changelog/2026-11-24/20261124_2300-build-wait-reviewer-resource-gaps-and-context-ack.md
?? docs/changelog/2026-11-24/20261124_2300-issue-739845c7624a-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_2310-gen627-live-npc-phase-profile.md
?? docs/changelog/2026-11-24/20261124_2310-resource-accountability-live-proof.md
?? docs/changelog/2026-11-24/20261124_2310-rl-code-pass-3-weapons-jump-npc-prediction.md
?? docs/changelog/2026-11-24/20261124_2313-issue-3c1894ca50ec-opaque-black-review.md
?? docs/changelog/2026-11-24/20261124_2320-issue-0567fb769b04-build-wait-review.md
?? docs/changelog/2026-11-25/
?? docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-e3eb52067a38.md
?? docs/issues/build-friction/20261125-game-dll-build-fails-compile-glm-remainder-is-no-issue-fb9f3f56f1f1.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-a36ab17255e9.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-e6c253b2fa9f.md
?? docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md
```

## Captured diff stat

```text
config/analytics.json                              |   2 +-
 config/gamemodes/vehicle_arena.json                |   5 +-
 config/hot-reload/packet-ledger.json               |   6 +-
 config/onlinemodes.json                            |   8 +
 devscripts/blender-ai                              |   0
 devscripts/game_build_worker.py                    |  74 ++++-
 docs/issues/README.md                              |  41 +--
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   6 +
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |  16 ++
 ...cs-fall-through-the-world-issue-1d1b9c62ac08.md |  43 +++
 ...collidable-in-arcade-mode-issue-87d8c9c7ffb1.md |  37 +++
 ...on-marks-float-away-from--issue-8e48cd0bc786.md |  46 +++
 ...nt-choppiness-against-the-issue-16500baeb0cc.md | 148 ++++++++++
 ...aces-must-not-become-pass-issue-3c1894ca50ec.md |  32 +++
 ...e-6-full-frame-stretch-th-issue-e0a6bcbacad0.md |  54 ++++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  38 +++
 ...e-dll-build-at-game-build-issue-739845c7624a.md |  32 +++
 ...e-dll-build-at-game-build-issue-a2de003a1900.md |  38 +++
 ...e-dll-build-at-game-build-issue-ee3e906c8273.md |  32 +++
 ...e-dll-build-at-game-build-issue-fdf3fb2f88e2.md |  36 +++
 ...-resource-use-and-reasons-issue-b9edd402e658.md |  79 +++++
 ...guration-limits-with-warn-issue-9e73745d3154.md |  73 +++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  32 +++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  30 ++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  16 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 185 ++++++++++++
 ...y-dev-loop-gui-button-and-issue-7ee67a85a40c.md |   6 +
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 320 +++++++++++++++++++++
 ...gh-inclusion-headers-npc--issue-196c3045b4cb.md |  27 ++
 src/engine/engine-tick-camera.cpp                  | 188 ++++++++++--
 src/engine/engine-tick-combat.cpp                  |   6 +-
 src/engine/engine-tick-net.cpp                     |   1 +
 src/entities/player-render.cpp                     |   4 +
 src/entities/player.h                              |   3 +
 src/gamemode/gamemode.cpp                          |   2 +
 src/gamemode/gamemode.h                            |   3 +
 src/network/multiplayer-physical-entities.cpp      | 111 ++++++-
 src/network/server-projectiles.cpp                 |   3 +
 src/network/vehicle-networking.cpp                 | 103 ++++++-
 src/physics/physical-entity.cpp                    |  93 +++++-
 src/physics/physical-entity.h                      |   7 +
 src/sim/simulate-tick.cpp                          |   6 +
 src/vehicles/vehicle-system.cpp                    |  72 ++++-
 src/vehicles/vehicle-types.h                       |   7 +
 44 files changed, 1989 insertions(+), 82 deletions(-)
```

## Captured changed paths

```text
M	config/analytics.json
M	config/gamemodes/vehicle_arena.json
M	config/hot-reload/packet-ledger.json
M	config/onlinemodes.json
M	devscripts/blender-ai
M	devscripts/game_build_worker.py
M	docs/issues/README.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md
M	docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md
M	docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
M	docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
M	docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md
M	docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-739845c7624a.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-a2de003a1900.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-fdf3fb2f88e2.md
M	docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
M	docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
M	src/engine/engine-tick-camera.cpp
M	src/engine/engine-tick-combat.cpp
M	src/engine/engine-tick-net.cpp
M	src/entities/player-render.cpp
M	src/entities/player.h
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/network/multiplayer-physical-entities.cpp
M	src/network/server-projectiles.cpp
M	src/network/vehicle-networking.cpp
M	src/physics/physical-entity.cpp
M	src/physics/physical-entity.h
M	src/sim/simulate-tick.cpp
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
    "latest_successful_build": "2026-11-25T01:21:28.987918Z",
    "latest_successful_build_id": "2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b",
    "latest_successful_source_generation": 58
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "309ad58da6a8b8fc01c066792a76d75d25a9f312820c777c504eba8d7c0e878a",
    "dll": "build\\game\\generation-632\\game-g632.dll",
    "generation": 632,
    "revision": "f605d6dfbd4d8759fd10e63ff97dc49f417eb09d4d63c2fa4847f63a9ceb0d39",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-632\\game-g632.dll",
    "generation": 632,
    "published_utc": "2026-11-25T01:29:46.686Z",
    "revision": "f605d6dfbd4d8759fd10e63ff97dc49f417eb09d4d63c2fa4847f63a9ceb0d39",
    "status": "ready"
  },
  "progress": {
    "compiled": 2,
    "elapsed_s": 13.827,
    "phase": "build_complete",
    "result": "success",
    "skipped": 85,
    "updated_utc": "2026-11-25T01:21:26.486Z"
  }
}
```
