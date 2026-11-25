// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# TDM NPC target selection diagnosis

# Task

- Task ID: session-0ffd4314d52d
- Session ID: session-0ffd4314d52d
- Owner: tdm-npc-target-retention-diagnosis
- Focus: Diagnosis/update to issue-bbd7b6567300. Inspected config/gamemodes/tdm.json, src/network/server-npcs.cpp, src/npc/npc-behavior.cpp, src/npc/npc-behavior-module.cpp, src/npc/npc-targeting.cpp, src/npc/npc.cpp, TDM/NPC specs and related issue-cd492e4faa08.
- Summary: Source review found a plausible target-switch explanation: TDM scores hostile players and NPCs together, and switching targets clears the old pursuit memory. A safer fix direction is human priority separated from TDM hostility rules; runtime confirmation is still required.
- Status: source hypothesis recorded; no code change; no TDM runtime proof
- Date, time, timezone: 2026-11-25T21:03:39+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: c7fd30cf16d92e088881977159f161face60a176
- Final commit: c7fd30cf16d92e088881977159f161face60a176

# Pre-existing changes

- The generator captured the shared checkout after this investigation had begun. Its git-state snapshot includes unrelated ongoing edits. I did not edit source, configuration, or those unrelated files. The pre-investigation `git status` baseline was not captured, so I do not attribute other dirty entries to this task.

# Requested behavior

The user asks why TDM NPCs lose the player as a target mid-round and how to fix it. The earlier issue is `issue-bbd7b6567300`; this session added a source-based hypothesis and fix direction there. At review time, the issue state was `assigned` to `opencode-deepseek-v4-1`; this session made no source edit and did not take implementation ownership.

# Specification alignment

- `config/gamemodes/tdm.json:245-251` selects `elite_squad`, `opposite_team`, and both human/NPC target candidates.
- `docs/specs/npc-behavior/npc-behavior.md:727-746` requires target pursuit to persist through lost line of sight; it does not require the NPC to preserve one target against a higher-scoring valid enemy.
- `docs/specs/gamemodes/teamdeathmatch.md:70-75` says team identity must be authoritative before targeting and an actor with team `-1` is not hostile.
- Therefore a TDM-specific preference must remain separate from hostility/team validity. Directly switching to the legacy `player` mode is unsafe because `src/npc/npc-targeting.cpp:30-35` treats teamless actors as hostile in that mode.

# Exact implementation changes

- No code/config change. Read-only source inspection found a plausible explanation:
  - `src/network/server-npcs.cpp:2402-2417` has a player-first branch only for `NpcTargetingMode::Player`; TDM uses `OppositeTeam`, which proceeds to scored selection of hostile humans and NPCs (`:2423-2505`, `:2534-2573`).
  - `src/npc/npc-behavior-module.cpp:343-378` scores distance, vulnerability, weapon threat, current-target stickiness, and switch threshold. Elite defaults in `src/npc/npc-behavior.cpp:80-81` set stickiness to `0.85` and threshold to `0.05`, so target changes remain possible.
  - In `server-npcs.cpp:2429-2444`, the additional player retaliation threat is explicitly limited to FCS; TDM players get the generic `0.5` threat, while hostile NPC candidates derive threat from weapon damage (`:2492-2495`, `:2561-2564`). Thus another hostile NPC can plausibly outscore a player as distances/health change.
  - `src/npc/npc.cpp:321-325` clears perception memory whenever `serverTargetId` changes. A switch from the player to another NPC therefore drops the previous player pursuit memory and can look like the NPC stopped caring.
  - The server already emits `npc.target-changed` with previous/current actor, target reason, profile, and distance (`src/network/server-npcs.cpp:2662-2674`). A relevant prior FCS run in `issue-cd492e4faa08` observed a target switch to a hostile NPC after a player kill; it is analogous evidence from a different mode, not proof of this TDM occurrence.
