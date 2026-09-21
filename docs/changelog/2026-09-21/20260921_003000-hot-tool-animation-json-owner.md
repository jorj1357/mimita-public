# Hot tool animation JSON owner

## Change

Traced the live tool animation path and added an optional JSON phase cache at
the existing hot tool-visual owner. `pose-generation.cpp` continues to select
the phase; `tool-visuals.cpp` now supplies the phase from `config/animations.json`
when `behaviorSource` is `json`, with the existing C++ phase tables as the
fallback. The cache is bounded and keeps stable phase/keyframe storage.

## Evidence

- The hot DLL built successfully as `build/hotreload/mimita-live-g20260921.dll`.
- `git diff --check` reported no whitespace errors.
- The active default remains `behaviorSource: cpp`, so this slice does not
  change the current presentation until JSON mode is selected.
- `mimita.exe` is not present in this checkout, so live activation and visual
  acceptance could not be observed.

## Next

Add complete migrated tool phase data from the v2.0.6 reference into
`config/animations.json`, then run a live switch test from C++ to JSON and back
without restarting the world or process.
