# In-game music player format audit and implementation

- Task: Diagnose unsupported in-game music, finalize the player contract, and align its implementation.
- Status: IMPLEMENTED_WITH_BUILD_BLOCKER; source/config/spec updates are in place, but the current dev-loop build fails in unrelated networking code and runtime playback remains unverified.
- Date, time, timezone: 2026-10-31 (UTC; work began 2026-10-30 EDT).
- Branch: `20261011zt`.
- Base and final commit: `a7e5285a74a6ae8fe52aa4ced7dd39ecd2a10c98` (unchanged; working changes uncommitted).

## Findings and accepted behavior

The existing music notes did not define a complete player contract. Source tracing showed that MP3 and WAV playback went directly to miniaudio while Opus files required FFmpeg conversion to cached WAV. The selected bundled `tools/ffmpeg.exe` has an Opus decoder but no Ogg demuxer. All 27 current `.opus` assets use Ogg containers, so the bundled decoder rejected them. A system FFmpeg found on PATH decoded 26 of 27; one asset appears malformed. The available system binary is substantially larger than the bundled one, so it was not copied into the repository.

The user confirmed the intended format scope: MP3, WAV, Opus, OGG, FLAC, AAC/M4A, and other common FFmpeg-readable formats. They accepted filename order with looping for Linear, shuffle bags that play every playable track once without repeating at a bag boundary, and Previous/Next navigation through playback history. Mode-configured music is the default; users may mute it or use their personal in-game playlist.

## Changes

- Expanded [music.md](../../specs/music/music.md) into an implementation-ready contract while preserving its original notes. It defines formats/decoder expectations, error skipping, linear and shuffle behavior, history navigation, settings persistence, and game-mode versus personal music behavior. It records the packaged-FFmpeg limitation.
- Updated `MusicManager` to discover the supported extension set, sort linear playlists case-insensitively, manage shuffle bags and playback history, and decode non-direct formats into a cache on a worker thread. Failed tracks are skipped with categorized, rate-limited StructuredLogger events.
- Added persistent playlist-mode and personal-music settings, plus controls in the existing music widget and terminal commands for order and personal/mode selection.
- Changed shared FFmpeg discovery to prefer an explicit configured path, then a system binary on PATH, then the packaged fallback. This also affects replay/video FFmpeg use.
- Preserved the pre-existing local change to `config/audio/music-settings.json` (`musicVolume=0.1`).

## Validation and evidence

- Asset/codec inspection: confirmed the bundled FFmpeg Ogg demux gap and attempted decode of all 27 current Opus files; the system FFmpeg decoded 26 and rejected one malformed file.
- Configuration validation: `config/gui/music-widget.json` parses successfully.
- Build: requested via the canonical `python devscripts/dev-loop.py --request build --wait-for-build`; it failed. The latest logged compiler errors are in `src/network/server-packets.cpp` at lines 633 and 816, where `AabbTree` is incomplete. The latest log showed `src/audio/music-manager.cpp` compiling without a reported error. No new executable was produced for this source generation.
- Runtime: not run because the build failed. The already-running dev-loop game/server were left untouched; their executable predates these changes. No current-source `--versioninfo`, `EVENTS_JSONL_PATH`, or live music journal evidence exists.
- Human acceptance: listening and UI interaction remain unverified.
- Tests: no tests were added or run.

## Portability limitation

The implementation supports FFmpeg-readable formats when a full FFmpeg is available through the explicit setting or PATH. The bundled fallback remains small and lacks Ogg demuxing, so Opus-in-Ogg and some other formats may still fail on installations without a full FFmpeg. The malformed Opus asset also needs replacement or repair. Making the fallback fully capable requires a separate packaging decision about binary size or a compatible decoder dependency.

## Documentation review notes

The router-referenced `docs/doc-review-09-03-2026.md` and `docs/features/live-code-development/live-code-development.md` were absent in this checkout; their available skill/workflow substitutes were used. Music-related MIDI/fade TODOs in `music-midi.md`, `music-fadein.md`, and the Zombie Tower spec were left unchanged because they are separate authoring and transition specifications.
