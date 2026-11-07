# Juggernaut Horde rules and loading feedback

- Recorded: 2026-11-07T17:43:03Z (12:43:03 EST)
- Branch: `20261011zt`
- HEAD: `839322c04ef8ed95717609aba1a519ee4aa6ddfa`
- Result: `PASS_WITH_HUMAN_REVIEW` for specification/config changes; `BLOCKED` for final runtime verification and Retry/Cancel loading actions.

## Scope and pre-existing work

This work preserved the dirty worktree. Existing edits include avatar JSON, user gamemode masterlist/avatar-group edits, multiple unrelated configs/specs/rendering files, the prior Rocket Duel setup, and previous changelogs. No unrelated changes were reverted. The `competitive_no_headless` membership and `monster*` deny policy were kept semantically unchanged. The canonical gamemode sync added the nullable popup field to every gamemode as required by the masterlist; existing values were otherwise retained by the sync tool.

## Desired behavior and changes

- `docs/specs/gamemodes/juggernaut.md` now links a dedicated `juggernaut-horde.md`, records the selected presentation values for both modes, and removes its Horde variant section.
- `docs/specs/gamemodes/juggernaut-horde.md` defines the existing 64 Fighter / four Juggernaut roster, Fighter next-tick respawn, Juggernaut elimination, timeout, team targeting, 10% Juggernaut max-health healing per kill, unchanged +0 gold / +10 XP NPC reward, and runtime evidence expectations.
- `config/gamemodes/masterlist/gamemode-masterlist.json` adds nullable `presentation.gold_reward_popup`; `null` inherits existing behavior and a concrete boolean affects only popup gold-line visibility. All active gamemode JSON files gained the field through `gamemode_json_sync.py --write`.
- `config/gamemodes/juggernaut.json` and `jugger_horde.json` now enable player outlines, turn friendly outlines off, and use red enemy outlines at 0.25 opacity. Other presentation values and effect scales remain unchanged. Horde sets `gold_reward_popup: true`; the Juggernaut base leaves it null.
- `src/gamemode/gamemode.h` and `.cpp` parse the nullable popup override. `src/gui/hud/reward-popup.h` and `.cpp` retain default behavior unless a mode supplies an override, and conditionally hide only the gold line. `src/network/multiplayer-tick.cpp` passes the active mode policy at both reward event paths; reward calculations remain unchanged.
- `docs/specs/networking/networking.md` defines Loading versus Reconnecting, elapsed time, per-step duration logging, watchdog behavior, and the aspirational 10 ms target. `src/network/multiplayer-packets.cpp` displays Loading with the current connection/map/spawn stage and elapsed seconds/milliseconds before readiness. Existing connection-stage logging is reused; `src/engine/engine-tick-net.cpp` and `engine-tick-state.cpp` report map-load durations through that logger.
- `devscripts/dev-launch-modes.json` Horde launch mode 44 no longer passes `--no-npcs`, which prevented the requested NPC combat scenario from having NPCs.

## Validation and evidence

- `python devscripts/gamemode_json_sync.py --write`: completed; canonicalized all modes, preserving their declared values.
- `python devscripts/gamemode_json_sync.py --check`: `PASS`.
- `git diff --check` on focused files: `PASS`. Repository-wide `git diff --check` also reports pre-existing trailing whitespace in the gamemode masterlist comments, movement config, and open-gameplay notes.
- `python devscripts/config_selftest.py`: `FAIL` on pre-existing avatar schema issues across avatar JSON files (missing normalized keys and advanced-part fields); it reported 22 gamemodes. This is unrelated to the changed gamemode schema.
- Dev-loop status at start had no active build and the latest successful artifact was from 2026-11-07T17:16:25.830136Z. A build was requested through dev-loop. Its compiler phase reported `BUILD SUCCESS` (67 compiled, 551 skipped, 111.73 s), but artifact publication failed with `[Errno 28] No space left on device`; the daemon then failed writing its state for the same reason. The source changed during this build (active generation 41; worktree advanced through generation 63), so it does not prove the final code compiled or provide an executable for the runtime run. No existing build artifact was deleted.
- No Horde `events.jsonl` is present in the checked-in logs. Therefore NPC team assignment, target selection, attack gates, firing, damage, client receipt, slowdown correlation, and reconnect transitions remain unverified. No cause was guessed and no NPC targeting code was changed without that evidence.
- Rocket Duel checks are static only: `rocket_duel.json` references `competitive_no_headless`, uses `keep_per_npc`, and actor preset `duel_standard.json` denies `monster*`. No respawn identity runtime observation was made.
- Human in-game presentation review was not performed. Loading Retry/Cancel controls were not implemented; the new UI status/timer and map-duration diagnostics are source changes only.

## Focused documents reviewed

- `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/gamemodes/juggernaut.md`
- `docs/specs/networking/networking.md`
- `docs/specs/performance/performance.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/specs/dev-loop/quality-gates.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/README.md`, networking and performance regression indexes

## Remaining review

Free disk space without deleting user data, then use dev-loop for a fresh build and launch mode 44. Inspect the exact Horde server/client journals for team assignments, opposing-team target selection, attack gate, firing, applied damage, tick/NPC work, frame windows, snapshot age, and connection transitions. Verify Loading stage/timer and no initial Reconnecting, test the map-stall Retry/Cancel actions after implementing them, and observe Rocket Duel avatar retention across an NPC death/respawn. Human visual review remains required for the presentation settings.
