2026 20 19 1810 

C:\mimita-v9\docs\specs\graphics\graphics-fog.md

this
"that and rain
not sur ehow to do but 
rain is brown noies rain nosies happening in the game i want that 
and it gets muffled in the  tower "

i want dat
and directional audio but we have that
an dlike
bodycam 
reverb 
REVERBR ON SURFAESSSSS
PLZZZZZ
I LOVE THAT PLZ TAHT IS SO GOOD FOR LIKE REALISIMSMSS

---

# World-sound spatial behavior — final contract

**Status: IMPLEMENTED CONTRACT.** This is the desired behavior for all 3D
world sounds. Runtime and human listening acceptance are separate evidence.

## Scope

- Applies to every sound played as a 3D world sound: weapons, movement, NPCs,
  impacts, ambient world sources, and future world sounds such as tower rain.
- Does not apply to UI sounds or in-game music. Those stay 2D/unchanged.
- The in-game music playlist in `assets/music/ingame/` is music, not a spatial
  world sound. See [music.md](music.md).
- Distances are in meters.
- Category ranges and acoustic tuning live in one hot-reloadable source:
  `config/audio/spatial-audio.json`.
- Changes apply without restarting the audio device or mixer.

## Where to edit sound behavior

| File | Owns |
|---|---|
| `config/audio/spatial-audio.json` | World-sound mode, category curves, full-volume distance, dropoff, reverb, delay, obstruction, enclosure, and per-sound overrides. |
| `config/debuglogger.json` | Whether AUDIO diagnostics appear in the journal. This does not change sound. |
| `config/audio/music-settings.json` | In-game music volume and playlist state only. This does not change world sounds. |

`config/audio/spatial-audio.json` is the authoritative place to tune sound
behavior. It is read at runtime and polled for content changes. Editing it does
not require a rebuild or an audio-device restart.

## Modes

- `"mode": "legacy"` is the default. It uses old-style linear attenuation,
  no low-pass filter, no delay/reverb chain, and no geometry probes.
- `"mode": "spatial"` enables the category curves, distance-dependent
  direct/reverb split, muffling, obstruction, and enclosure.
- Both modes use the same category names and per-sound overrides.
- `legacy.fullVolumeDistance` controls how far legacy stays at full volume.
- `legacy.categories.<Category>.maxDistance` controls where legacy reaches
  silence.
- `legacy.categories.<Category>.volumeScale` and
  `legacy.sounds.<Sound>.volumeScale` adjust level.
- `"mode"` can be written as `legacy`/`spatial` or `1`/`2`.

## Distance behavior

- Every category is intended to stay at full volume through 50 m, then begin
  dropping. This applies to impacts, movement, weapons, NPCs, and ambient
  sounds.
- The 50 m full-volume distance is a tuning default, not a hard limit; each
  category's `points` array and `legacy.fullVolumeDistance` are editable.
- Each spatial category has its own attenuation points and outer limit.
- Attenuation is smooth between configured points. There is no abrupt
  per-sound cutoff at the old `maxDistance` values.
- Past `directFadeStart`, the dry direct copy fades out. Past
  `directFadeEnd`, the sound is heard through the delay/reverb path rather
  than as a dry source.
- The distant tail remains quiet and audible until the category outer limit,
  then reaches silence.

## Directionality

- Distance attenuation must not disable spatial panning.
- A distant sound keeps a perceptible direction relative to the listener.
- The reverberant share grows as the direct share fades. Reverb percentage is
  a share of the emitted sound, not a claim that total volume is 75%.

## Geometry-based acoustics

The first version infers acoustic character from nearby world collision
geometry; it does not trace reflected paths.

- **Obstruction:** source-to-listener probes detect walls between the sound and
  the listener. Obstruction reduces direct sound, lowers the muffling cutoff,
  and increases the reverberant share.
- **Enclosure:** short probes around the listener and source estimate whether
  the space is enclosed. Enclosure lowers the cutoff and increases the
  reverberant share.
- **Muffling:** obstruction and enclosure lower a per-voice low-pass cutoff.
  Nearby, unobstructed sounds remain clear.
- **Reverb:** distance, obstruction, and enclosure increase a short per-voice
  delay/reverb share. This is an approximation of room response, not
  convolution or traced reflections.
- If no world geometry is available, the sound uses the open-air profile.

## Category ownership

- Sound categories are Movement, UI, Weapons, NPC, Impacts, and Ambient.
- UI is excluded from spatial processing.
- Each category owns its range, attenuation points, muffling, obstruction,
  enclosure response, and reverb tuning in the centralized settings file.
- Exact sound names or prefixes may override the category default.

## Editable fields

Spatial category fields:

- `points`: `[distance, gain]` pairs. Use two or more; full volume through
  50 m means the first two points are normally `[1, 1.0]` and `[50, 1.0]`.
- `volumeScale`: final multiplier for this category.
- `directFadeStart` / `directFadeEnd`: distance where the dry direct copy
  starts fading and where it is fully replaced by delay/reverb.
- `reverb.wetNear` / `wetFar`: reverb share near and far.
- `reverb.delayMs` / `decay`: delay length and tail decay.
- `muffle.openHz` / `occludedHz` / `enclosedHz`: low-pass cutoffs.
- `obstruction.directLoss` and `reverbBoost`: how much walls reduce direct
  sound and add reverb.
- `enclosure.directLoss`, `reverbBoost`, and `probeRadius`: how enclosed
  spaces change direct sound, reverb, and geometry sampling.
- `sounds` and `prefixes`: map a sound name or prefix to a category.
- `legacy`: separate `fullVolumeDistance`, category ranges, and per-sound
  overrides used only when `mode` is `legacy`.

## Diagnostics and tuning

- The audio owner records the selected category, distance, direct gain, wet
  mix, cutoff, obstruction, and enclosure for a world voice through the
  structured audio journal.
- Logging is rate-limited; per-frame spam is not allowed.
- `audio_probe <sound> [distance] [front|left|right|rear]` plays a real world
  sound through the normal path for repeatable distance and direction checks.
- Final sound quality is accepted by human listening review; automated evidence
  proves the path and values, not perceived quality.

## Related design

- Rain and tower muffling use the same acoustic design:
  [graphics-fog.md](../graphics/graphics-fog.md).
- [World-sound spatial contract](#world-sound-spatial-behavior--final-contract)
- [Music contract](music.md)