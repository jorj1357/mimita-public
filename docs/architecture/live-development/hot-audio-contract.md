// 09 23 2026
/* purpose
* Define the ownership contract between hot audio behavior and the stable EXE
* audio service, and the append-only command/resource envelopes that cross it.
* Explain how audio policy, recipes, resources, and voice policy become live
* editable while the device and mixer stay alive.
* this file DOES NOT replace the live-development invariant or the hot ABI
* this file DOES NOT define individual gameplay sound formulas
* this file DOES NOT permit relinking a running executable
*/

start date: 2026-09-23
last updated: 2026-09-23

# Hot audio policy and resource contract

## Purpose

Keep the EXE-owned audio device and miniaudio mixer alive while every audio
decision, recipe, resource reload, and voice policy moves behind stable
plain-data capabilities. After migration, editing hot `.cpp` files or audio
configuration affects the running EXE at a safe tick without restarting the
world, player state, network session, or audio device.

The mixer implementation itself remains stable in the EXE so active voices and
miniaudio handles are never invalidated.

## Ownership contract

- The EXE owns the audio device and the miniaudio mixer. They are never
  restarted, replaced, or re-initialized by a hot change.
- Hot code owns all audio behavior: sound selection, variants, volume, pitch,
  falloff, category, one-shot/loop, interruption, owner identity, cooldown/edge
  policy, and routing.
- `config/audio-recipes.json` owns user-visible tuning and asset recipes.
- `src/audio/audio.cpp` owns mechanism only: validate, resolve, play, mix,
  retire, record.
- On any failed change (compile, config, decode) the last valid
  generation/snapshot stays active and a diagnostic is emitted.

One concept, one owner: for every migrated event exactly one of hot policy or
the cold fallback plays a sound. The fallback runs only when the hot handler
leaves the fact `handled == 0`.

```text
actor.jump.accepted
actor.air_jump.accepted
actor.footstep
actor.dash.completed
weapon.fire
weapon.reload
actor.damage
ui.click
npc.spawn
music.change
```

Hot policy converts those facts into one `GameAudioCommandV2`. Direct gameplay
calls such as `playWorldSound(...)`, `playSound(...)`, `playSoundPitched(...)`,
and `AudioManager::instance().play(...)` are removed from gameplay/presentation
owners and retained only inside the stable audio service or compatibility
adapters.

## Versioned ABI

### Existing fields are preserved at fixed offsets

`GameAudioCommandV1` keeps its current layout. The next version appends; it
never reorders or resizes existing fields, matching the `EffectRequestV1`
`.v2`/`.v3` precedent.

### Appended fields (v2)

```text
char     sound[64]          // logical sound id (recipe key / asset alias)
float    position[3]
float    volume
float    pitch
float    maxDistance
uint32   spatial            // legacy: 1 = world (spatialMode is authoritative)
uint32   action             // reserved
uint64   ownerEntity
uint64   slotId
uint32   op                 // GameAudioOp
uint32   loop               // legacy: 1 = loop
--- v2 append ---
uint32   commandVersion     // == 2 (0 from a legacy zero-initialized caller)
uint32   structSize         // sizeof(GameAudioCommandV2)
uint64   requestId          // monotonic request/event id
uint64   hotGeneration      // active hot-code generation that produced it
uint32   resourceGeneration // in: explicit generation (0 = current); out: resolved
uint32   category           // GameAudioCategory
uint32   spatialMode        // 0 local/2D, 1 world, 2 listener-relative
uint32   priority           // higher wins under a voice budget
uint32   interruption       // GameAudioInterruption
uint32   replayCapture      // 1 = capture into replay
uint32   flags              // bit0 oneShot bit1 loop bit2 pause bit3 resume bit4 stop
uint32   seed               // deterministic variant selection
float    velocity[3]
float    falloffStart
float    falloffEnd
float    startOffset        // seconds
float    fadeIn
float    fadeOut
--- out ---
uint32   ok
uint32   activeVoices
uint32   loadedResources
uint32   reserved
```

The ABI contains only fixed-size POD data. No `std::string`, STL container,
miniaudio object, owning pointer, or C++ class layout crosses the hot boundary.

### Op enum (one capability, expanded ops)

