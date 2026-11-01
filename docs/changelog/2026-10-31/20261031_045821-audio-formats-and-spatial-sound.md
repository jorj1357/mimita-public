# Audio formats and spatial world sound

- Task: Make `assets/music/ingame` playable across common formats and apply
  smooth, category-based spatial audio to 3D world sounds.
- Status: IMPLEMENTED_WITH_RUNTIME_EVIDENCE; final human listening review is
  still required.
- Follow-up: legacy mode is now the default, and all categories stay at full
  volume through 50 m before dropoff. The first implementation used spatial
  mode by default and a 10%-at-50-m weapon curve.
- Date, time, timezone: 2026-10-31T04:58:21Z; follow-up validated at
  approximately 2026-10-31T15:17Z.
- Branch: `20261011zt`.
- Base commit observed at start: `a7e5285a`.
- HEAD at record time: `7dd748c0` (advanced by other work during the session).
- Final commit: unchanged; all session work is uncommitted.

## Pre-existing changes

The working tree already contained many unrelated edits, including the large
in-progress `src/physics/movement/actor-triangle-solver.cpp` changes. Those were
not authored or claimed by this session. The only edit made there was replacing
four literal `\n` escape sequences in code with real line breaks so the file
compiles. Other pre-existing edits (weapons, collision, juggernaut, destructible
world, config files) were preserved.

## Specification alignment

- Finalized `docs/specs/music/music.md` from an implementation draft into the
  current music-player contract: format scope, case-insensitive discovery,
  linear/shuffle/history, mode versus personal music, corrupt-track skipping,
  FFmpeg selection order, and packaged-decoder limits.
- Finalized `docs/specs/music/music-sounds.md` as the world-sound spatial
  contract: all 3D world sounds, category profiles, gunshot reference points,
  directionality, distance tail, obstruction/enclosure/muffling/reverb, UI and
  music exclusion, centralized settings, live application, and diagnostics.
- Linked the acoustic contract from `docs/specs/graphics/graphics-fog.md` so
  future tower rain and indoor muffling use the same design.
- Documented the packaged FFmpeg contract and rebuild rule in
  `docs/operations/asset-management/asset-management.md`.

## Implementation changes

### Build blocker

- `src/network/server-packets.cpp`
  - Added `#include "physics/movement/collision-aabb-tree.h"`.
  - Before: `AabbTree::query()` was called through a forward declaration and
    failed to compile at the two `cachedEntitySurface` sites.
  - After: the complete tree type is available to the packet handlers.

### Packaged FFmpeg

- Restored `devscripts/build-minimal-ffmpeg.sh` from commit `083e7504` and
  extended its whitelist:
  - demuxers: `rawvideo,wav,mov,matroska,ogg`
  - decoders: `...,opus,vorbis,...`
  - parsers: `...,opus,vorbis`
  - muxers: `mov,mp4,wav,null`
  - encoder: `libx264,aac,pcm_s16le`
- Rebuilt `tools/ffmpeg.exe` with FFmpeg n9.0 and x264 `0480cb0`.
  - Before: 6,254,080 bytes; no Ogg demuxer, no WAV muxer, no PCM encoder.
  - After: 6,306,304 bytes; Ogg, Opus, Vorbis, WAV output, and null output
    present.
- `src/replay/replay-export-json.cpp`
  - `defaultFfmpegPath()` now prefers the packaged `tools/ffmpeg.exe` after
    the explicit environment/config overrides, then falls back to PATH.
  - Before: a system FFmpeg always won, hiding an incapable packaged binary.
  - After: the shipped, tested decoder is preferred; explicit overrides still
    work.

### Music playback

- `src/audio/music-manager.cpp`
  - Added `runFfmpeg(...)` using `CreateProcessA` with an inherited diagnostic
    log handle instead of `std::system`.
  - Before: FFmpeg was launched through a shell and returned exit 1 from the
    game process with no stderr evidence.
  - After: the decoder receives an absolute input path, writes its diagnostic
    output to a bounded log, and reports the real exit code and first error
    lines.
  - Added rate-limited `music.track.skipped` diagnostics for tracks whose
    cache is not ready or whose decoder lacks a capability.
  - Logs the selected FFmpeg path once per predecode worker.
- `src/audio/music-manager.h`
  - Added the skip-diagnostic set.
- Existing playlist discovery, linear/shuffle bags, previous/next history,
  personal mode, widget controls, and settings persistence were validated
  rather than replaced.

### Spatial audio

