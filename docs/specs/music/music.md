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

# In-game music player — behavior draft

**Status: READY FOR IMPLEMENTATION.** The format scope, playlist behavior,
navigation, and mode ownership below are confirmed. Runtime acceptance remains
separate from this behavior contract.

## Confirmed goals

- Music should play reliably during gameplay, including tracks encoded as
  Opus, MP3, and WAV.
- Supported formats include MP3, WAV, Opus-in-Ogg, OGG Vorbis, FLAC, AAC/M4A,
  and other common formats readable by the selected FFmpeg decoder. A file
  extension alone does not establish support: the selected decoder must have
  the required container demuxer and codec decoder. FFmpeg may come from the
  explicit setting, the system path, or the packaged fallback.
- The player should let the user skip tracks and choose linear or shuffled
  playlist playback.
- Linear playback follows case-insensitive filename order and wraps to the
  beginning after the last playable track.
- Shuffle is the default to preserve the current in-game playlist behavior;
  the selected order is saved in `config/audio/music-settings.json`.
- Shuffle should play every playable track once before reshuffling and avoid
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

## Confirmed playback contract

- `music_next` advances according to the selected linear or shuffle mode;
  `music_prev` returns to the preceding played track. Track history is bounded
  to the active playlist session and is reset when its playlist is rebuilt.
- A mode soundtrack has priority over the general in-game folder playlist.
  Personal music is a client-local override and does not change the track
  selected for other clients; mute is also client-local.
- The format set is MP3, WAV, Opus-in-Ogg, OGG Vorbis, FLAC, AAC/M4A, and other
  common formats readable by the FFmpeg decoder shipped with the game. The
  shipped decoder must include the demuxers and codec decoders for this set.

## Current source finding — format gap

The current implementation scans common music extensions. MP3, WAV, and FLAC
are passed directly to miniaudio. Opus-in-Ogg, OGG, AAC/M4A, and other listed
formats are converted by FFmpeg to a cached WAV on a worker before miniaudio
loads them. The packaged fallback in this checkout includes an Opus decoder
but lacks the Ogg demuxer, so FFmpeg-only formats require a compatible explicit
or system FFmpeg here. Extension scanning alone does not establish playable
format support. See the [music-player audit record](../../changelog/2026-10-31/20261031_025404-music-player-format-audit.md)
for exact source, asset, and executable evidence.

This finding describes current implementation, not desired behavior. The code
must be aligned only after this draft's open decisions are resolved.
