# NPC spawn action delay tuning

Time: 2026-09-29 21:03:00 EDT

## Implemented

- Added `spawnActionDelayTicks` to `config/npc-difficulty.json`, defaulting to
  `1` fixed 60 Hz tick (about 0.017 seconds).
- The setting is applied on every NPC create and respawn through the shared NPC
  spawn finalization path.
- While the counter is active, the authoritative server blocks target
  acquisition, movement, and shooting together.
- Existing `spawnFireDelayMinTicks` / `spawnFireDelayMaxTicks` remain the
  separate randomized first-shot timing controls.
- Existing `freezeDuringWaveBanner` remains an additional `npc_waves` banner
  gate; it is currently `false` in the working configuration, so the new
  one-tick action delay is the only spawn action delay outside the banner.

## Validation

- Changed C++ translation units compiled during `python build_agent.py`.
- Final link was blocked by unrelated missing command-registration symbols:
  `registerEditorCommands`, `registerDuelCommands`, and
  `registerActorCommands`.
- `git diff --check` and JSON validation remain required before the next cold
  build; no running game process was stopped or restarted.