```text
GAME_AUDIO_PLAY_ONESHOT        = 0   (existing)
GAME_AUDIO_SET_SLOT            = 1   (existing)
GAME_AUDIO_STOP_SLOT           = 2   (existing)
GAME_AUDIO_PAUSE_SLOT          = 3   (append)
GAME_AUDIO_RESUME_SLOT         = 4   (append)
GAME_AUDIO_SET_LISTENER        = 5   (append)
GAME_AUDIO_RELOAD_RESOURCE     = 6   (append; logical id in `sound`)
GAME_AUDIO_INVALIDATE_RESOURCE = 7   (append; logical id in `sound`)
GAME_AUDIO_QUERY_STATUS        = 8   (append; fills out fields)
```

```text
GameAudioCategory: Movement, UI, Weapons, NPC, Impacts, Ambient, Music,
                   Notification, Editor, Debug
GameAudioInterruption: Reject, ReplaceOldest, ReplaceSameSlot, Overlap, Restart
```

### Capability ids

There is one capability id `audio.play`; the signature is
`sig.audio.play.v2`. All operations route through it. The kernel callable is
compiled against the full struct and gates a mismatched DLL through the
capability signature and the package ABI self-test.

## Kernel service

`src/audio/audio.cpp` stays in the EXE and owns:

```text
validate command
resolve logical resource
apply resource generation
start/update/stop voice
update mixer
record playback result
```

It no longer owns feature-specific decisions. `AudioManager::play` /
`stopOwner` remain as the low-level voice mechanism, reachable only from the
kernel service and compatibility adapters. `capAudioPlay` in
`src/live-code/live-behavior.cpp` becomes a thin dispatcher on `op`; it owns the
slot table keyed by `(ownerEntity, slotId)`, entity-death cleanup, and command
validation, and performs no selection policy.

## Hot audio-policy owner

```text
src/hot-reload/modules/presentation/audio-policy.cpp
src/hot-reload/hot-audio-policy.h
```

- Subscribes to generic facts through `MimitaHotPackage::EventRegistrar`.
- Maps fact -> recipe key -> resolved `GameAudioCommandV2`; sets `handled = 1`
  only when it emits a command.
- Reads `config/audio-recipes.json` directly with the established
  `last_write_time` poll and last-valid snapshot pattern.
- Inline audio policy is removed from `effect-composition.cpp`, which keeps
  visual composition only.

## Recipe schema

```json
{
  "footstep": {
    "sounds": ["entity/player/walk1", "entity/player/walk2",
               "entity/player/walk3", "entity/player/walk4"],
    "weights": [1, 1, 1, 1],
    "selection": "nondeterministic",
    "volume": { "base": 0.8, "min": 0.72, "max": 0.88 },
    "pitch":  { "base": 1.0, "min": 0.96, "max": 1.04 },
    "maxDistance": 22.0,
    "falloffStart": 2.0,
    "category": "Movement",
    "spatialMode": "world",
    "loop": false,
    "priority": 10,
    "overlap": "allow",
    "repeatAllowed": true,
    "cooldownMs": 0,
    "perActor": { "npc.*": { "volume": { "base": 0.7 } } },
    "perSurface": { "metal": { "sounds": ["surface/metal1"] } }
  }
}
```

Supports sound lists, weighted variants, deterministic/nondeterministic
selection, volume/pitch ranges, distance/falloff, category, spatial mode, loop,
priority, overlap/interruption, cooldown/edge policy, and per-actor/per-surface
overrides. Invalid JSON preserves the previous valid snapshot and emits a
diagnostic. `config/hitfx.json` remains visual-only; audio values do not go
there.

## Generation-aware sound resources

Reuse `PresentationResourceProvider` (`src/project/presentation-resource.h`) as
the single generation authority, extended with refcounted retire:

```text
acquire(logicalId) -> const ResourceGeneration*   // ++refcount
release(logicalId)                                 // --refcount; retire when 0
apply(...)                                         // moves old gen to retired list
```

Lifecycle:

1. Detect a changed sound file (content hash).
2. Decode on a worker thread into the hash-addressed `ArtifactCache`
   (`ResourceKind::Wav` + `validateWav` already exist).
3. Validate format, duration, channels, size.
4. Assign a new immutable generation.
5. Publish at the safe audio boundary; the loader installs the pre-decoded clip
   in O(1) with no game-thread decode.
6. New voices `acquire` the new generation.
7. Active voices keep their generation handle and reference.
8. Old generations retire only when no active voice references them.

