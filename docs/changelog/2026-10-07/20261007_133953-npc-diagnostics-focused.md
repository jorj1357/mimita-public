# Focused NPC diagnostics

Date: 2026-10-07

## Outcome

Added bounded diagnostics for the current NPC investigation and narrowed
`config/debuglogger.json` to movement, objective, grenade, world, and essential
executable logging. No NPC decision or navigation behavior was changed in this
slice.

## Source changes

- `src/npc/npc.cpp` now keeps route-failure events edge/rate limited, moves
  repeated wall/replan/jump details to `Verbose`, and emits one
  `npc.stuck-episode` summary after recovery with displacement, wall-avoid,
  recovery, and jump counts.
- `src/npc/npc.h` and the server respawn path hold/reset the bounded diagnostic
  state.
- `src/network/server-gamemode.cpp` emits `npc.objective-context` changes and
  explicit `objective.plant-interrupted` / `objective.defuse-interrupted`
  events.
- `src/npc/npc-combat.cpp` emits `npc.grenade-thrown` after the NPC grenade
  fire path executes; `src/network/server-npcs.cpp` emits
  `npc.grenade-projectile-created` when the authoritative projectile is made.

## Logger profile

- Enabled at Important: `npc_movement`, `duel`, `grenade_launcher`, `world`.
- Disabled for this investigation: general, physics, performance, avatar,
  network, npc_combat, ragdoll, and unrelated categories.
- Console output is disabled; events continue to use the canonical
  `events.jsonl` journal.

## Validation

- `config/debuglogger.json` parsed successfully.
- Canonical build: `python build_agent.py` — SUCCESS; 2 compiled, 547 skipped;
  executable `mimita-20261007Tnpc-diag-v1.exe`.
- `--versioninfo` recorded
  `logs/10-07-2026/20261007_133942/events.jsonl` for the final executable.
- Real headless scenario: Dust2, counterstrike, 2 NPCs, 3 seconds, exited
  cleanly. The focused journal was 16,206 bytes / 23 events, containing six
  one-second movement snapshots, three stuck-episode summaries, and no
  per-tick wall/replan flood.
- Grenade and plant/defuse events were not exercised by this no-player smoke
  run; their source emitters are compiled but still require a combat/objective
  scenario for runtime confirmation.

## Evidence boundary

This proves the diagnostics compile, load, and keep a real NPC run bounded. It
does not yet prove that NPCs avoid every wall, plant, defuse, or use grenades;
the next investigation should run a populated Counter-Strike round and use
the first missing event in the movement/objective/grenade chains as the fix
target.
