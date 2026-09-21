# Hot animation JSON timing bridge

Date: 2026-09-20
Status: implemented slice

## Changes

- Added `config/animations.json` with explicit `behaviorSource` selection.
- Kept hot C++ pose/keyframe clips as the default source.
- Added JSON action-duration overrides through the shared hot animation clip
  library. The file timestamp is checked and the values are re-read without an
  executable rebuild.
- Preserved the existing animation state machine, physical pose system, and
  versioned dynamic components.

## Evidence

- Immutable live build succeeded:
  `build/hotreload/mimita-live-g20260920.dll`.
- The live build path never writes `mimita.exe`.
- No cold executable build or process restart was performed.

## Remaining

- JSON-authored keyframe arrays and tool animation references are not yet
  migrated; this slice proves the source-selection seam with timing data.
- Full visual acceptance still requires a running executable and playtest.
