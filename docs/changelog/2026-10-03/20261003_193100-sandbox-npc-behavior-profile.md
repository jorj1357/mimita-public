# Sandbox NPC behavior profile selector

Date: 2026-10-03 19:31 EST
Branch: `afad20a-rebuild`
Result: PASS_WITH_HUMAN_REVIEW

## Request

Allow Sandbox to select one NPC combat behavior profile from JSON so all
unassigned Sandbox NPCs use `balanced`, `aggressive`, `nervous`, or another
profile defined in `config/behavior-profiles.json`, without changing human
players or role-specific NPCs.

## Changes

- `config/gamemodes/sandbox.json`: added the NPC-only hot-reloadable
  `npc_behavior_profile` setting, defaulting to `balanced`.
- `src/gamemode/gamemode.h` / `src/gamemode/gamemode.cpp`: added and parsed
  `Gamemode::npcBehaviorProfile`.
- `src/network/server-gamemode.cpp`: resolves the mode profile only for NPC
  actors as a fallback; role-specific behavior profiles still override it.
  Unknown profile ids warn and fall back to role/default behavior.
- `src/network/server-npcs.cpp`: refreshes living unassigned NPCs when the
  active mode profile changes, and reapplies difficulty tuning so aggression
  changes take effect without respawn.

## Usage

Edit this value in `config/gamemodes/sandbox.json`:

```json
"npc_behavior_profile": "aggressive"
```

The value must match a profile `id` in `config/behavior-profiles.json`.

## Validation

- `python build_agent.py`: BUILD SUCCESS; one affected translation unit
  compiled and the executable linked.
- `mimita.exe --gamemode-selftest`: PASS.
- `mimita.exe --npc-movement-policy-selftest`: PASS.
- `git diff --check`: no whitespace errors in the files changed for this task;
  existing unrelated whitespace warnings remain in `docs/specs/20261003plan.md`.

## Human review still required

Start Sandbox, change `npc_behavior_profile` from `balanced` to `aggressive`
or `nervous`, save the file, and confirm the live NPCs change their combat
behavior. Visual/gameplay confirmation was not performed in this session.

## Pre-existing work preserved

The worktree contained unrelated edits in account, analytics, NPC movement,
navigation, difficulty, weapon, documentation, sound, and changelog files.
They were not modified or reverted by this task.
