# In-game music format support audit

- Task ID: current Codex session
- Summary: Explain why the newly added MP3 plays and audit current format handling.
- Status: PASS_WITH_HUMAN_REVIEW; source audit only, no code changed.
- Date, time, timezone: 2026-10-23T22:45:45Z (2026-10-23 18:45:45 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: 2103321b
- Final commit: 2103321b (unchanged)

## Pre-existing changes

- Existing working-tree edits observed before this session's documentation addition: `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/lighting.json`, `config/maps/zombietower4.json`, `docs/jorj-docs/20261023plan.md`, `docs/specs/open-gameplay-issues.md`, `src/gui/gui-main.cpp`, `src/network/server-ice.cpp`, `docs/changelog/2026-10-23/20261023_182134-clarify-gameplay-issues-intake.md`, and `docs/changelog/2026-10-23/20261023_182820-extend-room-startup-wait.md`.
- These pre-existing changes were not modified.

## Requested behavior

Explain why MP3 music plays while other music files may not, whether MP3/Opus/WAV and common formats can be supported, and whether additional libraries are needed.

## Specification alignment

- Current focused specification: no general in-game music file-format specification was found in the routed/current music documents. Zombie Tower's current spec examples include `.opus` tracks at `docs/specs/gamemodes/zombie-tower/zombie-tower.md:75-88`.
- Existing implementation and prior changelog define the practical source behavior; this session did not change it.

## Exact implementation changes

### File: `src/audio/music-manager.cpp`

- Lines/functions: `hasMusicExt()` lines 25-34; `resolvePlaybackPath()` lines 108-160; `startTrack()` lines 162-179.
- Old content or behavior: No changes; source was read only.
- New content or behavior: No changes; source was read only.
- Findings: folder scanning accepts `.mp3`, `.wav`, `.ogg`, and `.opus`. Non-Opus files pass directly to miniaudio. `.opus` files are converted by the existing FFmpeg executable to cached 48 kHz stereo PCM WAV, then passed to miniaudio. Decode/load failures are logged and playlist advancement skips unusable tracks.

### File: `docs/changelog/2026-10-23/20261023_184545-audio-format-support-audit.md`

- Lines/functions: full file, added for this session.
- Old content: There was no prior text; this file did not exist.
- New content: The complete text of this audit record.
- Reason: Repository session records are required by the project router.

## Diagnostics

- Owner/category: `MusicManager`, `Debug::Category::Audio`.
- Existing diagnostics identify missing FFmpeg, Opus conversion failure, miniaudio file load failure, skipped playlist entries, and no playable tracks.
- This source audit did not exercise the running game or capture runtime logs.

## Validation

- Focused skill paths and results: `docs/skills/asset-checker-v1.md` reviewed; PASS for source/path audit. `docs/skills/spec-behavior-review-v1.md` reviewed; no general supported-format rule found.
- Tests and exact commands: None run; user asked for explanation, and the source already handles the named formats.
- Build status: Not run; no code changed.
- Runtime or hot-reload evidence: Not captured. Current executable identity is unknown.
- Source evidence: `src/audio/music-manager.cpp:25-34,108-179` and bundled `tools/ffmpeg.exe` exists.
- Historical evidence: `docs/changelog/2026-10-05/20261005_210500-opus-music-support.md` reports that one Opus file was malformed and 26 other Opus files decoded successfully. It does not identify that malformed filename in its summary.

## Regression review

- Regression entry appended: no.
- Why: Current source accepts the requested formats; this audit did not establish a fresh behavior regression in the active executable.

## Human acceptance

- Audible playback in the currently running game remains unverified.
- A stale executable could lack the source behavior; the exact process/build must be checked before assigning the cause to file decoding.

## Related feature record

- `docs/changelog/2026-10-05/20261005_210500-opus-music-support.md`
