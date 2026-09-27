# Phase 1: hot audio ownership contract and versioned command envelope

Date: 2026-09-23T21:32:04Z
Status: contract written; ABI envelope landed; build + headless selftests pass; no live human acceptance

## Change

- Added `docs/architecture/live-development/hot-audio-contract.md`: the audio
  ownership contract, append-only `GameAudioCommandV2` field list, expanded
  `GameAudioOp`/`GameAudioCategory`/`GameAudioInterruption`, recipe schema,
  generation-aware resource lifecycle, journal event list, runtime commands,
  `hitfx.json` plan, migration order, and acceptance criteria. Registered the
  doc in `docs/ROUTER.md`.
- Extended `GameAudioCommandV1` append-only in `src/hot-reload/game-api.h` with
  `commandVersion`/`structSize`, request/event id, hot generation, resource
  generation, category, spatial mode, priority, interruption, replay capture,
  flags, seed, velocity, falloff, start offset, fade in/out, and query out
  fields. Existing fields keep their offsets; a legacy zero-initialized caller
  is treated as v1. Fixed-size POD only.
- Expanded `GameAudioOp` with `PAUSE_SLOT`, `RESUME_SLOT`, `SET_LISTENER`,
  `RELOAD_RESOURCE`, `INVALIDATE_RESOURCE`, and `QUERY_STATUS`; added
  `GAME_AUDIO_COMMAND_VERSION = 2`.
- Bumped the capability signature to `sig.audio.play.v2` and made
  `capAudioPlay` (`src/live-code/live-behavior.cpp`) a validating dispatcher:
  version/structSize gate, unknown-version and unknown-op rejection with a new
  `audioCommandRejectCount` counter, `QUERY_STATUS` (live voice/cached counts),
  `SET_LISTENER`, and in-place `PAUSE_SLOT`/`RESUME_SLOT`. `RELOAD_RESOURCE` and
  `INVALIDATE_RESOURCE` accept the shape and report `ok = 0` until the resource
  phase. No sound selection policy remains in the bridge.
- Added kernel mechanism `AudioManager::setOwnerPaused`, `activeVoiceCount`, and
  `cachedSoundCount` (`src/audio/audio.h`, `src/audio/audio.cpp`).
- Added headless proof in `src/network/hot-combat-selftest.cpp`: command is
  trivially copyable + standard layout; `audio.play` v2 resolves; unknown
  version rejected without playback; unknown op rejected; `QUERY_STATUS` reports
  without playback; `SET_LISTENER` updates without playback; `PAUSE`/`RESUME`
  slot is safe.

## Evidence

Source ownership:

- Hot policy still lives in `effect-composition.cpp` (unchanged this phase);
  this phase adds the stable envelope only.
- Cold owner removed: none yet (policy migration is Phase 3+).
- Cold mechanism remaining: device, mixer, name-keyed cache, `playWorldSound`.

Build:

- `python build_agent.py` -> `BUILD SUCCESS`, exe
  `mimita-20260923T172355.exe` (compiled 174, skipped 523).

Runtime (headless):

- `--hot-combat-selftest`: 204 ok / 28 fail (was 197 ok / 28 fail on the
  pre-change exe `mimita-20260923T164718.exe`). The +7 are the new audio ABI
  checks; no new failure names. The 28 failures are pre-existing (animation
  phase2, plus the `effect.footstep.sound`/`effect.movement.air_jump` tests that
  expect playback the current handler intentionally suppresses).
- `--live-code-selftest`: PASS.

Human:

- No live human audible/visual acceptance was performed.

## Remaining (next phases)

1. Phase 2: audio journal events (`audio.requested` ... `audio.hot_policy_activated`)
   and the `audio status|resources|voices|reload|trace` hot commands.
2. Phase 3: hot audio-policy owner + `config/audio-recipes.json`; migrate
   movement audio (jump/air-jump/dash/down-dash/land/footstep/freeze/hurt) with
   a cold fallback that yields on `handled`.
3. Phase 4: weapons/projectiles; Phase 5: NPC/actor; Phase 6: UI/notifications/
   editor/live-code; Phase 7: music/ambient.
4. Phase 8: logical sound-resource generations with off-thread decode and
   refcounted retire in `PresentationResourceProvider`; wire
   `RELOAD_RESOURCE`/`INVALIDATE_RESOURCE`.
5. Phase 9: `hitfx.json` control for jump/air-jump/footstep/movement effects.
6. Phase 10: delete obsolete direct helpers and dead fallbacks after runtime
   proof. Phase 11: audit update + permanent regression coverage.

## Notes

- The environment committed these edits as `b1c4b2c` (not authored by this
  session's tooling as an explicit commit step); the working tree is clean.
- Voice budget/interruption enforcement is declared in the ABI but not yet
  applied by the mixer; that is Phase 3+ work.
