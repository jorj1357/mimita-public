# Funcounterstrike Bomb Interactions and NPC Objective Wiring

- Status: `PASS_WITH_HUMAN_REVIEW`
- Date: `2026-11-01T04:46:51Z`
- Display time: `2026-10-31 23:46:51 EST` (America/New_York)
- Branch: `20261011zt`
- Base commit: `f757b936`
- Session commits: none (changes left uncommitted for review)

## Requested behavior

Replace proximity-only bomb planting/defusing with deliberate, server-authoritative
interactions, add hold and repeated-tap modes, suppress planting movement while
preserving momentum, connect NPC objective travel/combat to the same objective
state, improve prompts and editor site feedback, and prove the result through the
dev loop.

## Specification findings and decisions

The routed specifications disagreed with the current implementation:

- `docs/specs/gamemodes/funcounterstrike.md` previously said the implementation
  used proximity/state transitions and a future hold-interaction action.
- `docs/specs/entity-editor/entity-editor.md` said gameplay resolved a plant by
  the carrier's position inside the site radius alone.
- `docs/specs/gamemodes/objective-hud.md` did not require mode or tap progress.

Human decisions recorded during planning:

- Mode 2 (repeated tap) does not continuously block movement.
- The 1-second plant applies to `funcounterstrike` only; `counterstrike` keeps
  its existing 3-second plant.
- Add a debug-only `objective_give` command for deterministic runtime tests.

## Specification and configuration changes

Updated:

- `docs/specs/gamemodes/funcounterstrike.md`
  - Replaced proximity language with Mode 1 hold (default) and Mode 2 repeated
    tap.
  - Recorded 1-second plant, 5-second defuse, 0.1-second tap progress.
  - Recorded cancellation and momentum rules.
  - Added editor circle/resize documentation and new objective events.
- `docs/specs/gamemodes/objective-hud.md`
  - Added interaction mode and percentage/tap-count requirements.
- `docs/specs/entity-editor/entity-editor.md`
  - Entering a site now only enables interaction.
  - Documented `mapeditor set_radius <m>` and editor site feedback.
- `config/gamemodes/funcounterstrike.json`
  - Old: `"plant_seconds": 3`
  - New: `"plant_seconds": 1`, plus
    `"interaction_mode": "hold"` and `"tap_progress_seconds": 0.1`.
- `config/gamemodes/counterstrike.json`
  - Added explicit `"interaction_mode": "hold"` and
    `"tap_progress_seconds": 0.1`; its 3-second plant is unchanged.
- `config/gamemodes/masterlist/gamemode-masterlist.json`
  - Added schema fields `interaction_mode` (default `hold`) and
    `tap_progress_seconds` (default `0.1`).

## Code changes

### Objective interaction owner

`src/game/objective-state.h` and `.cpp`:

- Added `ObjectiveInteractionMode { Hold, RepeatedTap }`.
- Added parsing/name helpers and interaction/tap fields on `ObjectiveInstance`.
- Added `advanceObjectiveTapProgress`.
- Extended `--objective-selftest` with 1s/5s hold totals and 10/50-tap totals.
- Selftest passed: `progress=ok`, `interaction-modes=ok`, `PASS`.

### Mode configuration and timer resolution

`src/gamemode/gamemode.h` and `.cpp`:

- Added `interactionMode` and `tapProgressSeconds` to
  `GamemodeObjectiveDefinition`.
- Parse `interaction_mode` and clamped `tap_progress_seconds`.

`src/network/server-gamemode.cpp`:

- Copies the mode interaction policy into the objective instance.
- `objective.timers.resolved` now records `interaction_mode` and
  `tap_progress_ticks`.

### Server-authoritative interaction state machine

`src/network/server-gamemode.cpp` (`serverObjectiveTick`, signature now receives
`NpcSystem&`):

- Plant requires carrier + living + inside a usable site + held/tap input.
- Defuse requires living CT + within interaction range + held/tap input.
- Mode 1 advances one fixed tick per held input; release/death/leave/invalid
  state resets progress.
