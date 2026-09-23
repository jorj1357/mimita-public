# Phases 8-9: sound-resource generations and hitfx.json movement control

Date: 2026-09-23T22:40:34Z
Status: off-thread generation reload + refcounted retire landed; hitfx.json drives hot movement effects; build + headless selftests pass; no live human acceptance

## Change

Phase 8 (logical sound-resource generations):

- `src/audio/audio-resource.h` + `src/audio/audio-resource.cpp` (new):
  `AudioResourceRegistry` maps a logical sound name to an immutable, hash-tagged
  generation of bytes. A worker thread decodes/validates on reload
  (`decodeAudioToPCM`, checking format/channels/length), `update()` publishes the
  new generation at the safe audio boundary, and a failed decode keeps the
  last-good generation. Generations are `shared_ptr`, so a voice holds the
  generation it started with and an old generation retires only when no voice
  references it. `acquire` lazily creates generation 1 on first play.
- `src/audio/audio.cpp`: `ActiveSound` holds the generation shared_ptr; `startSound`
  uses the current generation's bytes (falling back to the legacy name cache);
  `audioUpdate` publishes completed decodes and journals `audio.voice_finished`
  when tracing is on. Added `AudioManager::setTrace/trace`.
- `src/live-code/live-behavior.cpp`: `RELOAD_RESOURCE` now queues an off-thread
  reload, `INVALIDATE_RESOURCE` drops the generation, `QUERY_STATUS` reports the
  generation count, and `SET_TRACE` mirrors into the audio service.
- Journal events: `audio.resource_decode_started/finished/failed`,
  `audio.resource_generation_changed`, `audio.resource_lookup`,
  `audio.voice_finished`.

Phase 9 (hitfx.json control for hot movement effects):

- `src/hot-reload/modules/presentation/effect-composition.cpp`: reads
  `config/hitfx.json` (hot, `last_write_time` poll, last-valid preserved) and
  drives `groundJumpBurst`, `airJumpBurst`, `walkBurst`, `landingBurst`,
  `movementDashBurst`, `dash`, `perfectDash`, `freeze`, `freezeTrail`,
  `downDash`, and `footstep`. The composer now uses enabled state, lifetime
  (ticks or seconds), start/end length, start/end radius, colors, alpha,
  brightness, offsets, and speed-scaling fields; the cold `HitEffects`
  implementation remains the fallback. Nothing was deleted.

## Evidence

Source ownership:

- Hot: effect composer consumes the parsed hitfx recipe; audio policy owns sound.
- Kernel: audio service owns the device/mixer, the generation registry, and the
  voice table. No active voice is invalidated by a generation swap.
- Cold fallback: `HitEffects` and the name-keyed cache remain.

Build:

- `python build_game_dll.py` -> DLL built (94 sources).
- `python build_agent.py` -> `BUILD SUCCESS`, exe `mimita-20260923T183920.exe`.

Runtime (headless):

- `--hot-combat-selftest`: 216 ok / 25 fail (was 212 ok / 25 fail); the +4 are
  the new Phase 8 checks; no new failure names. The 25 failures are pre-existing.
- `--live-code-selftest`: PASS.
- Live journal `live_events_20260923_224007.jsonl` shows the generation path:
  `audio.resource_decode_started` / `audio.resource_decode_finished`
  (`content_hash=16327699586396159739`) for `entity/player/dash`, and
  `audio.resource_decode_failed` (`file-missing`) for an unknown sound - the
  failure path keeps the last-good generation.
- No `[HITFX]` parse error on the hitfx loader.

Human:

- No live human audible/visual acceptance was performed.

## Remaining (next phases)

1. Phase 10: delete obsolete direct helpers/dead fallbacks after runtime proof
   (explicitly deferred this session).
2. Phase 11: hot/cold audit update + permanent regression coverage.
3. Live proof still required: edit a `.wav` asset and hear the new generation
   without restart; edit `config/hitfx.json` footstep radius / jump colors and
   observe the next effect; confirm `audio.resource_generation_changed` during a
   live reload (the headless test does not run `audioUpdate`).
4. Still not hot: looping/owner-stopped voices (spyknife/quick-hit/npc-spawn
   `stopOwner`) and music streaming; voice budget/interruption and recipe
   `cooldownMs`/`repeatAllowed` are parsed but not enforced.
5. `PresentationResourceProvider` was not extended with refcounting this session;
   the generation/refcount authority for sound is `AudioResourceRegistry` (via
   `shared_ptr`), which satisfies the retire semantics. A later pass can unify
   the two providers if desired.

## Notes

- The reload worker decodes off-thread but playback still uses
  `ma_decoder_init_memory` on the generation's raw bytes, so no voice setup or
  device behavior changed. A future pass could play the decoded PCM via
  `ma_audio_buffer`.
- This session again ran alongside a concurrent networking task; builds were
  serialized by the build lock and no networking files were touched.
