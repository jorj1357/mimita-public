# Animation marker retiming implementation

- Local time: 2026-10-19 16:47 EDT (UTC-04:00)
- UTC: 2026-10-19 20:47 UTC
- Branch/starting commit: `20261011zt` / `35c9b12a`
- Task: implement Melee marker retiming from Blender-authored source poses through the animation exporter, gameplay phase timeline, and animation sampler.

## Changes

- `tools/mimita_animation_exporter.py:42,166-223,491-556` now exports both `tick` and `sourceTick`, offers preserve/reset when re-exporting an existing Melee clip, validates marker order, updates runtime `durationTicks`, and prints both timing timelines.
  - Before: `markers.append({"name": marker.name, "tick": tick})`
  - After: `entry = {"name": marker.name, "tick": tick}` and required Melee markers add `entry["sourceTick"] = tick`.
- `src/entities/player.h:223-227` stores runtime marker ticks, authored source ticks, and whether the clip has a Melee timeline.
  - Before: `struct AnimClip` had no Melee phase timeline fields.
  - After: `std::array<int, 4> phaseTicks`, `phaseSourceTicks`, and `hasMeleePhaseTimeline` are part of the clip.
- `src/entities/player-animation-config.cpp:189-266` validates required markers and source ticks when parsing clips, retaining identity timing for legacy clips with no `sourceTick`.
  - Before: marker values were not parsed into `AnimClip`.
  - After: runtime/source marker arrays are validated and malformed Melee timelines are rejected.
- `src/entities/player-animation.cpp:52-67,678-716` maps runtime ticks piecewise linearly to source ticks and samples the existing pose curve at the mapped fractional tick.
  - Before: a locked clip sampled directly at `(float)lockedClipSampleTick`.
  - After: `mapRuntimeTickToAuthoredTick(*lc, runtimeTick)` selects the authored sample tick for Melee clips.
- `src/combat/weapon-melee-config.h:130-132` adds source boundary storage to parsed Melee actions.
  - Before: actions had no authored phase boundaries.
  - After: `animationSourceTicks` stores the four Blender-authored marker ticks.
- `src/combat/weapon-melee-config.cpp:32-91,317-330` parses and validates marker `sourceTick` values into each Melee action.
  - Before: clip parsing returned duration and runtime phase starts only.
  - After: the loader also returns the ordered authored source boundaries.
- `src/combat/melee-client.cpp:457-478` adds source span and phase speed ratio to the existing once-per-phase diagnostic.
  - Before: the event held the runtime phase/tick and clip only.
  - After: `phase_speed_ratio` is calculated from authored span divided by runtime phase length.
- `src/combat/weapon-json-config.cpp:67-112,438-446,509-533` detects edits to JSON files under the animation library and republishes Melee move sets through the existing weapon reload path.
  - Before: `reloadBuiltinWeaponsIfChanged()` returned early whenever `config/weapons.json` was unchanged.
  - After: a changed animation library file also reloads move sets; actions already in progress retain their copied action snapshot.
- `docs/specs/animations/animations.md:798-801` now requires Melee move-set timing and pose mapping to reload together after a library clip edit.
  - Before: the hot-reload list ended with rig/model import behavior.
  - After: item 7 specifies that new actions use edited phase boundaries while active actions keep their captured timing.
- Six Captain Falcon clips now carry `sourceTick`: `config/animations/library/melee/captainfalcon/{downkick_a,downpunch_a,kick_a,punch_a,upkick_a,uppunch_a}.json`.
  - Before: required markers carried only `tick`.
  - After: each marker carries an authored source boundary. In `kick_a.json:457-473`, active remains playback `tick: 2` and has `sourceTick: 20`; other files initially set source values equal to their existing ticks.

## Validation and runtime evidence

- Python AST parsing succeeded for the exporter.
- All six Captain Falcon Melee clip JSON files parsed, and their source markers are present and ordered.
- Focused `git diff --check` passed for the implementation/spec/clip files; Git reported line-ending notices only. A repository-wide check also finds pre-existing trailing whitespace in the user-edited Melee spec line about crouch, which this task did not change.
- Canonical dev-loop build succeeded as build `2615`; status reported `source_current=true`. Published executable: `.dev/builds/2615/mimita.exe`.
- Ran that exact executable with `--versioninfo`; it reported build date/time `Oct 19 2026 16:39:00` and journal `logs/10-19-2026/20261019_164747/events.jsonl` (run `20261019_164747`).
- Started build 2615 through the existing dev loop. The real `agent.melee a_air_kick` command completed, and the server journal recorded the NPC's `a_air_kick` action start.
- A content-preserving timestamp change to `kick_a.json`, followed by restoring its original timestamp, caused both client and server to publish move-set revision 2 and then revision 3. The client also emitted `anim.set.reloaded`; the file content was not changed by this reload check.
- The repository's `melee_combat_v1.json` scenario did not complete: it invokes client terminal command `meleeattack`, which the client reports as unknown. This is recorded as scenario failure, not as a feature pass.
- Build and structured logs do not confirm the animation's visual feel. Human gameplay review remains open.

## Preserved workspace state

Other existing or concurrently changed user files remain untouched, including `config/weapons.json`, account/analytics/audio configuration, and `docs/specs/gamemodes/supersmashbrosmelee.md`.
