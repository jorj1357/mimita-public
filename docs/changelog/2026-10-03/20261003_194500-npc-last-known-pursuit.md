# NPC last-known-position pursuit

Date: 2026-10-03 19:45 EST
Branch: `afad20a-rebuild`
Result: PASS_WITH_HUMAN_REVIEW

## Request

Make NPC pursuit use the JSON-controlled perception memory window and keep
following a player toward the last position where the player was seen after
the player moves behind cover.

## Changes

- `src/npc/npc-state-machine.cpp`: removed the duplicate hard-coded 8-second
  search timeout. Memory pursuit now lasts until `perceptionMemoryTicks` in
  `config/npc-difficulty.json` expires.
- `src/npc/npc-states.cpp`: hidden-target Chase now uses
  `targetMemory.lastKnownPosition` instead of defaulting to the +X direction.
- `config/behavior-profiles.json`: added documented pursuit settings for
  `forget_when_hidden`, `last_known_position`, and `persistent` behavior,
  cover continuation, last-known pursuit, and post-arrival behavior.
- `src/npc/npc-behavior.*` and `src/npc/npc.cpp`: profile pursuit settings are
  parsed, hot-reloaded, and applied to living NPCs without respawn.

## Usage

The memory duration remains here, in fixed 60 Hz ticks:

```json
"perceptionMemoryTicks": 1800
```

That is about 30 seconds because `1800 / 60 = 30`.

Choose pursuit behavior in `config/behavior-profiles.json`:

```json
"pursuit_mode": "persistent",
"continue_through_cover": true,
"pursue_last_known_position": true,
"after_reaching_last_known": "look_around"
```

## Validation

- `python build_agent.py`: BUILD SUCCESS; executable linked.
- `mimita.exe --npc-perception-selftest`: PASS.
- `mimita.exe --npc-search-behavior-selftest`: PASS.
- `mimita.exe --npc-movement-policy-selftest`: PASS.
- `mimita.exe --gamemode-selftest`: PASS.
- `git diff --check`: only pre-existing whitespace warnings remain in
  `docs/specs/20261003plan.md`.

## Human review still required

Start Sandbox, set its NPC profile to `aggressive`, save the profile or mode
JSON, then walk behind a wall. Confirm the NPC keeps routing toward the last
seen position and that changing `perceptionMemoryTicks` changes how long it
continues after sight is lost.

## Pre-existing work preserved

The worktree contained unrelated edits in account, analytics, NPC movement,
navigation, difficulty, weapon, documentation, sound, and prior changelog
files. They were not reverted.
