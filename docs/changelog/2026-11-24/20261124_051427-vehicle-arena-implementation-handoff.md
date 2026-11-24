// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Vehicle Arena implementation handoff

# Task

- Task ID: vehicle-arena-handoff-20261124
- Session ID: vehicle-arena-handoff-20261124
- Owner: Codex
- Focus: Documentation handoff for shared vehicle handling, GLFW gamepad input, rebinding, and runtime proof.
- Summary: Investigated Vehicle Arena vehicle-profile wiring, controller APIs/binds, NPC setup, scoring, multiplayer control path, historical runtime evidence, and wrote a cold implementation handoff.
- Status: PASS
- Date, time, timezone: 2026-11-24T05:14:25+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Final commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83

# Pre-existing changes

- The checkout contained extensive unrelated dirty and untracked work before this documentation task. The full captured status and diff summary remain in the automatic evidence section below; none of those source changes are attributed to this session.
- Relevant pre-existing dirty paths include `src/input/input-commands.cpp`, `src/input/input-frame.h`, `src/input/input-poll.cpp`, `src/gamemode/gamemode.cpp`, `src/gamemode/gamemode.h`, `src/gamemode/gamemode-runtime-resolver.cpp`, `src/gui/gui-main.cpp`, and `src/network/server-gamemode.cpp`.

# Requested behavior

- The user clarified that the first priority is controllable vehicle physics and support for any controller, with configurable bindings. Vehicle Arena should become a repeatable networking testbed; NPC match setup and team-size console commands follow after handling/input.

# Specification alignment

- Read `docs/ROUTER.md` and routed vehicle, Vehicle Arena, gamemode, networking, agent coordination/build authority, proof, and runtime validation docs. The handoff points the implementer to the owning specs and records discrepancies without changing them.
- No gameplay specification was edited. Existing spec requirements call for fixed-tick shared vehicle simulation, Vehicle Arena actions, local prediction/server authority, and a playable match path.
- Decision: implement a measured controllability slice before expanding roster/console features. Do not claim support for every physical device without mapped-device and raw/manual fallback coverage.

# Exact implementation changes

## File: `docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md`

