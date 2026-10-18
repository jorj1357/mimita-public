# Task: Stable Melee ground state and server exit diagnostics

- Date: 2026-10-18 11:55 EDT
- Branch: `20261011zt`
- Base commit: `702973f3506712aa6e37741b523b17a1dbbad8c8`
- Status: implemented and built; runtime diagnosis captured; root cause of the earlier server crash remains unresolved.

## Findings

- The Melee animation selector used raw `ground.onGround` to decide whether to play `fall`, although the spec and movement state already define `stableOnGround`. The selector and Melee attack grounded/airborne categorization now use the stable value.
- The live crouch sample shows both raw contact and the stable flag can become false. The configured stable-ground grace is 0.08 seconds (`src/config/movement-config.cpp:201`). The live movement trace shows a floor contact changes vertical velocity from about -2.96 to +1.04 m/s. `config/collision.json` enables bounce at strength 0.35 with a 0.01 m/s minimum speed and 0.1 minimum push; `2.96 * 0.35` is approximately `1.04`. The rebound can keep the actor airborne longer than the grace window, so the corrected selector still chooses `fall` while genuinely between floor contacts. Do not describe stable-ground selection alone as fixing the repeated physical bounce.
- Previous server PID 4160 ended with Windows exception `0x40000015` in `libstdc++-6.dll`; the associated dump is `C:\Users\guita\AppData\Local\CrashDumps\mimita.exe.4160.dmp`. This code is now named `STATUS_FATAL_APP_EXIT` in crash reports, but that status and module do not identify the fatal call. The dump still needs a debugger/symbolized stack to find its origin.
- Dedicated-server startup now installs the existing crash handler before `runServer`, and suppresses its modal dialog in server mode. Server startup, five-second heartbeats, known early/normal exits, and the latest loop activity are recorded with PID/run/room/mode/map context. A fatal crash cannot reliably write a final JSONL exit event; the crash report/minidump is the fatal-exit evidence, while the last heartbeat and breadcrumb identify the last known server state.

## Files and exact changes

- `src/entities/player-animation.cpp`: changed Melee's airborne test from `!ground.onGround` to `!ground.stableOnGround`; added `animation_source`, `locked_clip`, and raw/stable/crouch fields to state-transition events.
- `src/combat/melee-client.cpp`: grounded/airborne Melee move selection now reads `stableOnGround`; input events include both raw and stable fields.
- `src/physics/physics-mini.cpp`: added rate-limited `movement.ground_transition` records with previous/current collision ground, contacts, contact timers, position, velocity, and a suppressed-transition count while a Melee actor crouches.
- `src/network/client.cpp`, `src/network/multiplayer-interpolation.cpp`, `src/engine/engine-tick-render.cpp`: copy raw snapshot grounding into `stableOnGround` for remote and replay actors, since those snapshots do not carry the local temporal grace state.
- `src/debug/crash-handler.h/.cpp`, `src/main.cpp`, `src/main-init.cpp`: install crash diagnostics before server dispatch, suppress a blocking crash dialog for headless server mode, and label exception `0x40000015`.
- `src/network/server.cpp`: added `server.process.started`, five-second `server.process.heartbeat`, `server.process.exited` for known startup/normal returns, and server startup/loop crash breadcrumbs.
- `docs/specs/gamemodes/supersmashbrosmelee.md`: specifies that Melee move category and authored grounded/crouch pose selection use `stableOnGround`; raw `onGround` remains collision-owned.

## Validation and runtime evidence

- Dev-loop published build 2571 from source generation 4; running and latest builds matched at capture. No separate synthetic test was added.
- Live journal: `logs/2026-10-18/20261018_155223/events.jsonl`.
- The live agent command `agent.crouch 1`, snapshot, and `agent.crouch 0` completed. Snapshot label `stable_ground_check` recorded `on_ground=false`, `stable_on_ground=true`, `active_animation=crouch`, confirming the selector no longer drops to `fall` for a raw-only miss.
- The same journal recorded repeated ground-contact transitions while crouching. A sampled contact had one real world contact and vertical velocity changed from about `-2.96` to `+1.04 m/s`; five additional transitions were suppressed between bounded records. These are consistent with the configured floor bounce and explain why stable state later expires.
- Server heartbeat observed PID 15712 in room `SZFMR6K`, mode `melee`, map `cramped`, with one player and one NPC. The server remained alive during capture; no `server.process.exited` event was expected in this run.
- The repository Melee probe did not pass: its first `modestart 14` command timed out before later scenario steps. This is recorded as a scenario delivery failure, not a gameplay regression. Direct agent commands on the same later run did complete.
- Build evidence is compilation/linking only. Runtime evidence is limited to the journal above; visual acceptance was not performed.

## Follow-up

- Investigate why this Melee actor is bouncing from the floor and decide whether actor-to-world bounce should be disabled or separately tuned. Do not increase stable-ground grace as a substitute without deciding whether an actor in a real airborne gap should continue to count as grounded.
- Symbolize/analyze the saved PID 4160 dump to identify the fatal call. The new startup/heartbeat/breadcrumb path will make a future recurrence easier to correlate with its journal.
