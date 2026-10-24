# Extend the room registration wait

- Status: `PASS_WITH_HUMAN_REVIEW`
- Recorded: 2026-10-23T22:28:20Z (2026-10-23 18:28:20 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD: `2103321b`
- Request: increase the hosted room creation wait from 15 seconds to 60 seconds, preserve automatic room-code join, and investigate why Zombie Tower startup takes longer.
- Pre-existing work observed: `config/accounts/default.json`, `docs/jorj-docs/20261023plan.md`, `docs/specs/open-gameplay-issues.md`, and `docs/changelog/2026-10-23/20261023_182134-clarify-gameplay-issues-intake.md` had existing user/task edits. `config/lighting.json` appeared modified later during this work. These files were not edited here except the requested additions to `open-gameplay-issues.md`; unrelated edits and all running game/server processes were preserved.

## Source changes

### `src/gui/gui-main.cpp`

- `pollPendingServerRoomCode()`, former line 681, had:

```cpp
constexpr uint64_t ROOM_CODE_TIMEOUT_MS = 15000;
```

- It now uses 60 seconds and emits one structured event when the room-code wait starts, completes, or times out. Each event includes process ID, map/mode, and elapsed/budget milliseconds where applicable.

```cpp
constexpr uint64_t ROOM_CODE_TIMEOUT_MS = 60000;
```

### `src/network/server-ice.cpp`

- `initServerIceListener()` had no structured duration breakdown for room-host setup. Added one bounded `network.server-ice-startup.stage` event for each completed TURN credential request, ICE candidate gathering attempt, and coordinator room-registration request. Fields include stage, duration, result, process ID, map, and mode. These events are to separate possible causes on the next runtime attempt; they do not assert that one stage is the cause before the new run is captured.

## Issue intake

### `docs/specs/open-gameplay-issues.md`

- Added the room-timeout and no-auto-join report under the 2026-10-23 intake. It records the requested 60-second wait, the measured startup-to-registration interval, map load and NPC navigation timings, and that the ICE/coordinator contribution remains to be measured. It notes build 2908 and that live auto-join acceptance is pending.

## Investigation evidence

- The GUI wait starts immediately after `CreateProcessA` successfully starts the server. It polls for the room file and, at the old 15-second deadline, shows `Room creation timed out`, deletes the pending file, and clears the wait state.
- `runServer()` loads the map, prepares startup NPCs, then calls `initServerIceListener()`; only after ICE succeeds does it register the room and write the room code to `--room-file`. This ordering permits the room-registration step to occur after the GUI's old deadline.
- Existing live journal: `logs/2026-10-23/20261023_221303/events.jsonl`. For the Zombie Tower server, `server.process.started` was at `2026-10-23T22:20:47.066Z` and `network.room.registered` at `2026-10-23T22:21:20.002Z`, a 32.9368-second interval. The same process logged headless map load from t=0.2580s to t=11.1901s (10.9320s), and NPC navmesh prepare/bake failure `recast_grid_out_of_bounds` at about t=16.476s. Room registration followed about 16.46 seconds later. This run establishes the long delay and a navmesh failure, but does not isolate the remaining time or prove a GUI timeout/no-join occurred in that exact run.
- ICE setup source shows a candidate-gather loop of up to 15 seconds, plus coordinator HTTP calls with 5-second request timeouts. These are plausible contributors, not a confirmed attribution for the observed 16.46-second tail.

## Validation and remaining review

- `git diff --check -- docs/specs/open-gameplay-issues.md src/gui/gui-main.cpp src/network/server-ice.cpp`: passed.
- Canonical build through the existing dev-loop profile `npc-navigation`: succeeded; published build 2908. The running build 2906 session was left in place.
- Exact executable identity check: `C:\mimita-v9\.dev\builds\2908\mimita.exe --versioninfo` reported `RUN_ID=20261023_182705` and `EVENTS_JSONL_PATH=logs/10-23-2026/20261023_182705/events.jsonl`.
- No new server or client scenario was launched because an existing user session was running; no process was stopped. Therefore build and executable identity are proven, while the 60-second auto-join behavior, new timing events, and player-visible result need a live Zombie Tower run on build 2908.
- Focused documents read: `docs/ROUTER.md`, `docs/specs/networking/networking.md`, `docs/specs/open-gameplay-issues.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, `docs/regressions/README.md`, and the related archived room-code regression. The router points to `docs/regressions/regressions-v1.md`, which is absent at that path; archived regressions were treated as historical only.
