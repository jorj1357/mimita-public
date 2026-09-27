# JSON animation keyframe loader

Date: 2026-09-21
Status: implemented slice

## Changes

- Added bounded JSON keyframe storage to the hot animation clip library.
- JSON actions may define `duration`, `loop`, and up to 32 keyframes.
- Keyframes support per-part `translation` and `rotation` arrays for torso,
  head, arms, and legs.
- The loader keeps C++ clips as the fallback and only replaces an action when
  the JSON data is valid and `behaviorSource` is `json`.
- Reloading `config/animations.json` refreshes the fixed cache at the next hot
  clip lookup without rebuilding or restarting the EXE.

## Evidence

- Immutable live build succeeded:
  `build/hotreload/mimita-live-g20260921.dll`.
- The live build path never writes `mimita.exe`.
- Tool animation references were not changed in this slice because existing
  weapon/tool pose ownership is split between `weapons.json`, WeaponConfig,
  and hot procedural carry poses; forcing a new owner without tracing that
  path could regress current behavior.
