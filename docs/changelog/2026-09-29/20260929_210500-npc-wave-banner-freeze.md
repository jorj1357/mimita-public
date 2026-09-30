# NPC wave banner spawn and freeze timing

Time: 2026-09-29 21:05:00 EDT

## Summary

Fixed the apparent three-second NPC freeze in `npc_waves`. The old lifecycle
waited for the `WAVE N` banner to finish before creating the NPCs, so they were
not actually present during the banner. NPCs are now spawned during the banner
and the authoritative server suppresses their target selection, movement, and
fire simulation until the banner ends.

## Configuration

- `config/npc-difficulty.json` now contains `freezeDuringWaveBanner`.
- `true` keeps wave NPCs still and silent while the banner is visible; `false`
  allows them to act immediately. The setting hot-reloads.
- `spawnFireDelayMinTicks` and `spawnFireDelayMaxTicks` remain the separate
  first-shot delay controls in this file.
- `wave_banner_seconds` remains in `config/gamemodes/npc_waves.json` because it
  controls match presentation/lifecycle duration, not NPC difficulty.

## Evidence

- `python build_agent.py`: `BUILD SUCCESS`, return code 0; one changed C++
  translation unit was compiled and the executable linked successfully.
- `git diff --check`: passed for the changed source/config files; only normal
  line-ending warnings were reported.
- A direct headless launch attempt was blocked by the local Windows executable
  access state, so connected-client visual acceptance was not claimed.
- Existing unrelated edits in `config/npc-difficulty.json` were preserved.

## Human validation still required

Join a local `npc_waves` match and verify that NPCs are visible during `WAVE N`,
remain still and do not shoot while `freezeDuringWaveBanner` is true, then move
and shoot when the banner disappears. Change the boolean and save the JSON to
verify hot reload.
