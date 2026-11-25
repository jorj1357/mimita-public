// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# NPC post-kill hostility link

# Task

- Task ID: session-caefbf1a219e
- Session ID: session-caefbf1a219e
- Owner: tdm-npc-postkill-followup
- Focus: User context ctx-0b5a1ae6738f linked primarily to issue-bbd7b6567300 and related to issue-cd492e4faa08. Appended follow-up notes to both existing issue records; no source or runtime change.
- Summary: Linked the user's new recollection that NPCs stop treating the player as an enemy after player kills to the TDM targeting investigation and the older FCS post-kill targeting issue, preserving the mode/run uncertainty.
- Status: related post-kill clue recorded; mode unknown; runtime confirmation pending
- Date, time, timezone: 2026-11-25T21:13:12+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: c7fd30cf16d92e088881977159f161face60a176
- Final commit: c7fd30cf16d92e088881977159f161face60a176

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
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/animations.json` |  |  |  | git checkout -- config/animations.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/camconfig.json` |  |  |  | git checkout -- config/camconfig.json |
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/input/gamepad.json` |  |  |  | git checkout -- config/input/gamepad.json |
| `config/vehicles/octane_rl.json` |  |  |  | git checkout -- config/vehicles/octane_rl.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` |  |  |  | git checkout -- docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md |
| `docs/issues/bug/20261122-fun-counter-strike-npcs-stop-targeting-player-af-issue-cd492e4faa08.md` |  |  |  | git checkout -- docs/issues/bug/20261122-fun-counter-strike-npcs-stop-targeting-player-af-issue-cd492e4faa08.md |
| `docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md` |  |  |  | git checkout -- docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md |
| `docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md` |  |  |  | git checkout -- docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md |
| `docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md` |  |  |  | git checkout -- docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md |
| `docs/specs/gamemodes/bloodtest.md` |  |  |  | git checkout -- docs/specs/gamemodes/bloodtest.md |
| `src/camera.cpp` |  |  |  | git checkout -- src/camera.cpp |
| `src/combat/weapon-viewmodel.cpp` |  |  |  | git checkout -- src/combat/weapon-viewmodel.cpp |
| `src/config/camera-config.cpp` |  |  |  | git checkout -- src/config/camera-config.cpp |
| `src/config/camera-config.h` |  |  |  | git checkout -- src/config/camera-config.h |
| `src/engine/engine-tick.cpp` |  |  |  | git checkout -- src/engine/engine-tick.cpp |
| `src/entities/player-render.cpp` |  |  |  | git checkout -- src/entities/player-render.cpp |
| `src/gamemode/gamemode-runtime-resolver.cpp` |  |  |  | git checkout -- src/gamemode/gamemode-runtime-resolver.cpp |
| `src/gamemode/gamemode.cpp` |  |  |  | git checkout -- src/gamemode/gamemode.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `src/input/gamepad-input.cpp` |  |  |  | git checkout -- src/input/gamepad-input.cpp |
| `src/input/gamepad-input.h` |  |  |  | git checkout -- src/input/gamepad-input.h |
| `src/render/render-player.cpp` |  |  |  | git checkout -- src/render/render-player.cpp |
| `src/render/render-player.h` |  |  |  | git checkout -- src/render/render-player.h |
| `src/render/render-world-mesh.cpp` |  |  |  | git checkout -- src/render/render-world-mesh.cpp |
| `src/render/render-world.cpp` |  |  |  | git checkout -- src/render/render-world.cpp |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `docs/changelog/2026-11-25/20261125_1933-issue-0cdac76fb006-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_1933-issue-0cdac76fb006-independent-review.md' |
| `docs/changelog/2026-11-25/20261125_204559-tdm-npc-target-and-stairs-issue-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_204559-tdm-npc-target-and-stairs-issue-intake.md' |
| `docs/changelog/2026-11-25/20261125_210339-tdm-npc-target-selection-diagnosis.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_210339-tdm-npc-target-selection-diagnosis.md' |
| `docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md' |
| `docs/issues/bug/20261125-mimita-client-burns-20-33-percent-cpu-while-mini-issue-a6be56f9d9fe.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261125-mimita-client-burns-20-33-percent-cpu-while-mini-issue-a6be56f9d9fe.md' |
| `docs/issues/bug/20261125-tdm-npcs-get-stuck-below-stairs-and-fail-to-reco-issue-d9e93ee0aa8a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261125-tdm-npcs-get-stuck-below-stairs-and-fail-to-reco-issue-d9e93ee0aa8a.md' |
| `docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md' |
| `docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-5b6649db44cc.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-5b6649db44cc.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-883fd502727e.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-883fd502727e.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md' |
| `docs/issues/improvement/20261125-allow-and-encourage-agent-coordination-while-ope-issue-949d201156b4.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261125-allow-and-encourage-agent-coordination-while-ope-issue-949d201156b4.md' |
| `docs/issues/performance/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/performance/' |

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

