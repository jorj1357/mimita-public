# NPC wakeup delay and per-wave count

Time: 2026-09-29 21:31:00 EDT

## Implemented

- Traced the long spawn pause to `Npc::Npc` assigning a hardcoded
  `wakeupTimer = 3.0f`; `updateOneNpc` returned before physics, movement,
  targeting, and combat while that timer was active.
- Removed the hardcoded three-second wakeup and made the existing lifecycle
  use `npc-difficulty.json` `spawnActionDelayTicks` for every NPC create and
  respawn. The current value is 1 fixed 60 Hz tick.
- Removed the duplicate server-side action-delay counter so the configured
  delay is applied exactly once.
- Added `npcs_per_wave` to `config/gamemodes/npc_waves.json`. With value 10,
  wave targets are 10, 20, 30, and so on. The older start/increment fields
  remain as a fallback when `npcs_per_wave` is 0.

## Evidence

- `config/npc-difficulty.json` and `config/gamemodes/npc_waves.json` parsed
  successfully.
- `python build_agent.py` completed with `BUILD SUCCESS`, compiled 40 units,
  linked successfully, and returned code 0.
- `git diff --check` passed for the changed files; only normal line-ending
  warnings were reported.
- Connected-client visual acceptance was not performed.