- Added `config/audio/spatial-audio.json`
  - Central category profiles for Weapons, Impacts, Movement, NPC, Ambient,
    and UI.
  - All categories stay at full volume through 50 m, then drop off.
  - Default mode is legacy; spatial mode keeps the same 50 m full-volume
    behavior plus low-pass, delay/reverb, and geometry response.
  - `legacy` exposes `fullVolumeDistance`, per-category `maxDistance`, and
    `volumeScale`, plus per-sound overrides.
  - Per-category reverb, muffle cutoff, obstruction, enclosure, and probe
    radius values.
  - Exact sound and prefix category overrides. UI is non-spatial.
- Added `src/config/spatial-audio-config.{h,cpp}`
  - Loads the JSON with last-valid-snapshot behavior.
  - Polls by content hash, not only timestamp, so live edits apply reliably
    without restarting the audio device or mixer.
- Added `src/audio/audio-acoustics.{h,cpp}`
  - Bounded world ray queries over the existing collision tree.
  - Three source-to-listener obstruction probes.
  - Nine-direction listener and source enclosure probes, ignoring the floor
    directly below the listener.
  - No traced reflection paths.
- `src/audio/audio.cpp` / `audio.h`
  - Added per-voice spatial profiles, distance gain, obstruction, enclosure,
    wet mix, cutoff, and node state.
  - World voices keep miniaudio panning for direction but use the category
    curve for volume. The inverse attenuation model is configured with equal
    min/max distance so the engine does not add a second curve.
  - Each spatial voice gets a low-pass filter plus delay tail:
    `sound -> low-pass -> delay -> engine endpoint`.
  - Direct sound is reduced by obstruction/enclosure; wet mix grows with
    distance and enclosure. UI and 2D sounds skip the chain.
  - Added `setAudioSpatialWorld(...)` and listener-forward access.
  - Added `audio.spatial.applied` and config load/reload events with sound,
    category, profile, position, listener, direction, distance, gain,
    obstruction, enclosure, wet mix, and cutoff.
- `src/main-systems.cpp` and `src/network/client.cpp`
  - Register the active world with the audio spatial estimator.
- `src/audio/audio-commands.{h,cpp}`
  - Added `audio_probe <sound> [distance] [front|left|right|rear]` through the
    normal world-sound path.
- `src/main-systems.cpp`
  - Registers the new audio command surface.
- `config/debuglogger.json`
  - Enabled the AUDIO structured category at `important` so the new evidence
    is observable. This does not change playback behavior.

### Validation support

- `devscripts/config_selftest.py`
  - Added `validate_spatial_audio(...)` for category names, increasing points,
    gain bounds, min/outer ranges, and required point pairs.

## Diagnostics

- Audio category owner: `StructuredCategory::Audio`.
- New events:
  - `audio.spatial.config.loaded`
  - `audio.spatial.config.reloaded`
  - `audio.spatial.applied`
  - `music.track.skipped`
- Existing events retained:
  - `music.track.started`
  - `music.track.load_failed`
  - `music.decode.failed`
  - `music.playlist.empty`
- Spatial updates are bounded to the one-shot start and 4 Hz acoustic refresh;
  no per-frame journal write was added.

## Validation and evidence

### Source/config

- `python -c "import sys; sys.path.insert(0,'devscripts'); import config_selftest as c; e=[]; print(c.validate_spatial_audio(e), e)"`
  returned `6 []`.
- `config/audio/spatial-audio.json` parsed as valid JSON.
- `tools/ffmpeg.exe` capability checks:
  - demuxers include `ogg`, `matroska`, `mov/mp4/m4a`, `wav`
  - decoders include `opus`, `vorbis`, `aac`, `pcm_s16le`
  - encoders include `pcm_s16le`
- FFmpeg sweep over all 28 files in `assets/music/ingame` found only the known
  malformed Opus file failing. The rebuilt binary decoded MP3 through
  miniaudio and Opus/OGG through FFmpeg.

### Build and executable identity

- Canonical dev-loop build succeeded as build `3905`.
- Exact executable: `C:\mimita-v9\.dev\builds\3905\mimita.exe`
- `--versioninfo`:
  - `EVENTS_JSONL_PATH=logs/10-31-2026/20261031_005551/events.jsonl`
  - `RUN_ID=20261031_005551`
- Runtime client journal for this executable:
  `C:\mimita-v9\logs\2026-10-31\20261031_045459\events.jsonl`

### Runtime journal evidence

- Music:
  - `music.track.started` for the in-game MP3 and for decoded Opus tracks:
    `Dj Twi$t II - 01 - Where U From_ 32k.opus`,
    `i know i.opus`, `my audio drivers broke while streaming fortnite.opus`,
    `The Great Strategy ...`, and others.
  - The malformed
    `UW eric scp 066 A creep phonk that  WOTN like.wav .opus` produced
    `music.decode.failed` with the selected executable and FFmpeg's
    `Invalid data found` detail, then was skipped without stopping the
    playlist.
  - `music_next`, `music_prev`, personal mode, and playlist order commands all
    reached `terminal.command.completed`.
