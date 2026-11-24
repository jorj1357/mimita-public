# Config limits → warning-first audit (issue-9e73745d3154)

Date: 2026-11-24 22:20 UTC
Agent: hot-reload-latency-root
Context: ctx-41260d9f9184 (high)
Issue: issue-9e73745d3154 — "Replace arbitrary configuration limits with
warnings and best-effort evaluation"

Read-only source audit. No source edit, build, or runtime run in this
activation. The user asked to remove arbitrary limits, replace rejection with
warnings, and compute the actual result for negative / 999999 inputs.

## Requested policy

For each user-editable numeric field:

1. Accept the requested finite value.
2. Compute the owning feature's best-effort effective result.
3. Emit one warning when `effective != requested` (or semantics are undefined),
   naming the key, requested value, effective value, and reason.
4. Keep only the minimal point-of-use safety guard needed to prevent NaN
   propagation, overflow, OOM, or an unbounded loop — and warn when it fires.
5. Reject only malformed JSON / wrong type, never a lone out-of-range number.

## Current behavior classes

### A. Silent clamps on feel/tuning values (highest user impact)

These are safe to accept broadly; today a stray value is silently rewritten.

- Bodycam response seconds: `src/gamemode/gamemode.cpp:719-730` clamps
  `position/rotation/head/torso/arm_response_seconds` and
  `weapon_stability_seconds` to `[0.01, 2.0]`; `src/camera.cpp:249-251`
  clamps the camera responses again. Example: `rotation_response_seconds`
  `-1` → `0.01` (instant snap), `0.5` → `0.5` (works), `999999` → `2.0`
  (maximum smoothing). No warning. This is the exact field the user tuned in
  `config/gamemodes/bloodtest.json`.
- Camera lens/sway: `src/config/camera-config.cpp:115,133-138,180` clamp to
  hard ranges; non-finite values are silently replaced with defaults
  (`123-128`, `152-165`, `178-187`).
- Player FOV / sensitivity / slot: `src/config/player-settings.cpp:69-71`
  clamp `[60,140]`, `[0.01,2]`, `[0,10]` with no warning.
- Movement tuning: `src/config/movement-config.cpp:306-465` many `std::clamp`
  calls (strafe angles, sprint multiplier `1-4`, stamina `1-1000`, etc.).
- Killfeed colors: `src/config/killfeed-config.cpp:37-40` clamp `[0,1]`.
- Weapon tracers: `src/config/weapon-tracers-config.cpp:62-75` clamp
  alpha/thickness/segments.
- Impact decals: `src/config/impact-decals-config.cpp:159-160` clamp
  `countMin/countMax` to `64`.
- Spatial audio: `src/config/spatial-audio-config.cpp:262-277` many clamps.
- Networking: `src/config/networking-config.cpp:67-102`
  `clampMin/clampRange/readUintRange/readSizeRange/readMsRange` used
  throughput the file (e.g. `235-762`).

### B. Whole-file hard rejects (a single bad number discards every other edit)

- Player visuals: `src/config/player-visuals-config.cpp:53,82,119,137,239`
  return `false` on any out-of-range/non-finite value; `257-258` logs
  `player_visuals.config.rejected` and keeps the previous file. One bad color
  or thickness discards all other valid changes.
- Camera `proximityFade`: `src/config/camera-config.cpp:88-92` throws on
  negative or `end <= start`; `206-213` keeps the previous camera config.
- Collision `weapon_vs_weapon`: `src/config/collision-config.cpp:57-104`
  throws on out-of-range clash values; `245-246` retains the previous file.
- Actor collision: `src/config/actor-collision-config.cpp:166-213` rejects the
  whole file on non-positive radius/height; `344` retains previous.
- Material: `src/config/material-config.cpp:101` throws on
  `cutThresholdJ`; `127` keeps the previous table.
- Gamemode enums (aim/dash mode): `src/config/gameplay-config.cpp:133-164`
  keep the previous whole config on an unknown string.

### C. Structural / resource-critical values

A bounded guard is justified to prevent OOM/hang (e.g. `countMin/countMax`
allocations, `maxSubSteps` loops, `maxCandidatePairs`, buffered snapshot
counts, chunk/socket sizes, tracer `segments`). These must still warn with the
effective value and reason `safety`, not clamp silently.

## Negative and 999999 semantics (examples)

- `response_seconds`: `<= 0` means "instant"; best-effort is `0` (not a
  nonsensical negative time constant). Large values saturate visually; only
  cap where the integration step would overflow.
- `fov`: negative is out of physical range; compute the projection with the
  magnitude and warn, rather than silently jumping to `60`.
- `lens_distortion` / `curve` / `zoom`: evaluate in double precision; guard
  only against division by zero and non-finite output.
- `color` / alpha `999999`: compute clamped-channel result and warn; do not
  reject the file.
- Allocation/loop counts `999999`: keep the safety ceiling, warn
  `requested=999999 effective=<ceiling> reason=safety`.

## Proposed slices (need human approval before any code edit)

1. Add one shared resolver, e.g. `src/config/config-value.h`:
   `resolveNumber(key, requested, semanticRule, safetyBound, category)` returns
   the effective value and logs exactly one WARN per changed key per load
   (`key requested=X effective=Y reason=semantic|safety computed=<note>`).
   Pure function, unit-testable.
2. Migrate class A first (gamemode bodycam, camera config, player settings,
   movement, killfeed, tracers, decals, audio). `-1` and `999999` compute a
   documented result; guard only overflow/NaN.
3. Convert class B whole-file rejects into per-field warnings that keep the
   other fields; keep reject only for malformed JSON / wrong type.
4. Publish explicit class C exceptions with a measured reason; still warn.
5. Emit one per-load summary event listing every changed key (requested vs
   effective) so humans and agents can diff.

## Evidence status

- Source evidence: direct `file:line` above (read-only).
- Runtime evidence: none in this activation. Acceptance still requires a
  pinned dev-loop run journal showing accepted value, warning, and computed
  result for representative negative / 999999 inputs.
- Unknowns: the full inventory spans ~60 config files and 900+ read sites;
  structural safe bounds need per-owner confirmation.

## Next step

Route slices 1-2 to the owning agent after human approval; run the bounded
negative/999999 matrix against a pinned generation and attach the exact
`events.jsonl` to issue-9e73745d3154.