- Owner: tdm-npc-postkill-followup
- Focus: User context ctx-0b5a1ae6738f linked primarily to issue-bbd7b6567300 and related to issue-cd492e4faa08. Appended follow-up notes to both existing issue records; no source or runtime change.
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / c7fd30cf16d92e088881977159f161face60a176 / c7fd30cf16d92e088881977159f161face60a176
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-25T20:35:21.601166Z", "latest_successful_build_id": "2026-11-25T20:35:21.601166Z:ff20ae8525ee2afc5727a0ca055796369445e697123df0bef8bb4d5b46422764", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "e70fd210a24291b2b1d513981bd91d60603203db088b4152232cd0ed7b998143", "dll": "build\\game\\generation-647\\game-g647.dll", "generation": 647, "revision": "1b99644e686cf2fcc7b3c044a206339a3a0be64abbfcebc79a8cc78f9f1e9bce", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-647\\game-g647.dll", "generation": 647, "published_utc": "2026-11-25T21:10:18.734Z", "revision": "1b99644e686cf2fcc7b3c044a206339a3a0be64abbfcebc79a8cc78f9f1e9bce", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.364, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-25T20:35:43.133Z"}}
- Generation: 647
- Artifact: build\game\generation-647\game-g647.dll
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_211048\client-01\events.jsonl
- Session ID: session-caefbf1a219e

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
M AGENTS.md
 M config/accounts/default.json
 M config/analytics.json
 M config/animations.json
 M config/audio/music-settings.json
 M config/camconfig.json
 M config/gamemodes/bloodtest.json
 M config/input/gamepad.json
 M config/vehicles/octane_rl.json
 M config/weapons/bloodtest_ak47/config.json
 m devscripts/blender-ai
 M docs/issues/README.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261122-fun-counter-strike-npcs-stop-targeting-player-af-issue-cd492e4faa08.md
 M docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
 M docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
 M docs/specs/gamemodes/bloodtest.md
 M src/camera.cpp
 M src/combat/weapon-viewmodel.cpp
 M src/config/camera-config.cpp
 M src/config/camera-config.h
 M src/engine/engine-tick.cpp
 M src/entities/player-render.cpp
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/input/gamepad-input.cpp
 M src/input/gamepad-input.h
 M src/render/render-player.cpp
 M src/render/render-player.h
 M src/render/render-world-mesh.cpp
 M src/render/render-world.cpp
 M src/vehicles/vehicle-system.cpp
?? docs/changelog/2026-11-25/20261125_1933-issue-0cdac76fb006-independent-review.md
?? docs/changelog/2026-11-25/20261125_204559-tdm-npc-target-and-stairs-issue-intake.md
?? docs/changelog/2026-11-25/20261125_210339-tdm-npc-target-selection-diagnosis.md
?? docs/issues/bug/20261125-calibrate-world-units-10-m-cube-hud-20-m-s-and-3-issue-27de15e2dbe2.md
?? docs/issues/bug/20261125-mimita-client-burns-20-33-percent-cpu-while-mini-issue-a6be56f9d9fe.md
?? docs/issues/bug/20261125-tdm-npcs-get-stuck-below-stairs-and-fail-to-reco-issue-d9e93ee0aa8a.md
?? docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md
?? docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-40243f99b16c.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-5b6649db44cc.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-883fd502727e.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md
?? docs/issues/improvement/20261125-allow-and-encourage-agent-coordination-while-ope-issue-949d201156b4.md
?? docs/issues/performance/
```

## Captured diff stat

```text
AGENTS.md                                          |  12 ++
 config/accounts/default.json                       |   4 +-
 config/analytics.json                              |   2 +-
 config/animations.json                             | 141 +++++++++++++++++++++
 config/audio/music-settings.json                   |   2 +-
 config/camconfig.json                              |   9 ++
 config/gamemodes/bloodtest.json                    |   6 +-
 config/input/gamepad.json                          |   2 +-
 config/vehicles/octane_rl.json                     |   8 +-
 config/weapons/bloodtest_ak47/config.json          |   2 +-
 devscripts/blender-ai                              |   0
 docs/issues/README.md                              |  26 +++-
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |   6 +
 ...-stop-targeting-player-af-issue-cd492e4faa08.md |   6 +
 ...m-match-has-no-team-no-ho-issue-5d82fc58a875.md |  35 +++++
 ...-resource-use-and-reasons-issue-b9edd402e658.md |  18 +++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  18 +++
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  51 ++++++++
 ...-while-server-starts-unde-issue-c8ac61cf07fc.md |   6 +
 docs/specs/gamemodes/bloodtest.md                  |  42 ++++++
 src/camera.cpp                                     |  91 +++++++++++--
 src/combat/weapon-viewmodel.cpp                    |  26 ++++
 src/config/camera-config.cpp                       |  29 +++++
 src/config/camera-config.h                         |  10 ++
 src/engine/engine-tick.cpp                         |  12 +-
 src/entities/player-render.cpp                     |   3 +-
 src/gamemode/gamemode-runtime-resolver.cpp         |   8 ++
 src/gamemode/gamemode.cpp                          |  21 +++
 src/gamemode/gamemode.h                            |  11 +-
 src/input/gamepad-input.cpp                        |  98 ++++++++++++--
 src/input/gamepad-input.h                          |  12 +-
 src/render/render-player.cpp                       |  21 ++-
 src/render/render-player.h                         |   5 +
 src/render/render-world-mesh.cpp                   |   4 +-
 src/render/render-world.cpp                        |   3 +-
 src/vehicles/vehicle-system.cpp                    |  44 ++++++-
 36 files changed, 734 insertions(+), 60 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/animations.json
