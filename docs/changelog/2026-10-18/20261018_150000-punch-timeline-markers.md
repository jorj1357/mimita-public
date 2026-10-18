# Add movable phase markers to Captain Falcon punch source

- Timestamp: 2026-10-18 15:00 EDT
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and result

Added one set of general, movable phase markers to the user's Blender scene.
Markers are scene timeline metadata shared by all six part Actions; no limb was
selected or changed. Backed up the source before saving. Re-exported the clip
with the marker values in the shared JSON.

The marker positions are neutral starter placeholders, spaced evenly across
the 60-tick clip: windup tick 0, active tick 20, recovery tick 40, and exclusive
end tick 60. They are easy to move in Blender. These positions do not claim to
be the final hit timing for the punch.

## Files and exact changes

### Blender source: `C:\important\quiet\blender-projects\mimita sizing 6.blend`

- Before: scene range 1–60, no timeline markers; six part Actions each covered
  frames 1–60.
- After: scene end is frame 61 to show the exclusive end marker. Added
  `phase.windup` at frame 1, `phase.active` at frame 21,
  `phase.recovery` at frame 41, and `action.end` at frame 61. Existing Actions
  remain unchanged.

### Backup: `C:\important\quiet\blender-projects\mimita sizing 6.before-mimita-markers.blend`

- New backup made immediately before editing; it retains the original 1–60
  scene range and no markers.

### `config/animations/library/melee/captainfalcon/punch_a.json`

- Before: no `markers` array.
- After: lines 7–12 include the four marker names at ticks 0, 20, 40, and 60.
  Duration remains 60; authored poses remain at ticks 0 and 59.

## Validation and remaining implementation gap

- Reopened the saved `.blend` with Blender 5.0 and exported it with the
  standalone exporter in Melee mode.
- Confirmed exported marker ticks are 0/20/40/60 and keyframe ticks remain
  0/59. JSON parsing and `git diff --check` passed.
- The current melee game runtime still does not consume clip markers; it uses
  the existing weapon-config phase ticks and phase clips. The new markers are
  ready in the authoring asset and JSON, but do not yet drive gameplay timing.
- No game build or in-game test was run.
