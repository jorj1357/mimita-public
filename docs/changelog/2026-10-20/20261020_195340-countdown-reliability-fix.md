# Countdown reliability fix

Timestamp: 2026-10-20 19:53:40 EDT (-04:00)  
Branch: `20261011zt`  
HEAD at start: `0b5145b2` (`actor actor colisionsand ...`)

## Result

Implemented a monotonic wall-clock deadline for match countdowns, made client
countdown display and GO presentation independent of a regressing server tick,
and paused all server NPC decision, movement, physics, and actor-pair simulation
through COUNTDOWN and GO. The server continues networking and match phase
processing. Team-brain assignments/objective context also stop during those
phases. A rate-limited pause/resume event records the number of NPCs held.

The duration remains the active mode's configured countdown duration (three
seconds for standard matches). Custom mode countdown values were not replaced.

## Exact source changes

- `src/network/server-gamemode.h:148`, `ServerGamemodeState`: added
  `uint64_t countdownDeadlineMs = 0;` as the server's monotonic deadline.
- `src/network/server-gamemode.cpp:1069-1113`, countdown helpers:
  `countdownWallSecondsRemaining`, `countdownWallDeadlineReached`,
  `startCountdownWallClock`, and `logCountdownCompletion` replace tick-based
  countdown authority. `broadcastDuelState` now sends remaining wall duration
  at line 1170. `beginGamemodeCountdown`, `beginObjectiveRound`, and
  `beginMatchCountdown` set deadlines at lines 1778, 2345, and 3346. Objective,
  standard match, duel, and Bomb Tag transitions use the deadline at lines
  5488, 5694, 6022, and 6876. `serverTeamBrainTick` is gated at line 4930.
- `src/network/server-npcs.cpp:1191-1232`, `simulateSharedNpcs`: return during
  COUNTDOWN/GO after roster and health synchronization; emit one
  `npc.simulation.paused` and one `npc.simulation.resumed` event per pause.
  Removed countdown wakeup-timer holds at `finalizeServerNpcSpawn` and the
  narrower objective-only target gate. Pre-existing collision diagnostics in
  this file were preserved.
- `src/network/community-match-client.h:40,183-185` and
  `src/network/community-match-client.cpp:330-347,422-460,702-735,750-754`:
  maintain a local monotonic countdown/GO deadline, only shorten the countdown
  when a later snapshot reports less time, reject server-tick anchor regression,
  and retain GO through its wall-clock window.
- `src/engine/engine-tick-ui-overlays.cpp:781-790`: use remaining wall seconds
  and the local countdown-elapsed state for the shared match HUD.
- `src/duel/duel-queue.h:84-85,202-203` and
  `src/duel/duel-queue.cpp:737-789`: apply the same forward-only local deadline
  to the duel HUD and hold its GO label for the configured GO duration.
- `src/duel/duel-ui.cpp:197-204`: display the configured GO label when the
  duel countdown deadline expires.
- `config/gui/duel-match-hud.json:31-32`: added `"goText": "GO!"`.
- `docs/specs/networking/networking.md:12-27` and
  `docs/specs/gamemodes/gamemodes.md:49-55`: documented the wall-clock,
  forward-only countdown and all-NPC pause contract. The pre-existing personal
  notes at the top of `networking.md` were left in place.

Representative exact old/new content:

```text
src/network/server-gamemode.cpp, standard countdown transition
old: if (tick >= d.matchStartTick)
new: if (countdownWallDeadlineReached(d))

src/network/server-npcs.cpp, simulateSharedNpcs
old: NPC targeting/movement/physics ran during countdown.
new: if (phase == COUNTDOWN || phase == GO) log pause once and return.

src/network/community-match-client.cpp, server tick anchor
old: mServerTick = packet.serverTick;
new: if (packet.serverTick >= mServerTick) { mServerTick = packet.serverTick; ... }

src/engine/engine-tick-ui-overlays.cpp, match countdown
old: derive countdown from matchStartTick - serverTick.
new: derive it from countdownSecondsLeft() and countdownElapsed().
```

## Pre-existing working tree changes

The checkout was already dirty before this implementation, including
unrelated graphics, settings, movement/collision, documentation, and changelog
work. Those changes were preserved. The user's networking note and the
collision candidate/contact fields in `src/network/server-npcs.cpp` were also
present before the countdown edits and were preserved. No unrelated files were
included in the implementation scope.

## Documents and focused review

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/gamemodes.md`
- Routed NPC movement, debug logging, performance, time formatting, and
  code-ownership specifications
- `docs/operations/task-completion/task-completion.md`
- `docs/specs/dev-loop/dev-loop.md` and routed build/runtime-validation docs

Focused review result: PASS. The implementation keeps the existing
server-authoritative mode duration and packet fields, with one monotonic
deadline owner on the server and local presentation deadlines on clients.

## Validation evidence

- Canonical dev-loop incremental build succeeded and published build 2698
  (source generation 34, debug; 40 compiled, 521 skipped).
- Exact executable `C:\mimita-v9\.dev\builds\2698\mimita.exe --versioninfo`
  reported run `20261020_195340` and journal
  `logs/10-20-2026/20261020_195340/events.jsonl`.
- Real dev-loop runtime: run `20261020_235159`, journal
  `logs/2026-10-20/20261020_235159/events.jsonl`, build 2698. At
  `23:52:18.480Z`, `match.countdown.start` recorded 3 seconds. At
  `23:52:21.485Z`, `match.countdown.complete` recorded `late_by_ms=5` and
  `round.go`. `npc.simulation.paused` recorded `npc_count=67` and
  `living_npc_count=67`; `npc.simulation.resumed` followed after GO with
  `npc_count=67`. The journal showed no server `npc.*` events during that
  pause interval after the gate was entered.
- An earlier build-2696 live run exposed team-brain objective-context updates
  during countdown. The team-brain gate was then added and build 2698 runtime
  verified the pause event and 67 NPC count.
- `git diff --check` still reports trailing whitespace in pre-existing user
  notes in `docs/jorj-docs/20261020plan.md`, `docs/specs/actor-presets/actor-presets.md`,
  `docs/specs/networking/networking.md`, and `docs/specs/weapons/swordsword.md`;
  no whitespace cleanup was applied to those unrelated notes.
- Human visual/gameplay acceptance was not performed. The runtime journal proves
  the server phase timing and NPC simulation gate, not visual countdown quality
  on a user's screen.
