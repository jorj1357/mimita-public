// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# TDM NPC target and stairs issue intake

# Task

- Task ID: session-20261125-204551
- Session ID: session-20261125-204551
- Owner: npc_tdm_retention_stairs_triage
- Focus: Issue intake only: issue-bbd7b6567300; issue-d9e93ee0aa8a. Full user context ctx-8252f8917eca linked to both; related prior records include issue-cd492e4faa08, issue-5d82fc58a875, issue-82cdc1277704 and issue-b18e63894585.
- Summary: Recorded TDM NPC target-retention and stair-navigation reports as two unassigned, agent-claimable investigations; no runtime reproduction or code change was performed.
- Status: issues recorded; behavior unverified; human/runtime review pending
- Date, time, timezone: 2026-11-25T20:45:57+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: c7fd30cf16d92e088881977159f161face60a176
- Final commit: c7fd30cf16d92e088881977159f161face60a176

# Pre-existing changes

- Exact status output: The changelog generator captured the checkout after issue intake. It shows many unrelated dirty files, including ongoing Bloodtest, input, renderer, coordination, and issue work. I did not edit those source/config files. I did not capture `git status` before creating the issue records, so I cannot separate every pre-existing entry from generated issue-index changes; preserve all unrelated paths as-is.
- Files not created or modified by this session: All captured `config/`, `src/`, and other issue/changelog paths outside the two new issue records, the generated `docs/issues/README.md` index update, and this final changelog. The status snapshot is retained under `## Captured git state` below.

# Requested behavior

- The user reports that enemy NPCs in TDM initially attack aggressively, then at random stop noticing, targeting, or attacking the player later in the round.
- The user also reports NPCs descend stairs, get stuck below, and do not come back up; they want stair up/down navigation improved.
- The request was to add/explain the reports for agents to track. No implementation or root-cause decision was authorized or attempted.
- Canonical verbatim context: `ctx-8252f8917eca` (`user-turn-2026-11-25-npc-tdm-stairs-report`), linked primarily to `issue-bbd7b6567300` and related to `issue-d9e93ee0aa8a`.

# Specification alignment

- Read `docs/specs/gamemodes/teamdeathmatch.md`: the TDM contract says authoritative team identity must exist before NPC targeting and that an actor with unknown team is not hostile. This makes team identity a required investigation check, not an assumed cause.
- Read `docs/specs/npc-behavior/npc-behavior.md`, section 9, “Persistent target pursuit”: all NPCs keep and pursue a target after line of sight is lost; immediate forgetting is explicitly not the default. The targeting issue asks agents to identify the first incorrect transition and validate the live result.
- Read `docs/architecture/player-npc-systems/npc-movement.md`: Recast input includes stairs and connected surfaces; Detour responsibilities include multi-floor navigation and route recovery. The stair issue asks for the actual map/route and first failing step before changing the shared navigation owner.
- Read `docs/workflows/issue-intake-and-triage.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/change-to-evidence-latency.md`, `docs/skills/issue-triage-v1.md`, `docs/regressions/README.md`, and `docs/operations/task-completion/task-completion.md`.
- No specification or TODO was edited. The issue wording distinguishes user observations from unknown causes and requires pinned runtime evidence before any behavior completion.

# Exact implementation changes