- Recorded this hypothesis and fix direction as an append-only suggestion on `issue-bbd7b6567300` at 2026-11-25T21:01:18Z.
- Proposed fix: add a generic human-target preference while retaining TDM `opposite_team` hostility and NPC fallback. Also retain/reacquire a valid hidden human target. Do not weaken team validation by simply changing TDM to `player` mode.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md` | Appended source hypothesis and fix direction | Preserve issue history; supersede with later evidence rather than erase |
| `docs/issues/README.md` | Generated index reflects issue state | Regenerate from coordination brain |
| `docs/changelog/2026-11-25/20261125_210339-tdm-npc-target-selection-diagnosis.md` | This session record | Retain as history |

# Diagnostics

- Existing target-change logging is available but no TDM target transition was captured for the user report.
- Recommended additional bounded diagnostic fields: candidate actor type/team/score, current target, switch reason, and target invalidation reason.

# Validation

- Source/spec inspection only; no tests or build requested.
- `python devscripts/dev-loop.py --status` showed running run `20261125_204934_01`, launch mode 60 `vehicle_freeplay`, map `arena1`; this is not a TDM run and does not verify the symptom.
- No TDM runtime log from the reported failure was available. Root cause remains a hypothesis.
- No gameplay, multiplayer, visual, or human acceptance was performed.

# Measured evidence

No before/after measurements. The next discriminating check is a pinned TDM multi-round run correlating `npc.target-changed` target IDs with player/NPC teams, candidate liveness, pursuit-memory reset, and `server.damage.applied`.

# Regression review

No confirmed new regression was proven, so no regression record was appended. Related history: `issue-cd492e4faa08` (FCS post-kill target switching) and `issue-5d82fc58a875` (late-join TDM team identity).

# Human acceptance

The source path and fix proposal are explained, but gameplay confirmation remains outstanding. Verify after any implementation in the real TDM runtime and keep the target team rule intact.

# Agent handoff context

- Existing issue: `issue-bbd7b6567300`, assigned to `opencode-deepseek-v4-1` at 2026-11-25T21:01:18.659Z.
- Next exact step: correlate a TDM `npc.target-changed` event with valid enemy players/NPCs, then decide whether the first failure is scored target switching, team identity, memory clearing, or an attack gate. If target competition is confirmed, implement human preference through the generic targeting policy without changing TDM hostility semantics.
- This session did not edit the assigned agent's source paths.
# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / c7fd30cf16d92e088881977159f161face60a176 / c7fd30cf16d92e088881977159f161face60a176
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-25T20:35:21.601166Z", "latest_successful_build_id": "2026-11-25T20:35:21.601166Z:ff20ae8525ee2afc5727a0ca055796369445e697123df0bef8bb4d5b46422764", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "c54e45abf3802bd60b63ab465463758fbe84779006abdd5c7e0a1bd402836b9d", "dll": "build\\game\\generation-646\\game-g646.dll", "generation": 646, "revision": "2c11a9500c54a93b736d33ee24c75eb8e22183ce9daafd8a9c4ff0dabe7b197a", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-646\\game-g646.dll", "generation": 646, "published_utc": "2026-11-25T21:02:21.095Z", "revision": "2c11a9500c54a93b736d33ee24c75eb8e22183ce9daafd8a9c4ff0dabe7b197a", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.364, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-25T20:35:43.133Z"}}
- Generation: 646
- Artifact: build\game\generation-646\game-g646.dll
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_210233_rl-check\server-01\events.jsonl
- Session ID: session-0ffd4314d52d

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
 M config/gamemodes/bloodtest.json
 M config/input/gamepad.json
 M config/vehicles/octane_rl.json
 M config/weapons/bloodtest_ak47/config.json
 m devscripts/blender-ai
 M docs/issues/README.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
 M docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/specs/gamemodes/bloodtest.md
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
?? docs/issues/bug/20261125-mimita-client-burns-20-33-percent-cpu-while-mini-issue-a6be56f9d9fe.md
?? docs/issues/bug/20261125-tdm-npcs-get-stuck-below-stairs-and-fail-to-reco-issue-d9e93ee0aa8a.md
?? docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md
?? docs/issues/build-friction/20261125-test-run-contention-an-active-dashboard-runtime--issue-16e2cf31162b.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-2e5c794199f1.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-5b6649db44cc.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-883fd502727e.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-f1de1fcd87a2.md
?? docs/issues/performance/
```

## Captured diff stat

```text
AGENTS.md                                          |  12 ++
 config/accounts/default.json                       |   4 +-
 config/analytics.json                              |   2 +-
 config/animations.json                             | 141 +++++++++++++++++++++
 config/audio/music-settings.json                   |   2 +-
 config/gamemodes/bloodtest.json                    |   6 +-
 config/input/gamepad.json                          |   2 +-
 config/vehicles/octane_rl.json                     |   8 +-
 config/weapons/bloodtest_ak47/config.json          |   2 +-
 devscripts/blender-ai                              |   0
 docs/issues/README.md                              |  19 ++-
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |   6 +
 ...m-match-has-no-team-no-ho-issue-5d82fc58a875.md |  35 +++++
 ...-resource-use-and-reasons-issue-b9edd402e658.md |   6 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  12 ++
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  51 ++++++++
 docs/specs/gamemodes/bloodtest.md                  |  42 ++++++
 src/combat/weapon-viewmodel.cpp                    |  26 ++++
 src/config/camera-config.cpp                       |   3 +
 src/config/camera-config.h                         |   3 +
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
 src/vehicles/vehicle-system.cpp                    |  15 +++
 32 files changed, 552 insertions(+), 43 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/animations.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/input/gamepad.json
M	config/vehicles/octane_rl.json
M	config/weapons/bloodtest_ak47/config.json
M	devscripts/blender-ai
M	docs/issues/README.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
M	docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/specs/gamemodes/bloodtest.md
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
    "code_hash": "c54e45abf3802bd60b63ab465463758fbe84779006abdd5c7e0a1bd402836b9d",
    "dll": "build\\game\\generation-646\\game-g646.dll",
    "generation": 646,
    "revision": "2c11a9500c54a93b736d33ee24c75eb8e22183ce9daafd8a9c4ff0dabe7b197a",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-646\\game-g646.dll",
    "generation": 646,
    "published_utc": "2026-11-25T21:02:21.095Z",
    "revision": "2c11a9500c54a93b736d33ee24c75eb8e22183ce9daafd8a9c4ff0dabe7b197a",
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

