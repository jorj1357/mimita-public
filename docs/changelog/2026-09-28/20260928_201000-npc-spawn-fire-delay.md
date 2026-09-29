# NPC spawn fire delay

- Timestamp: 2026-09-28 20:10:00 America/New_York
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Add an NPC first-fire delay controlled by `config/npc-difficulty.json`, with a minimum of 1 server tick and a maximum of 60 server ticks.

## Implemented

- Added `spawnFireDelayMinTicks` and `spawnFireDelayMaxTicks` to the NPC difficulty settings, defaulting to `1` and `60`.
- The range is interpreted as fixed 60 Hz server ticks and selected inclusively per NPC life.
- The selected delay is applied to initial NPC construction and server-authoritative respawn.
- Existing `fireDelayMin`/`fireDelayMax` behavior for delays between ordinary shots is unchanged.
- Invalid values are normalized so the minimum is non-negative and the maximum is never below the minimum.

## Evidence

- Source owner: `src/npc/npc-difficulty-config.cpp` parses, validates, logs, and saves the settings.
- Spawn owner: `src/npc/npc-spawn.cpp` converts the randomized tick value to the shared attack cooldown during construction.
- Respawn owner: `src/network/server-npcs.cpp` reapplies the randomized delay on server respawn.
- Build: `python build_agent.py` completed successfully at 2026-09-28 20:04:24 with 1 file compiled, 496 skipped, link successful, return code 0.
- `git diff --check` found no new whitespace issue; the only reported issue is pre-existing trailing whitespace in `config/movement/movement-heavy.json` line 75.

## Remaining acceptance

Runtime/human acceptance remains open. Using the updated build, spawn and respawn NPCs and verify that each first shot occurs after an inclusive 1–60 server-tick delay. Existing running MiMITA processes were not stopped or restarted.
