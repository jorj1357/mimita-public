# Entity Marker Visibility Fix

Time UTC: 2026-10-08T18:33:12.502Z
Branch: `afad20a-rebuild`
Commit: none created; local changes remain uncommitted.

## Diagnosis

`config/maps/zombietower4.json` contained entity `test1` with `visible: true`,
but `MapConfigRegistry::mEntityVisibility` remained false after JSON loading.
The renderer's entity loop is gated by that registry-wide flag, so the entity
was never drawn. This was the first missing step in the JSON-to-visible-marker
path.

## Changes

- `src/gamemode/map-config.cpp` now enables the registry entity-visibility
  flag when a valid map load contains at least one visible entity.
- `src/debug/debug-visuals-scene.cpp` now draws a wire cube for every visible
  entity. It uses the authored size when valid and falls back to a 1 m cube
  when a size component is missing, non-finite, or non-positive. Existing
  spawnpoint crosses, pickup spheres, and radius overlays remain.
- `src/debug/debug-visuals.h` and `src/debug/debug-visuals-labels.cpp` add a
  distance-faded world-label path. Entity labels are 50% opaque at zero
  distance and fade linearly to 0% at 50 m.

## Validation

BUILD: `mimita-20261008T1430-entity-marker-fix.exe` built successfully with
137 translation units compiled, 412 skipped, and return code 0.

IDENTITY: `--versioninfo` passed with
`EVENTS_JSONL_PATH=logs/10-08-2026/20261008_143308/events.jsonl`.

COMPONENT: `--map-config-selftest` passed. Target JSON parsing and the changed
source passed focused diff checks; Git reported only normal LF/CRLF conversion
warnings.

RUNTIME: No live visual acceptance is claimed. Native access to the Windows
game window is unavailable in this environment. Human verification should
launch the named executable, load Zombie Tower 4, confirm `test1` appears as a
green 1 m debug cube with its world label, edit its position/visibility in the
JSON, and confirm the marker hot-reloads.

## Preserved work

Unrelated pre-existing worktree edits were preserved and are not attributed to
this session. No regression record was created because no human-confirmed
regression was established.