- Lines/functions/headings: whole handoff; current evidence, input/physics implementation sequence, proposed pad layout, acceptance criteria, runtime proof, and exact next steps.
- Old content or behavior: no consolidated source-based implementation handoff.
- New content or behavior: documents the current code/config/network path, gaps and uncertainty, preserves historical evidence limits, and gives an ordered implementation/validation plan.
- Reason: make the user's intended first slice implementable without guessing or duplicating vehicle/network ownership.
- Why unrelated behavior is preserved: this session changed documentation only; no gameplay source or configuration was changed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md` | absent | investigation handoff | implementation-ready findings | remove this new file only |
| `docs/changelog/2026-11-24/20261124_051427-vehicle-arena-implementation-handoff.md` | generated template | completed session record | required audit trail | remove this new file only |
| parent issue `issue-e5a086b0a441` | existing record | clarification/review appended through coordination workflow | preserve verbatim scope and evidence gap | append-only coordination record; do not rewrite history |

# Contract / ABI changes

- Contract: none.
- Version bump: none.
- Compatibility: unchanged.
- Reverse: not applicable.

# State / schema changes

- Domain: none.
- Old schema: unchanged.
- New schema: unchanged.
- Migration: none.
- Reverse: not applicable.

# Deleted paths

- None.

# Diagnostics

- No runtime diagnostic was added. Source/config searches and the historical changelog are cited in the handoff; none prove current runtime behavior.

# Validation

- Read the routed specs/workflows and inspected source/config/history listed in the handoff.
- No tests or builds were run; this was a documentation/investigation request and no behavior was changed.
- No current Vehicle Arena runtime or controller was launched. Historical run IDs are identified as historical, not current proof.
- Planned final checks: local markdown links, `git diff --check`, and the changelog session check.

# Measured evidence

- No before/after gameplay, latency, tick, or controller measurements were collected in this investigation.
- Source and historical evidence are qualified in the handoff; the implementation agent must gather fresh pinned runtime evidence.

# Regression review

- Regression entry appended: no.
- No confirmed regression was changed or observed.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: current mode selection/start, handling feel, controller discovery/binding, scoring, NPC roster, prediction/reconciliation, and network behavior.

# Agent handoff context

- Owner: next implementation agent, respecting the existing parent issue owner until coordination changes it.
- Focus: implement controllable shared car handling, any-controller input/rebinding, then prove the Vehicle Arena match/network path.
- Files to check: `docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md`, the parent capability issue, and every dirty source path listed there.
- What another agent should not redo: repository inventory and the source/config/history trace already summarized in the handoff; independently re-check dirty diffs before touching them.
- Next exact step: make the parent issue claim-ready through the issue workflow, inspect active coordination leases, then establish fresh baseline runtime evidence before the smallest handling/input change.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-24T03:30:52.372066Z", "latest_successful_build_id": "2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6", "latest_successful_source_generation": 20}, "game_current": {"build_mode": "debug", "code_hash": "0e7ff7a9733ced73fc1f53610cfd4e5639af1feaa60c840eae7651ff10ed2e35", "dll": "build\\game\\generation-581\\game-g581.dll", "generation": 581, "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-581\\game-g581.dll", "generation": 581, "published_utc": "2026-11-24T04:35:40.515Z", "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 59.525, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-24T03:30:44.219Z"}}
- Generation: 581
- Artifact: build\game\generation-581\game-g581.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_043709\client-02\events.jsonl
- Session ID: vehicle-arena-handoff-20261124

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- `docs/features/vehicles/vehicles.md` (requires fresh evidence and human feel acceptance).

## Captured git state

```text
M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/gamemodes/bloodtest.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/hot-reload/packet-ledger.json
 M config/maps/dust2cyberiav4.json
 M config/movement/movement-slowdm.json
 M config/movement/movement-source.json
 M config/postfx.json
 M config/tips.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/parallel_test_runs.py
 M docs/ROUTER.md
 M docs/features/npc-behavior.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-693e63059a7c.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-41a872c08642.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6641a6c6fdf4.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
 M docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/operations/devscripts/script-inventory.md
 M docs/specs/dev-loop/agent-proof-and-verification.md
 M docs/specs/dev-loop/multi-instance.md
 M docs/specs/dev-loop/visual-proof-screenshots.md
 M docs/specs/gamemodes/bodycam.md
 M docs/specs/graphics/graphics-post-processing.md
 M docs/workflows/runtime-scenario-validation.md
 M src/config/movement-config.cpp
 M src/engine/engine-tick-net.cpp
 M src/engine/engine-tick-ui-game-hud.cpp
 M src/engine/engine-tick-ui.cpp
 M src/engine/engine-tick-ui.h
 M src/engine/engine-tick.cpp
 M src/entities/aimbody-config.h
 M src/entities/player.h
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/gui/gui-main.cpp
 M src/gui/ui-system.cpp
 M src/gui/ui-system.h
 M src/input/input-commands.cpp
 M src/input/input-frame.h
 M src/input/input-poll.cpp
 M src/input/input-state.h
 M src/network/community-match-client.cpp
 M src/network/movement-validation.h
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
 M src/network/multiplayer-reconcile.cpp
 M src/network/multiplayer-tick.cpp
 M src/network/schema/packet-common.h
 M src/network/server-gamemode.cpp
 M src/network/server-packets.cpp
 M src/network/server-players.cpp
 M src/network/server.h
 M src/physics/movement/movement-conversion.cpp
 M src/physics/movement/movement-step.cpp
 M src/physics/movement/movement-step.h
 M src/physics/movement/movement-types.h
 M src/ragdoll/ragdoll-mode.cpp
 M src/render/post-fx.cpp
 M src/render/post-fx.h
 M src/sim/simulate-tick.cpp