- Spatial:
  - `audio.spatial.config.loaded` and two `audio.spatial.config.reloaded`
    events were captured from live file changes.
  - Legacy default: `revolvershoot` gains were `1.0` at 10 m, `1.0` at
    50 m, `0.90` at 60 m, and `0.50` at 100 m.
  - Spatial mode, after live mode switch: gains were `1.0` at 10 m and 50 m,
    `0.87` at 60 m, and `0.35` at 100 m, with wet mix reaching `1.0` at
    distant/obstructed probes.
  - Direction vectors were non-zero and distinct for front/right/left/rear
    probes.
  - `hitworld` resolved to `Impacts`; `npc_death` resolved to `NPC`;
    movement sounds resolved to `Movement`.
  - `ui/click` resolved to `UI` with `spatial=false`, `wet_mix=0`, and
    `distance_gain=1`.
  - Geometry probes produced varying obstruction/enclosure values and changed
    the cutoff, confirming the world input reaches the acoustic estimator.

### Follow-up legacy/mode validation

- Canonical dev-loop build: **3991**.
- Exact executable: `C:\mimita-v9\.dev\builds\3991\mimita.exe`.
- `--versioninfo` run id: `20261031_111528`;
  `EVENTS_JSONL_PATH=logs/10-31-2026/20261031_111528/events.jsonl`.
- Runtime client journal:
  `C:\mimita-v9\logs\2026-10-31\20261031_151514\events.jsonl`.
- Default config loaded with `"mode": "legacy"` and 6 legacy categories.
- Legacy probes:
  - `revolvershoot`: gain `1.0` at 10 m and 50 m, `0.90` at 60 m,
    `0.50` at 100 m.
  - `hitworld` and `npc_death`: gain `1.0` at 50 m, correct category and
    legacy max distances.
  - `player_hurt`: `volume_scale=1.2` from the legacy sound override.
  - `entity/player/jump`: `Movement`, full volume at 50 m, legacy max 80 m.
  - `ui/click`: `world=false`, `spatial=false`, `mode=legacy`, unaffected.
- Live mode switch to `"spatial"`:
  - `audio.spatial.config.reloaded` recorded `mode=spatial`.
  - `revolvershoot`: gain `1.0` at 10 m and 50 m, `0.87` at 60 m,
    `0.35` at 100 m; wet mix reached `1.0` on distant/obstructed probes.
- Live switch back to `"mode": "legacy"` recorded another reload event and
  left the file with legacy as the default.

### Quality gates

- `config_selftest.py` as a whole still fails on pre-existing avatar metadata
  errors unrelated to this task. The new spatial-audio validator passes.
- No new synthetic gameplay test was added; the real executable and journal
  were used for behavior evidence.

## Regression review

- No regression entry appended. The audio paths were previously incomplete or
  blocked by the build, not a confirmed working behavior that broke during
  this session.
- The pre-existing build failure in `server-packets.cpp` and the corrupted
  literal `\n` sequences in `actor-triangle-solver.cpp` were fixed only to
  reach a compilable, runnable executable.

## Human acceptance still required

- Listen to gunshots and other categories at near, 50 m, 500 m, 1 km, and the
  outer tail.
- Compare open-air and enclosed positions for muffling and reverb.
- Confirm distant direction feels usable.
- Confirm UI and music are unaffected.
- Tune `config/audio/spatial-audio.json` from that listening review.
- Repair or replace the malformed Opus asset outside this task.

## Files changed by this session

- `config/audio/spatial-audio.json` (new)
- `config/debuglogger.json`
- `devscripts/build-minimal-ffmpeg.sh` (restored/updated)
- `devscripts/config_selftest.py`
- `docs/operations/asset-management/asset-management.md`
- `docs/specs/graphics/graphics-fog.md`
- `docs/specs/music/music-sounds.md`
- `docs/specs/music/music.md`
- `src/audio/audio-acoustics.{h,cpp}` (new)
- `src/audio/audio-commands.{h,cpp}` (new)
- `src/audio/audio.cpp`
- `src/audio/audio.h`
- `src/audio/music-manager.cpp`
- `src/audio/music-manager.h`
- `src/config/spatial-audio-config.{h,cpp}` (new)
- `src/main-systems.cpp`
- `src/network/client.cpp`
- `src/network/server-packets.cpp`
- `src/physics/movement/actor-triangle-solver.cpp` (literal escape repair only)
- `src/replay/replay-export-json.cpp`
- `tools/ffmpeg.exe`