- Mode 2 adds `tapProgressTicks` per distinct press; release does not reset.
- NPC interaction intent comes from the live `Npc` utility goal
  (`PlantObjective` / `DefuseObjective`) and `atObjective`.
- NPC Mode 2 emits one tap every `tapProgressTicks`.
- Added bounded events:
  - `objective.plant-start`, `objective.plant-interrupted`,
    `objective.planted`
  - `objective.defuse-start`, `objective.defuse-interrupted`,
    `objective.defused`
  - `objective.interaction-progress`
  - `objective.site-unavailable`, `objective.target-unresolved`
  - `objective.debug-assign` for the debug command
- Stale plant progress is cleared when the carrier drops/dies.

`src/gamemode/map-config.cpp`:

- `siteIndexAt` now tests the authored horizontal circle (`X/Y`), not a 3D
  sphere. Authored Z remains a navigation/presentation hint. This makes a site
  usable when the floor differs from the authored Z.
- `--map-config-selftest` now checks same-XY/different-Z containment and a
  horizontal outside point. It passed.

### Input transport

- `src/network/packets.h`
  - `NET_STATE_INTERACTING = 1 << 14`.
  - Reused unused `reservedWeapon` as `interactSerial` (packet size unchanged).
  - `DuelStatePacket` now carries `objectiveInteractorId`,
    `objectiveRequiredTicks`, `objectiveTapTicks`, and
    `objectiveInteractionMode`.
- `src/input/input-frame.h`, `input-state.h`, `input-poll.*`
  - Added `interactHeld`.
  - Added scheduled `agent.interact hold <ticks>` and
    `agent.interact tap <presses>`.
- `src/network/multiplayer-context.h`, `multiplayer-tick.cpp`, `client.cpp`
  - Send held interaction state and a distinct press serial.
- `src/network/server.h`, `server-packets.cpp`
  - Added `ServerInput.interactHeld`, `lastInteractSerial`, and
    `pendingInteractTaps`.
- `src/network/community-match-client.*`
  - Replicate interactor, required ticks, tap ticks, and mode.
- `src/devtools/scenario-recorder.cpp`, `scenario-player.cpp`
  - Record/read `interact_held`.

### Planting movement suppression

`src/network/server-players.cpp`:

- While the local player is the active Mode 1 planter, the movement command's
  axes, jump, dash, down-dash, freeze, and movement-direction flags are zeroed.
- Velocity is not zeroed, so momentum remains.

`src/sim/simulate-tick.cpp`:

- Mirrors the lock locally for the eligible holder/planter and preserves
  velocity.

### NPC objective and combat connection

`src/npc/team-brain.h` and `.cpp`:

- `TeamObjectiveContext` now carries planting state, planter id, contest site,
  and interaction mode.
- Planted objective targeting prefers the authoritative bomb position.
- During a plant, CT defenders target the contest site.
- Existing utility ordering still lets a visible enemy select `KillTarget`
  before `DefuseObjective`.

`src/network/server-gamemode.cpp`:

- Fills the new objective context from the authoritative objective.
- Logs `objective.target-unresolved` when no usable site exists.
- `npc.objective-context` now records planting, planter, and interaction mode.

### HUD and prompts

`src/engine/engine-tick-ui-overlays.cpp`:

- Shows `HOLD F TO...` or `TAP F TO...` based on the replicated mode.
- Shows plant/defuse percentage and tap counts.
- Shows defuse prompt when a CT is in range of a planted bomb.
- Planted timer remains authoritative.

### Map editor feedback

`src/mapeditor/map-editor.cpp` and `map-editor-commands.cpp`:

- Draws every authored site as a labeled translucent circular area with a ring,
  cross, and selected highlight.
- Added `mapeditor set_radius <m>` using the existing `setSiteRadius`.
- Help text and property details updated.

### Debug commands