?? devscripts/bloodtest-quick.py
?? devscripts/evidence-watch.py
?? devscripts/quality/test_dev_loop_cli_run_identity.py
?? docs/changelog/2026-11-23/20261123_2350-one-brain-emergent-fields-reference.md
?? docs/changelog/2026-11-24/
?? docs/coordination/decisions/20261124_002757-issue-136d4553ff92-decision-3f62c4f8633a.md
?? docs/gold/2026-11-23-one-brain-emergent-fields-research-reference.md
?? docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md
?? docs/handoffs/2026-11-24-live-evidence-snapshot-time-handoff.md
?? docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md
?? docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md
?? docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md
?? docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md
?? docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-linking-issue-627ca4b9f577.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-456061c38a8c.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-5bca92fcb543.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md
?? docs/issues/improvement/20261124-require-runtime-event-proof-for-every-gameplay-c-issue-dad84e4e7c50.md
?? docs/specs/dev-loop/live-evidence-observer.md
?? docs/specs/simulation/
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   4 +-
 config/gamemodes/bloodtest.json                    |  12 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |   2 +
 config/hot-reload/packet-ledger.json               | 236 ++++----
 config/maps/dust2cyberiav4.json                    | 110 +++-
 config/movement/movement-slowdm.json               |  12 +-
 config/movement/movement-source.json               |   2 +-
 config/postfx.json                                 |   5 +
 config/tips.json                                   |   6 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             |  27 +
 devscripts/dev_loop/cli.py                         |  28 +
 devscripts/dev_loop/parallel_test_runs.py          |  10 +-
 docs/ROUTER.md                                     |   4 +
 docs/features/npc-behavior.md                      |  85 +++
 docs/issues/README.md                              |  61 +-
 ...-snapshots-and-reconnecti-issue-453d0596e294.md |  40 ++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  18 +
 ...ness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md |   8 +
 ...et-exe-build-at-compiling-issue-00b133ae7e13.md |  37 ++
 ...et-exe-build-at-compiling-issue-693e63059a7c.md |  38 ++
 ...budget-exe-build-at-setup-issue-723326f11472.md |  39 ++
 ...ild-queue-at-build-pendin-issue-af958c3e10eb.md |  36 ++
 ...e-dll-build-at-game-build-issue-191782d7d9c5.md |  37 ++
 ...e-dll-build-at-game-build-issue-41a872c08642.md |  45 ++
 ...e-dll-build-at-game-build-issue-6641a6c6fdf4.md |  41 ++
 ...e-dll-build-at-game-build-issue-84ec2bf27598.md |  38 ++
 ...e-dll-build-at-game-build-issue-f66e6e82656b.md |  49 ++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  75 +++
 ...e-dll-build-at-game-build-issue-b1744402b0a5.md |  46 ++
 ...physics-and-car-based-mod-issue-e5a086b0a441.md |  45 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  34 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md | 106 ++++
 ...-a-specific-tick-for-proo-issue-89efbbc86535.md |   6 +
 ...opment-and-networking-con-issue-cf9cd77eb056.md | 177 ++++++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  33 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 621 +++++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 370 ++++++++++++
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |  48 ++
 ...-while-server-starts-unde-issue-c8ac61cf07fc.md |  12 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   8 +
 docs/operations/devscripts/script-inventory.md     |   2 +
 .../specs/dev-loop/agent-proof-and-verification.md |  34 ++
 docs/specs/dev-loop/multi-instance.md              |   6 +
 docs/specs/dev-loop/visual-proof-screenshots.md    |  34 +-
 docs/specs/gamemodes/bodycam.md                    |  41 +-
 docs/specs/graphics/graphics-post-processing.md    |  49 +-
 docs/workflows/runtime-scenario-validation.md      |   7 +
 src/config/movement-config.cpp                     |  20 +
 src/engine/engine-tick-net.cpp                     |   4 +
 src/engine/engine-tick-ui-game-hud.cpp             |  22 +
 src/engine/engine-tick-ui.cpp                      |   4 +-
 src/engine/engine-tick-ui.h                        |   2 +-
 src/engine/engine-tick.cpp                         |  17 +-
 src/entities/aimbody-config.h                      |   6 +
 src/entities/player.h                              |   1 +
 src/gamemode/gamemode-runtime-resolver.cpp         |   4 +
 src/gamemode/gamemode.cpp                          |   3 +
 src/gamemode/gamemode.h                            |   2 +
 src/gui/gui-main.cpp                               |   2 +-
 src/gui/ui-system.cpp                              |  22 +-
 src/gui/ui-system.h                                |   3 +-
 src/input/input-commands.cpp                       |   2 +
 src/input/input-frame.h                            |   1 +
 src/input/input-poll.cpp                           |   2 +
 src/input/input-state.h                            |   2 +
 src/network/community-match-client.cpp             |  15 +
 src/network/movement-validation.h                  |   3 +-
 src/network/multiplayer-context.h                  |   1 +
 src/network/multiplayer-projectiles.cpp            |   5 +
 src/network/multiplayer-reconcile.cpp              |   1 +
 src/network/multiplayer-tick.cpp                   |   1 +
 src/network/schema/packet-common.h                 |   3 +
 src/network/server-gamemode.cpp                    |   1 +
 src/network/server-packets.cpp                     |   4 +
 src/network/server-players.cpp                     |  28 +-
 src/network/server.h                               |   1 +
 src/physics/movement/movement-conversion.cpp       |   4 +
 src/physics/movement/movement-step.cpp             |  83 ++-
 src/physics/movement/movement-step.h               |   8 +
 src/physics/movement/movement-types.h              |  27 +
 src/ragdoll/ragdoll-mode.cpp                       |  35 +-
 src/render/post-fx.cpp                             |   1 +
 src/render/post-fx.h                               |   6 +
 src/sim/simulate-tick.cpp                          |  38 ++
 87 files changed, 2961 insertions(+), 215 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/hot-reload/packet-ledger.json
