# Audio gap closure: slot API, voice budget/interruption, cooldown policy, music selection

Date: 2026-09-23T23:30:01Z
Status: implemented; build + headless selftests pass; no live human acceptance; nothing deleted

## Change

Owner-stopped / looping voices now use the hot slot API:

- `src/hot-reload/game-api.h`: appended `slotId`/`slotOp`/`loop` to
  `GameAudioFactV1` (audio.fact.v3) with `GAME_AUDIO_FACT_SLOT_SET` /
  `GAME_AUDIO_FACT_SLOT_STOP`. Added `GameMusicPolicyV1` + `GAME_EVENT_AUDIO_MUSIC`.
- `src/hot-reload/hot-audio-policy.h` / `modules/presentation/audio-policy.cpp`:
  added `hotEmitRecipeSlot` / `hotStopRecipeSlot`, a shared
  `emitRecipeCommand` (one-shot and slot), recipe `cooldownMs` / `repeatAllowed`
  enforcement (suppressed events stay "handled" so the cold fallback does not
  play), and the `audio.music` policy handler. `onAudioFact` now routes SET/STOP
  slot ops.
- `src/live-code/live-behavior.h/.cpp`: added `emitAudioSlot` /
  `emitAudioSlotStop`, and voice-budget/interruption enforcement in
  `capAudioPlay` (new-callers only; legacy version 0 bypasses). One-shot and slot
  voices now carry `priority`.
- `src/audio/audio.h/.cpp`: `AudioEvent.priority`, `ActiveSound.priority`,
  `AudioManager::setVoiceBudget/voiceBudget/makeRoomForVoice/stopOldest`.
  `makeRoomForVoice` allows OVERLAP, refuses REJECT, and for
  REPLACE_OLDEST/REPLACE_SAME_SLOT/RESTART evicts the lowest-priority active
  voice when the incoming voice is at least as high.
- `src/combat/weapon-spyknife.cpp`, `src/combat/weapon-quick-hit.cpp`: swing
  voices migrated to `emitAudioSlot` (SET replaces the previous swing voice);
  cold `stopOwner` retained as fallback.
- `src/npc/npc-spawn.cpp`: spawn voices migrated to `emitAudioSlot`; the
  `stopOwner` teardown sites use `emitAudioSlotStop` with a cold fallback.
- `src/audio/music-manager.{h,cpp}`: added a hot music-selection policy seam
  (`queryMusicPolicy`) used by `pickMenuTrack` / `playNextIngame` /
  `enterGameMode`; the policy picks the candidate index and returns
  volume/pitch multipliers. The streaming engine, file decode, seek, pause, and
  playlist storage stay in the EXE. Cold RNG selection remains the fallback.
- `config/audio-recipes.json`: `music.change` now `loop: true`.

Nothing was deleted; every migrated call keeps its cold path.

## Evidence

Source ownership:

- Hot: slot lifecycle, recipe cooldown/repeat, music selection policy.
- Cold mechanism remaining: miniaudio device/mixer/voice table, music streaming
  engine, name-keyed cache fallback.
- One owner per fact: the hot handler sets `handled`; cold fallback runs only
  when unhandled.

Build:

- `python build_agent.py` -> `BUILD SUCCESS`, exe
  `mimita-20260923T192858.exe` (the pipeline also rebuilt `build/mimita-game.dll`
  against the changed hot sources).

Runtime (headless):

- `--hot-combat-selftest`: 220 ok / 25 fail (previous baseline 216 ok / 25 fail);
  the +4 are the new checks: `audio.fact SET_SLOT starts an owner slot voice`,
  `audio.fact STOP_SLOT stops the slot voice`, `audio.music policy picks a
  candidate index`, `voice budget + REPLACE_OLDEST keeps voices at budget`. No
  new failure names.
- `--live-code-selftest`: PASS.
- Earlier transient readings (149/146 ok) were caused by the concurrent
  networking build replacing `build/mimita-game.dll` mid-run; re-running both
  exes reproduced 216/216 before the new checks were added.

Human:

- No live human audible acceptance was performed.

## Remaining

- Live proof still required: edit a recipe `cooldownMs` / `repeatAllowed` and
  hear the gate; confirm spyknife/quick-hit swings cut the previous voice and
  NPC spawn voices stop on despawn; confirm music selection/volume changes live.
- Music **streaming** itself (engine, decode, seek, pause) is intentionally still
  cold; only its selection/volume/pitch policy is hot. Migrating the streaming
  mechanism would require a different (streaming) capability than the in-memory
  sample slot.
- Phase 10 (delete obsolete helpers/dead fallbacks) and Phase 11 (audit update +
  permanent regression coverage) remain outstanding by instruction.
