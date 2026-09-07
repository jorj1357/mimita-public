// 2026-09-07T19:30:00Z
/* purpose
* preserve the confirmed replay-export effect re-presentation fix
* explain the swap-buffer ownership bug in a reusable form
* separate confirmed effect progress from untested weapon and camera behavior
* this file does NOT claim complete replay/live effect parity
* this file does NOT claim the camera startup regression is fixed
* this file does NOT replace the authoritative replay or effects specifications
*/

# Gold behavior: replay export consumes effect batches exactly once

- Timestamp: `2026-09-07T19:30:00Z`
- Scope: replay effect, sound, and killfeed event consumption during export
- Result: after clearing the reusable caller buffers, effects no longer disappear and then respawn indefinitely from the same consumed replay batch
- Regression: `docs/regressions/regressions-v1.md`, entry `2026-09-07T19:30:00Z`
- Changelog: `docs/changelog/2026-09-07/20260907_151500-replay-consumed-batches.md`

## The old failure

Replay delivery used `swap()` to transfer pending events into static reusable vectors:

```cpp
gReplayPlayer.takeTriggeredEffects(effects);
for (const ReplayEffectEvent& effect : effects) {
    // dispatch effect
}
// missing effects.clear()
```

The consumed vector still owned the previous events. On the next frame,
`takeTriggeredEffects()` swapped those old events back into the replay player,
so an effect could reach its lifetime, disappear, and then be created again.

The same ownership mistake affected replay sounds and killfeed events.

## The working behavior

```cpp
gReplayPlayer.takeTriggeredEffects(effects);
for (const ReplayEffectEvent& effect : effects) {
    // dispatch effect through the existing effect owner
}
effects.clear();

gReplayPlayer.takeTriggeredSounds(sounds);
for (const ReplaySoundEvent& sound : sounds) {
    // dispatch sound through the existing audio owner
}
sounds.clear();

gpReplayPlayer->takeTriggeredKillfeedEvents(killEvents);
for (const ReplayKillfeedEvent& event : killEvents) {
    // dispatch through KillfeedManager
}
killEvents.clear();
```

Clearing the caller buffer after dispatch makes ownership one-way for that
presentation batch. The effect system can then advance and delete each object
according to its normal lifetime without the replay player reintroducing it.

## Evidence

- Existing replay export self-check: `26/26 passed`.
- Human report after the fix: the third export no longer showed the previous
  effect spam.
- The first-export camera issue remains: first export at `(0,0,0)` with no
  movable camera; second export allowed looking around but movement was not
  tested; third export camera worked.

## Still unverified

The following require a fresh live export test:

- revolver muzzle flash and one-tick white muzzle sphere;
- revolver tracer fade;
- rocket projectile movement and collision;
- rocket smoke and explosion;
- dynamic-light lifetime;
- other weapon effects;
- camera behavior on the first export after process startup.

This is gold evidence for exactly-once replay event consumption, not proof that
all replay effects already use the complete shared live gameplay path.
