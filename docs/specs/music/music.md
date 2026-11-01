2026 10 28 0902 jorj todo WHYYYYYYYYYYYY
DOES .OPIS
NOT PLA
INT HE GAME
PLZ
WE NEED MUSIC TO BE PLAUING ALL TEH TIME PLZZZZ BRO 

2026 10 19 1806 jorj todo
jorj = jorj1357
thats the artist name
i startded doing creating things on theinternet  doing msuic
so 
msuic  is deep deep important to me 

but ok sooooo

music 

i made this lowk because its like  broken right now
i want music to pla like per gamemode
so we need that

---

# In-game music player — final behavior contract

**Status: IMPLEMENTED CONTRACT.** The format scope, playlist behavior,
navigation, and mode ownership below are the desired behavior. Runtime and
human listening acceptance remain separate evidence from this contract.

## Goals

- Music plays reliably during gameplay from `assets/music/ingame/`, including
  tracks encoded as MP3, WAV, FLAC, Ogg Vorbis, Opus-in-Ogg, AAC/M4A, and other
  common formats readable by the selected FFmpeg build.
- Users may add their own music files to `assets/music/ingame/`. The player
  discovers them at startup or after `music_reload`; track names are not
  hard-coded.
- The player lets the user skip tracks and choose linear or shuffled playlist
  playback.
- Linear playback follows case-insensitive filename order and wraps to the
  beginning after the last playable track.
- Shuffle is the default to preserve the current in-game playlist behavior;
  the selected order is saved in `config/audio/music-settings.json`.
- Shuffle plays every playable track once before reshuffling and avoids
  repeating the last track at a playlist boundary. Unreadable or unsupported
  tracks are skipped with a useful diagnostic.
- Previous returns to the track most recently heard, including in shuffle
  playback. It navigates the most recent playback history before Next resumes
  forward through that history; history is limited to the latest 256 tracks.
- Music should be selectable per game mode rather than requiring track names
  to be hard-coded into general playback code. Mode-specific soundtrack rules
  remain owned by the relevant game-mode specification.
- A mode-configured soundtrack is the default while that mode is active. The
  user may mute it or choose personal music locally.
- The personal playlist is the local collection in `assets/music/ingame/`.
  The in-game widget and terminal command surface expose order and personal
  playlist selection through the same `MusicManager` actions.
- In-game music is a 2D playlist and is never treated as a spatial world sound.

## Playback contract

- `music_next` advances according to the selected linear or shuffle mode;
  `music_prev` returns to the preceding played track. Track history is bounded
  to the active playlist session and is reset when its playlist is rebuilt.
- A mode soundtrack has priority over the general in-game folder playlist.
  Personal music is a client-local override and does not change the track
  selected for other clients; mute is also client-local.
- Discovery is case-insensitive for the file extension. A file is offered to
  the playlist when its extension is in the accepted set; actual playability
  is decided by the decoder that opens it.
- MP3, WAV, and FLAC are opened directly by miniaudio. Ogg Vorbis,
  Opus-in-Ogg, AAC/M4A, and other FFmpeg-only formats are decoded by the
  selected FFmpeg into the per-track cached PCM WAV before miniaudio opens
  them. Decoding runs on a worker thread so gameplay never blocks on FFmpeg.
- A file that cannot be decoded is skipped, logged once with its path and
  reason, and never interrupts the rest of the playlist.
- If the FFmpeg cache for a track is not ready yet, playback retries later
  instead of crashing, hanging, or silently replacing the selected order.

## Format authority and fallback limits

FFmpeg selection order is:

1. `MIMITA_FFMPEG` when it points to an existing executable.
2. `config/replay/ffmpeg-path.json` when its `path` points to an existing
   executable.
3. `ffmpeg.exe` on the system `PATH`.
4. The bundled `tools/ffmpeg.exe` beside the game or working directory.

The bundled binary is a deliberately small build. It must contain the file
protocol plus the demuxers and decoders needed by the shipped music set:
`wav`, `mov/mp4/m4a`, `matroska`, `ogg`, `opus`, `vorbis`, `aac`, and PCM.
A binary that lacks the Ogg demuxer cannot play Opus-in-Ogg even though it
contains an Opus decoder. The rebuild recipe is versioned in
`devscripts/build-minimal-ffmpeg.sh`; its feature whitelist is part of this
contract, not an incidental build detail.

When a selected binary lacks a required demuxer or decoder, the diagnostic
must name the selected executable, the failing track, and the missing
capability rather than only reporting a generic load failure.

## Controls

- Music widget: play/pause, next, previous, volume, mute, linear/shuffle,
  personal-versus-mode music, and speed.
- Terminal: `music_next`, `music_prev`, `music_order linear|shuffle`,
  `music_personal 0|1`, `music_pause`, `music_resume`, `music_stop`,
  `music_reload`, `music_info`, `music_status`, `music_volume`, `music_mute`,
  `music_speed`.

## Evidence required

- Source and configuration evidence for discovery, decoder selection, and
  playlist state.
- Runtime evidence from the exact executable: `--versioninfo`, its
  `EVENTS_JSONL_PATH`, and journal records for a successful track start plus a
  skipped corrupt/unsupported track.
- Human listening and widget interaction acceptance. A successful build alone
  does not prove audible playback.

## Sound effect configuration

Music settings and world-sound settings are separate:

- `config/audio/music-settings.json` owns music volume, mute, speed, playlist
  order, and personal/mode selection.
- `config/audio/spatial-audio.json` owns all 3D world-sound categories,
  distance curves, full-volume distance, dropoff, reverb/delay, obstruction,
  enclosure, legacy mode, and per-sound overrides.
- `config/debuglogger.json` only controls whether AUDIO diagnostics are
  written; it does not change sound.

`config/audio/spatial-audio.json` is the authoritative place to edit sound
behavior. See the full [spatial world-sound contract](music-sounds.md) for
field descriptions and editing rules.

## Related records

- [Music-player format audit](../../changelog/2026-10-31/20261031_025404-music-player-format-audit.md)
- [Opus music support](../../changelog/2026-10-05/20261005_210500-opus-music-support.md)
- [Spatial world-sound contract](music-sounds.md)
