# Scaled lethal-hit death audio

Date: 2026-10-05

## Change

- Added damage-scaled death audio for authoritative network damage events.
- The lethal hit damage controls pitch and volume.
- The local killer hears the sound with the killer multiplier.
- Other clients hear the same death at the observer multiplier.
- Removed the predicted NPC death sound so prediction cannot play an unscaled or duplicate sound before confirmation.
- Expanded the audio pitch clamp to support the requested low/high death-pitch range.
- Added hot-reloadable death-audio tuning fields to `config/audio/hitmarker.json`.
- Death scaling reaches its configured endpoints at `deathDamageForMaxEffect: 1000`.

## Owners

- `src/audio/hitmarker-audio.cpp`
- `src/network/multiplayer-projectiles.cpp`
- `src/network/multiplayer-shots.cpp`
- `src/combat/weapon-fire-damage.cpp`
- `src/audio/audio.cpp`
- `config/audio/hitmarker.json`

## Evidence

- Source patch applied successfully.
- `config/audio/hitmarker.json` parsed successfully.
- `git diff --check` passed for the changed paths.
- `python build_agent.py` completed successfully and linked `mimita.exe` (final pass compiled `src/audio/hitmarker-audio.cpp`).
- Affected object files were newer than their edited sources after the build.
- Live multiplayer and human audio acceptance remain pending.