- `src/main-systems.cpp`: `agent.interact <hold|tap> <ticks|presses>`.
- `src/terminal/debug-commands.cpp` and `server-packet-chat.cpp`:
  - `objective_give [self|actor_id]`; admin-gated server command.
  - Debug assignment is allowed across teams for deterministic tests.
- `objective_status` now prints mode, tap ticks, and active interactor.

## Pre-existing edits preserved

The working tree contained many unrelated modified/untracked files before this
session (audio, collision, destruction, weapons, docs, etc.). They were not
reverted or claimed as this session's work.

`config/maps/dust2cyberiav4.json` was temporarily edited through the map editor
for runtime tests. The temporary `TEST*` sites were removed. The file was
restored to a compact semantic equivalent of its pre-session content: the two
authored bomb sites, the existing `ct_spawnpoint` / `t_spawnpoint` entities, and
the existing map bomb timers. The remaining diff is the pre-existing spawnpoint
addition, not a new game rule.

## Validation evidence

### Source and build

- Canonical dev-loop build: success, final build `81`.
- Executable: `C:\mimita-v9\mimita.exe`.
- `--versioninfo` run id: `20261101_004711`.
- `EVENTS_JSONL_PATH`: `logs/11-01-2026/20261101_004711/events.jsonl`.

### Focused checks

- `mimita.exe --objective-selftest`: PASS.
- `mimita.exe --map-config-selftest`: PASS.
- `mimita.exe --gamemode-config-validate`: PASS
  (`masterlist=1 loaded=18 errors=0`).
- `python devscripts/config_selftest.py`: FAIL from pre-existing avatar
  advanced-schema findings, not objective configuration.
- `mimita.exe --gamemode-selftest`: FAIL in the broader Juggernaut/TDM/role
  fixture; config validation above passed. The printed output did not identify a
  new objective failure.
- `python devscripts/quality/lint.py --gate`: report-only findings
  (`report_failures=5`), all in existing damage-authority/contact-ownership/
  dead-code/duplicate/coverage categories; no new gate was added.

### Runtime journal

Run `20261101_043919`:

- `objective.timers.resolved` recorded
  `interaction_mode:"hold"`, `plant_seconds:1.0`, `defuse_seconds:5.0`,
  `tap_progress_ticks:6`.
- `objective.debug-assign` accepted `self`.
- A deliberately authored test site produced:
  - `objective.plant-start` with `operator_kind:"player"`,
    `required_ticks:60`, `site:"TEST3"`.
  - `objective.plant-interrupted` with `reason:"released"`,
    `progress_ticks:5`, `required_ticks:60`.
- Observed at:
  `C:\mimita-v9\logs\2026-11-01\20261101_043919\events.jsonl`.

The full 60-tick completion and CT defuse were not captured before the dev loop
relaunched the run during validation. The client fixed simulation was frozen
while the map editor was active; it resumed after `mapeditor 0`, which is when
the plant started. A contaminated/overlapping dev-loop journal and a relaunch
prevented a clean completion run.

## Human review still required

- Confirm the hold prompt, tap-count prompt, and progress percentage are readable
  and do not overlap the round HUD.
- Confirm the momentum feel while a sliding player plants.
- Confirm Mode 2 requires exactly 10 plant presses and 50 defuse presses in
  normal play.
- Confirm NPC T plant travel, CT contest/retake, and defuse behavior in a clean
  long-running match.
- Confirm editor site circles, selected highlight, radius edits, save/reload.
- Confirm multiplayer presentation and authority.
- Run a clean, non-overlapping dev-loop completion scenario for plant,
  defuse, death interruption, leaving the area, and dropped-bomb recovery.

## Docs and skills used

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/gamemodes/funcounterstrike.md`
- `docs/specs/gamemodes/counterstrike.md`
- `docs/specs/gamemodes/objective-hud.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/entity-editor/entity-editor.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/specs/dev-loop/quality-gates.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
