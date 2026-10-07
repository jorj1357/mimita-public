# Counter-Strike P3 slice: reachable goals, carrier objective, observable rounds

- Status: `P3_PARTIAL_VERIFIED_WITH_HUMAN_REVIEW_PENDING`
- UTC timestamp: `2026-10-06T21:48:17Z`
- Display timezone: `America/New_York`
- Branch: `afad20a-rebuild`
- Scope: P3 from `docs/architecture/player-npc-systems/npc-agent-work-20261006.md`
  (NPC behavior + core round/bomb loop). Adds navigation-reachable travel
  goals, fixes a bomb-carrier objective bug, makes the round/bomb lifecycle run
  and be observable in a controlled headless match, and turns on the round
  diagnostics category. No production-authority or physics changes.

## Findings that shaped the slice

- `--gamemode counterstrike` does NOT start Counter-Strike. The community mode
  is selected only by `--mode counterstrike` (`src/network/net_mode.h:45-46`,
  `net_mode.cpp:73-76`, `server.cpp:502-505`). Prior "CS" runs were Sandbox.
- The round roster requires a connected active human
  (`src/network/server-gamemode.cpp:3712-3751`), so a headless run never built
  teams or ran rounds.
- The `duel` StructuredLogger category was `off`
  (`config/debuglogger.json:52`), so every round/objective event was silently
  dropped from `events.jsonl`.

## Source changes

- `src/npc/npc-navigator.h` / `.cpp`
  - `resolveExploreTarget` now takes the `World` and uses a new
    `findReachableTravelPoint` helper: the synthesized Explore target must land
    on a real floor inside map bounds with a clear ray from the actor; the
    heading fans out in 30-degree steps and falls back to shorter distances.
    This replaces the old unvalidated `body.pos + dir*travel` point that landed
    in walls / off the map (P2 evidence: `dest_projection_m` ~10.6 m).
- `src/npc/team-brain.h` / `.cpp`
  - `objectiveTargetPosition(actorId, actorPos, out)` plus `nearestSitePosition`.
    A bomb-carrying Terrorist is now sent to the nearest site to plant instead
    of being pointed at its own position (the previous behavior made
    `atObjective` immediately true and suppressed movement). Supporters still
    follow the carried bomb; planted site still wins.
  - Added `carrier_target` self-test coverage.
- `src/network/server-gamemode.cpp`
  - Pushes the per-actor objective target (`npc.id`, `npc.body.pos`).
  - `MIMITA_CS_HEADLESS_MATCH` (strictly env-gated, default off) allows a
    round-mode match to run with NPCs only: `buildObjectiveRoster` fills both
    teams to capacity with no human, and the intermission/round-start gates
    proceed. Production behavior is unchanged when unset.
  - Added bounded events `round.start`, `round.go`, `round.active`,
    `objective.carried`.
- `config/debuglogger.json`
  - `duel` category `off` -> `important` so round/objective lifecycle events
    reach the canonical `events.jsonl`.

## Build evidence

- `mimita-20261006T-cs-goal-v6.exe` (reachable Explore targets): SUCCESS.
- `mimita-20261006T-cs-objective-v7.exe` (carrier objective): SUCCESS.
- `mimita-20261006T-cs-round-v8.exe` (headless match): SUCCESS.
- `mimita-20261006T-cs-round-events-v9.exe` (+ lifecycle events): SUCCESS.
- Command: `MIMITA_EXE_NAME=<name> python build_agent.py`.

## Runtime evidence

### Goal selection (measured)

- v5 baseline journal: `logs/10-06-2026/20261006_173922/events.jsonl`.
- v6 journal: `logs/10-06-2026/20261006_173924/events.jsonl`.
- Command (both): `--server --bind 127.0.0.1:0 --mode counterstrike --map
  dust2cyberiav4 --npcs 2 --timeout 8 --no-map-rotation --no-discord-notification`.
- `npc.movement-decision` positive `net_progress_toward_goal` samples:
  v5 = 2/14 (14%), v6 = 10/16 (62%).
- With `MIMITA_NPC_NAV_COMPARE=1`, Recast `dest_projection_m` fell from ~10.6 m
  to 0.20 m for 13/16 queries, and those routes succeeded.
- `npc.stuck` was 157 (v5) vs 170 (v6), i.e. within noise, not a regression.

### Objective behavior and round loop (controlled headless run)

- Journal: `logs/10-06-2026/20261006_174544/events.jsonl`.
- Command: `MIMITA_CS_HEADLESS_MATCH=1 mimita-...v9.exe --server --bind
  127.0.0.1:0 --mode counterstrike --map dust2cyberiav4 --npcs 0 --timeout 25
  --no-map-rotation --no-discord-notification`.
- Event chain: `objective.carried` (carrier 100018, team 1) -> `round.start`
  (round 1, countdown 3 s, wins 0-0, to-win 8) -> `round.go` (tick 240) ->
  `round.active` (round_end_tick 7140).
- Roster: 5 CT (ids 100000-100004, team 0) + 5 T (ids 100005-100009), all
  `preset=counter_strike`, `profile=rage2`, proving teams/roles apply.
- The bomb carrier navigated toward site B with positive net progress
  (goal `(-820,90,2339)` from spawn near `(-600,26,2366)`).

### Known remaining gap (observed)

- In a 70 s run (`logs/10-06-2026/20261006_174632/events.jsonl`) the carrier
  reached ~12 m from site B (`pos ~(-808,89,2354)` vs site `(-820,90,2339)`,
  radius 4 m) and then stalled with repeated `npc.stuck` (442 for that actor).
  No plant/defuse occurred. Reaching the site center, stuck recovery, and
  site-anchor/surface alignment remain open.

## Focused checks

- `--counterstrike-acceptance-selftest`: PASS (including `team-brain` with the
  new carrier-target case).
- Every inspected journal parsed as valid JSONL.
- Build success reported separately from runtime behavior; human visual review
  not performed (no GUI client).

## Pre-existing work preserved

Unrelated modified/deleted/untracked files from earlier work were not reset,
cleaned, deleted, or rewritten.

## Remaining P3 work (not claimed complete)

- NPCs reach a site but do not reliably stand inside it to plant/defuse; stuck
  recovery near objectives and site-anchor/surface alignment need work.
- TeamBrain assignments (AttackSite/DefendSite/Rotate) still do not distribute
  actors across different sites (anti-stacking) or expose per-actor sites.
- Combat aim/reaction/weapon-selection tuning and combat interruption +
  objective resumption were not changed.
- Timers (`freeze_seconds`, exact 3-2-1 presentation) and hold-F interaction
  remain spec gaps.

## Human review still needed

Run Counter-Strike with a real client and confirm the round flow, team
spawning, and NPC objective travel look correct. Machine evidence proves the
lifecycle ran and the carrier moved toward a site; it does not prove a
successful plant or acceptable gameplay.