- Created `issue-bbd7b6567300`, “TDM NPCs lose hostility and stop targeting the player during a round,” at `docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md` (human quote at line 5). Complete summary, reproduction, expected behavior, acceptance, paths, risk, and proof expectation are recorded in the coordination issue record. State `new`, unassigned, high priority.
- Created `issue-d9e93ee0aa8a`, “TDM NPCs get stuck below stairs and fail to recover upward,” at `docs/issues/bug/20261125-tdm-npcs-get-stuck-below-stairs-and-fail-to-reco-issue-d9e93ee0aa8a.md` (human quote at line 5). Complete investigation and proof fields are recorded in the issue record. State `new`, unassigned, high priority.
- Linked the single full user context to both records. Related existing records are noted in the new issue evidence: FCS hostility after kill (`issue-cd492e4faa08`), live TDM join identity (`issue-5d82fc58a875`), and generic NPC navigation (`issue-82cdc1277704`, `issue-b18e63894585`).
- `docs/issues/README.md` is the generated issue index updated by issue creation; it was not hand-edited. No gameplay code, specs, or configs changed.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md` | New append-only issue record | Preserve the report; correct through issue coordination, do not delete |
| `docs/issues/bug/20261125-tdm-npcs-get-stuck-below-stairs-and-fail-to-reco-issue-d9e93ee0aa8a.md` | New append-only issue record | Preserve the report; correct through issue coordination, do not delete |
| `docs/issues/README.md` | Generated index reflects issue records | Regenerate from coordination brain; do not hand-edit |
| `docs/changelog/2026-11-25/20261125_204559-tdm-npc-target-and-stairs-issue-intake.md` | This final session record | Retain as session history |

# Diagnostics

- Owner/category: No runtime diagnostic owner was changed; issue intake only.
- Input: User-reported TDM symptoms; no map, run, NPC ID, or journal supplied.
- Decision: Keep targeting retention and stair navigation as separate agent-claimable investigations; do not infer a shared cause.
- Output: Two complete open bug issues; no source/build/runtime modification.
- Failure/rejection reason: Not applicable.
- Rate limiting: Not applicable.

# Validation

- Focused skill: `docs/skills/issue-triage-v1.md` — PASS for preserved report excerpts, complete issue fields, unassigned state, and runtime proof expectations.
- Routed docs read: TDM spec, NPC behavior spec, NPC movement architecture, issue intake workflow, agent coordination/build authority/evidence latency specs, regression index, and task completion instructions (paths above).
- Tests: Not run; this was issue intake only.
- Build: No build requested. `python devscripts/dev-loop.py --status` reported no active run; the checkout already had game DLL generation 644 published/current during this session. That unrelated build is not evidence for either issue.
- Runtime: NOT TESTED. The automatic changelog snapshot contains a journal path, but no TDM run was linked to this task; it is not used as proof.

# Measured evidence

- Before/after values: None; no runtime scenario or measurements were supplied or collected.
- Required next evidence is listed in each issue: pinned TDM build/generation, run ID, server/client `events.jsonl`, exact NPC identities and targeting/stair transitions, followed by a repeat after any fix.

# Regression review

- Regression entry appended: no. These are new reports with no confirmed first failing code or verified regression occurrence.
- Related regression path: `docs/regressions/npc-movement/README.md`.

# Human acceptance

- Visual/gameplay/multiplayer review: not performed.
- Still unverified: Both reported behaviors and their causes. A build or issue record does not prove a fix.

# Agent handoff context

- Owner: Unassigned; both issues are available for agent claim.
- Focus: First reproduce in TDM, record a pinned run, and trace the first wrong transition. For targeting, inspect team/role identity, perception/memory, target retention, and attack gating. For stairs, record map geometry, route goal, stuck interval, and recovery choice.
- What another agent should not redo: Do not treat the related FCS post-kill issue or the late-join identity issue as proof of this mid-round TDM symptom. Do not infer a stair/navmesh cause without a pinned route capture.
- Next exact step: Claim either issue via `agent-coord next --claim`, then follow its reproduction and proof expectations.
# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / c7fd30cf16d92e088881977159f161face60a176 / c7fd30cf16d92e088881977159f161face60a176
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-25T20:35:21.601166Z", "latest_successful_build_id": "2026-11-25T20:35:21.601166Z:ff20ae8525ee2afc5727a0ca055796369445e697123df0bef8bb4d5b46422764", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "4bec9b79dd5e513b9a97c917fc18983ae246732a6dd5a9cb2940506cae850c60", "dll": "build\\game\\generation-644\\game-g644.dll", "generation": 644, "revision": "56e940a192eb5e4b3d0876626436d457a2cf59ce7e1a8af49cca1daa07119643", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-644\\game-g644.dll", "generation": 644, "published_utc": "2026-11-25T20:44:06.604Z", "revision": "56e940a192eb5e4b3d0876626436d457a2cf59ce7e1a8af49cca1daa07119643", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.364, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-25T20:35:43.133Z"}}
- Generation: 644
- Artifact: build\game\generation-644\game-g644.dll
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_184820\client-01\events.jsonl
- Session ID: session-20261125-204551

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
 M config/animations.json
 M config/audio/music-settings.json
 M config/gamemodes/bloodtest.json
 M config/input/gamepad.json
 M config/weapons/bloodtest_ak47/config.json
 m devscripts/blender-ai
 M docs/issues/README.md
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
?? docs/changelog/2026-11-25/20261125_1933-issue-0cdac76fb006-independent-review.md
?? docs/issues/bug/20261125-mimita-client-burns-20-33-percent-cpu-while-mini-issue-a6be56f9d9fe.md
?? docs/issues/bug/20261125-tdm-npcs-get-stuck-below-stairs-and-fail-to-reco-issue-d9e93ee0aa8a.md
?? docs/issues/bug/20261125-tdm-npcs-lose-hostility-and-stop-targeting-the-p-issue-bbd7b6567300.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-5b6649db44cc.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-883fd502727e.md
?? docs/issues/performance/
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/animations.json                             | 141 +++++++++++++++++++++
 config/audio/music-settings.json                   |   2 +-
 config/gamemodes/bloodtest.json                    |   6 +-
 config/input/gamepad.json                          |   2 +-
 config/weapons/bloodtest_ak47/config.json          |   2 +-
 devscripts/blender-ai                              |   0
 docs/issues/README.md                              |  12 +-
 ...m-match-has-no-team-no-ho-issue-5d82fc58a875.md |  35 +++++
 ...-resource-use-and-reasons-issue-b9edd402e658.md |   6 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  12 ++
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  51 ++++++++
 docs/specs/gamemodes/bloodtest.md                  |  42 ++++++
 src/combat/weapon-viewmodel.cpp                    |  26 ++++
 src/config/camera-config.cpp                       |   2 +
 src/config/camera-config.h                         |   3 +
 src/engine/engine-tick.cpp                         |  12 +-
 src/entities/player-render.cpp                     |   2 +-
 src/gamemode/gamemode-runtime-resolver.cpp         |   8 ++
 src/gamemode/gamemode.cpp                          |  21 +++
 src/gamemode/gamemode.h                            |  11 +-
 src/input/gamepad-input.cpp                        |  94 ++++++++++++--
 src/input/gamepad-input.h                          |  12 +-
 src/render/render-player.cpp                       |  20 ++-
 src/render/render-player.h                         |   5 +
 src/render/render-world-mesh.cpp                   |   4 +-
 src/render/render-world.cpp                        |   2 +-
 28 files changed, 505 insertions(+), 36 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/animations.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/input/gamepad.json
M	config/weapons/bloodtest_ak47/config.json
M	devscripts/blender-ai
M	docs/issues/README.md
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
    "code_hash": "4bec9b79dd5e513b9a97c917fc18983ae246732a6dd5a9cb2940506cae850c60",
    "dll": "build\\game\\generation-644\\game-g644.dll",
    "generation": 644,
    "revision": "56e940a192eb5e4b3d0876626436d457a2cf59ce7e1a8af49cca1daa07119643",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-644\\game-g644.dll",
    "generation": 644,
    "published_utc": "2026-11-25T20:44:06.604Z",
    "revision": "56e940a192eb5e4b3d0876626436d457a2cf59ce7e1a8af49cca1daa07119643",
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