Resource reload never closes the device, restarts the mixer, invalidates an
active voice, blocks the game thread, or replaces a resource in place while a
voice is using it. The command path supports `logical id -> current generation`
and `logical id + explicit generation` for replay/debug.

## Journal events

Written through `LiveEventJournal::Fields`.

```text
audio.requested  audio.accepted  audio.resource_lookup
audio.resource_generation_changed  audio.resource_decode_started
audio.resource_decode_finished  audio.resource_decode_failed
audio.voice_started  audio.voice_replaced  audio.voice_stopped
audio.voice_finished  audio.command_rejected  audio.device_status
audio.hot_policy_activated
```

Each line includes UTC millisecond timestamp, monotonic timestamp, simulation
tick, hot generation/hash, logical sound id, resource generation, actor/entity
id, voice/slot id, volume, pitch, spatial position, result/error, and fallback
status.

## Runtime commands

Hot `CommandRegistrar` commands, served by the `audio.query_status` capability:

```text
audio status
audio resources
audio voices
audio reload <logical-id>
audio trace <0|1>
```

Tracing is off by default and rate-limited.

## hitfx.json control for movement effects

`config/hitfx.json` remains cold-loaded and polled, but the hot effect composer
consumes its parsed recipe for `groundJumpBurst`, `airJumpBurst`, `footstep`,
`walkBurst`, `landingBurst`, dash, down-dash, freeze, and freeze trail. The hot
composer uses enabled state, lifetime ticks, start/end length, start/end radius,
colors, alpha, brightness, offsets, stretch axis, shape, and speed scaling. Cold
`HitEffects` remains the fallback until every migrated hot effect has live
runtime proof.

## Migration order

1. Define the audio ownership contract and versioned command/resource envelopes.
2. Add generic audio status and journal events.
3. Move movement audio completely to hot policy.
4. Migrate weapon and projectile audio.
5. Migrate NPC and actor audio.
6. Migrate UI, notifications, editor, and live-code sounds.
7. Migrate music and ambient policy.
8. Add hot logical sound-resource generations.
9. Restore `hitfx.json` control for jump, air-jump, footsteps, and movement effects.
10. Remove obsolete direct audio helpers and dead compatibility paths after runtime proof.
11. Update the hot/cold audit and add permanent regression coverage.

## Activation and rollback

- Hot C++: compile an immutable DLL generation, run ABI and deterministic
  self-test, activate at the top of the fixed tick, and retain rollback to the
  previous generation.
- Audio resource change: no DLL replacement; decode and publish a new resource
  generation at the audio command boundary.
- Invalid change: keep the previous generation or last valid config, emit a
  journal event, and keep the EXE and current audio state alive.

## Tests and acceptance

Automated:

- every command is POD-safe and ABI-version validated;
- invalid commands are rejected without crashing;
- one-shot reaches `audio.play`;
- loop start/update/stop is idempotent;
- owner cleanup stops voices on entity death;
- resource replacement preserves current voices;
- new voices use the new resource generation;
- invalid decode preserves the previous resource;
- hot policy changes activate after DLL reload;
- config changes preserve the last valid snapshot;
- hitfx jump/air-jump/footstep recipes reach the hot effect composer;
- no migrated event invokes both hot and cold audio owners.

Evidence is reported separately as source ownership, hot build/ABI result,
resource decode result, runtime journal result, live activation result, and
human audible/visual acceptance.

## Ownership map

- ABI / capability contract: `src/hot-reload/game-api.h`
- Kernel audio service: `src/audio/audio.cpp`, `src/audio/audio.h`
- Kernel command bridge: `src/live-code/live-behavior.cpp`
- Hot policy owner: `src/hot-reload/modules/presentation/audio-policy.cpp`
- Recipes: `config/audio-recipes.json`
- Resource generations: `src/project/presentation-resource.*`,
  `src/hot-reload/content-artifact.h`
- Journal: `src/live-code/live-journal.*`
- Hot effect recipes: `src/hot-reload/modules/presentation/effect-composition.cpp`,
  `config/hitfx.json`

## Related authoritative documents

- Specification: `docs/features/live-code-development/live-code-development.md`
- Architecture: `docs/architecture/live-development/hot-kernel.md`,
  `docs/architecture/live-development/hot-cold-audit.md`
- Asset rules: `docs/operations/asset-management/asset-management.md`
- Focused review: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`