M	config/maps/dust2cyberiav4.json
M	config/movement/movement-slowdm.json
M	config/movement/movement-source.json
M	config/postfx.json
M	config/tips.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/parallel_test_runs.py
M	docs/ROUTER.md
M	docs/features/npc-behavior.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-693e63059a7c.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-41a872c08642.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6641a6c6fdf4.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
M	docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/operations/devscripts/script-inventory.md
M	docs/specs/dev-loop/agent-proof-and-verification.md
M	docs/specs/dev-loop/multi-instance.md
M	docs/specs/dev-loop/visual-proof-screenshots.md
M	docs/specs/gamemodes/bodycam.md
M	docs/specs/graphics/graphics-post-processing.md
M	docs/workflows/runtime-scenario-validation.md
M	src/config/movement-config.cpp
M	src/engine/engine-tick-net.cpp
M	src/engine/engine-tick-ui-game-hud.cpp
M	src/engine/engine-tick-ui.cpp
M	src/engine/engine-tick-ui.h
M	src/engine/engine-tick.cpp
M	src/entities/aimbody-config.h
M	src/entities/player.h
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/gui/gui-main.cpp
M	src/gui/ui-system.cpp
M	src/gui/ui-system.h
M	src/input/input-commands.cpp
M	src/input/input-frame.h
M	src/input/input-poll.cpp
M	src/input/input-state.h
M	src/network/community-match-client.cpp
M	src/network/movement-validation.h
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
M	src/network/multiplayer-reconcile.cpp
M	src/network/multiplayer-tick.cpp
M	src/network/schema/packet-common.h
M	src/network/server-gamemode.cpp
M	src/network/server-packets.cpp
M	src/network/server-players.cpp
M	src/network/server.h
M	src/physics/movement/movement-conversion.cpp
M	src/physics/movement/movement-step.cpp
M	src/physics/movement/movement-step.h
M	src/physics/movement/movement-types.h
M	src/ragdoll/ragdoll-mode.cpp
M	src/render/post-fx.cpp
M	src/render/post-fx.h
M	src/sim/simulate-tick.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-24T03:30:52.372066Z",
    "latest_successful_build_id": "2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6",
    "latest_successful_source_generation": 20
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "0e7ff7a9733ced73fc1f53610cfd4e5639af1feaa60c840eae7651ff10ed2e35",
    "dll": "build\\game\\generation-581\\game-g581.dll",
    "generation": 581,
    "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-581\\game-g581.dll",
    "generation": 581,
    "published_utc": "2026-11-24T04:35:40.515Z",
    "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0",
    "status": "ready"
  },
  "progress": {
    "compiled": 4,
    "elapsed_s": 59.525,
    "phase": "build_complete",
    "result": "success",
    "skipped": 83,
    "updated_utc": "2026-11-24T03:30:44.219Z"
  }
}
```
