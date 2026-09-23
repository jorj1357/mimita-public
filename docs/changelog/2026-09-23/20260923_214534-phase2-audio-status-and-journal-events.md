# Phase 2: generic audio status and journal events

Date: 2026-09-23T21:45:34Z
Status: commands + journal events landed; build + headless selftests pass; no live human acceptance

## Change

- `src/hot-reload/modules/presentation/audio-commands.cpp` (new hot module): one
  `audio` command with subcommands `status`, `resources`, `voices`,
  `reload <logical-id>`, and `trace <0|1>`. It resolves the stable `audio.play`
  capability, issues plain-data ops, and prints through `terminal.output`. No
  cold command switch; auto-discovered by the existing presentation glob.
- `src/hot-reload/game-api.h`: appended `GAME_AUDIO_SET_TRACE = 9` to
  `GameAudioOp` (flags bit0 enables rate-limited tracing).
- `src/live-code/live-behavior.cpp`: added `audioJournal` /
  `audioJournalTrace` / `audioJournalRejected` and wired them into
  `capAudioPlay`. Journaled types: `audio.command_rejected` (always, throttled),
  `audio.device_status` (status query + trace toggle), `audio.hot_policy_activated`
  (first command per hot generation), and, while tracing is on,
  `audio.requested`, `audio.accepted`, `audio.voice_started`,
  `audio.voice_replaced`, `audio.voice_stopped`, and `audio.resource_lookup`.
  Each line carries UTC ms + mono ms (journal), hot generation, sound, op, slot,
  voice, volume, pitch, position, category, resource generation, and fallback.
  Added `audioJournalCount()`.
- `src/audio/audio.{h,cpp}`: added `AudioManager::deviceActive()` for the status
  report.
- `src/network/hot-combat-selftest.cpp`: initializes the live journal so the
  headless run writes real audio JSONL, and adds checks for the `audio` command,
  status journaling, and traced one-shot journaling.
- `docs/architecture/live-development/hot-audio-contract.md`: op list updated
  with `GAME_AUDIO_SET_TRACE`.

## Evidence

Source ownership:

- Hot: `audio-commands.cpp` owns the command surface and queries the kernel.
- Cold owner removed: none (policy migration is Phase 3+).
- Cold mechanism remaining: device, mixer, name-keyed cache, `playWorldSound`.
- `capAudioPlay` now records observability only; it still performs no selection.

Build:

- `python build_game_dll.py` -> `DLL build success` (sources=87).
- `python build_agent.py` -> `BUILD SUCCESS`, exe `mimita-20260923T174442.exe`.

Runtime (headless):

- `--hot-combat-selftest`: 207 ok / 28 fail (Phase 1 exe was 204 ok / 28 fail);
  the +3 are the new Phase 2 checks; no new failure names. The 28 failures are
  pre-existing.
- `--live-code-selftest`: PASS.
- Live journal `logs/features/live-code/2026-09-23/live_events_20260923_214506.jsonl`
  contains real `audio.command_rejected`, `audio.device_status`,
  `audio.hot_policy_activated`, `audio.requested`, and `audio.accepted` lines
  with UTC + monotonic time and hot generation.

Human:

- No live human audible/visual acceptance was performed.

## Remaining (next phases)

1. Phase 3: hot audio-policy owner + `config/audio-recipes.json`; migrate
   movement audio (jump/air-jump/dash/down-dash/land/footstep/freeze/hurt) with
   a fallback that yields on `handled`. Enforce voice budget/interruption.
2. Phase 4: weapons/projectiles. Phase 5: NPC/actor. Phase 6:
   UI/notifications/editor/live-code. Phase 7: music/ambient.
3. Phase 8: logical sound-resource generations with off-thread decode and
   refcounted retire in `PresentationResourceProvider`; wire
   `RELOAD_RESOURCE`/`INVALIDATE_RESOURCE` and journal
   `audio.resource_lookup`/`audio.resource_generation_changed`/decode events.
4. Phase 9: `hitfx.json` control for jump/air-jump/footstep/movement effects.
5. Phase 10: delete obsolete helpers/dead fallbacks after runtime proof.
   Phase 11: audit update + permanent regression coverage.

## Notes

- `audio.voice_finished` is not yet emitted (voice cleanup lives in
  `audio.cpp`); it lands with the resource-generation phase.
- `audio voices`/`audio resources` currently report counts; detailed listings
  land with Phase 8.