M	config/audio/music-settings.json
M	config/camconfig.json
M	config/gamemodes/bloodtest.json
M	config/input/gamepad.json
M	config/vehicles/octane_rl.json
M	config/weapons/bloodtest_ak47/config.json
M	devscripts/blender-ai
M	docs/issues/README.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261122-fun-counter-strike-npcs-stop-targeting-player-af-issue-cd492e4faa08.md
M	docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
M	docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
M	docs/specs/gamemodes/bloodtest.md
M	src/camera.cpp
M	src/combat/weapon-viewmodel.cpp
M	src/config/camera-config.cpp
M	src/config/camera-config.h
M	src/engine/engine-tick.cpp
M	src/entities/player-render.cpp
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/input/gamepad-input.cpp
M	src/input/gamepad-input.h
M	src/render/render-player.cpp
M	src/render/render-player.h
M	src/render/render-world-mesh.cpp
M	src/render/render-world.cpp
M	src/vehicles/vehicle-system.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-25T20:35:21.601166Z",
    "latest_successful_build_id": "2026-11-25T20:35:21.601166Z:ff20ae8525ee2afc5727a0ca055796369445e697123df0bef8bb4d5b46422764",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "e70fd210a24291b2b1d513981bd91d60603203db088b4152232cd0ed7b998143",
    "dll": "build\\game\\generation-647\\game-g647.dll",
    "generation": 647,
    "revision": "1b99644e686cf2fcc7b3c044a206339a3a0be64abbfcebc79a8cc78f9f1e9bce",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-647\\game-g647.dll",
    "generation": 647,
    "published_utc": "2026-11-25T21:10:18.734Z",
    "revision": "1b99644e686cf2fcc7b3c044a206339a3a0be64abbfcebc79a8cc78f9f1e9bce",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 0.364,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-25T20:35:43.133Z"
  }
}
```

# Follow-up analysis

- Exact user context: `ctx-0b5a1ae6738f`, source ref `user-turn-2026-11-25-tdm-npc-postkill-followup`; quote is preserved verbatim in the coordination brain.
- Linked primarily to TDM issue `issue-bbd7b6567300`, and related to FCS issue `issue-cd492e4faa08`. Mode and run for the user's recollection remain unknown.
- The FCS issue has direct historical evidence from run `20261122_210337`, generation 523: after player 1 killed NPC 100010, an NPC selected hostile NPC 100008 at 228.04 m instead of player 1. This supports a post-kill target-retaliation problem in FCS, not a confirmed TDM reproduction.
- The current source inspection found FCS-only player threat weighting in `src/network/server-npcs.cpp`; this makes missing post-kill threat memory in TDM a plausible hypothesis. Target changes also clear old pursuit memory in `src/npc/npc.cpp`.
- Issue state at handoff: TDM issue is assigned to `opencode-deepseek-v4-1`; FCS issue is triaged to `fcs-npc-hostile-target-after-kill`. This session only appended the user's follow-up and correlation notes; it changed no gameplay code.
- Validation: no TDM runtime run or build was performed. `dev-loop --status` showed the active run was Vehicle Freeplay on Arena 1, not TDM. The next discriminating evidence is a pinned TDM run spanning a player kill, with target-change, team/roster, pursuit-memory, and damage events.
