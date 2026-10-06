# Opus music playback support

- Updated `src/audio/music-manager.cpp` to discover `.opus` music files and to use the existing FFmpeg resolver to decode them into cached temporary WAV files before miniaudio playback.
- Aligned music discovery and credits loading with the active `assets/music/` layout, including `mainmenu/` and `ingame/`.
- Corrected the asset-management documentation to describe the active music layout.
- Updated `src/audio/music-manager.h` with the Opus conversion cache and playback-path resolver.
- Rebuilt the active in-game playlist during `music_reload`, added failover across unusable tracks, and confirmed one attached file is malformed while the other 26 Opus files decode successfully.
- Validation: `python build_agent.py` completed with `BUILD SUCCESS`; FFmpeg successfully decoded `assets/music/ingame/donttrack/i know i.opus` to PCM WAV in a focused check.
- Human audible playback in the newly built executable remains to be confirmed; the currently running `.dev` client must be relaunched from the new build.
