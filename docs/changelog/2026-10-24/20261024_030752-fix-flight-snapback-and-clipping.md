# Developer Flight Bounds and Geometry Sweep

Time created: 2026-10-24T03:07:52Z

Branch: `20261011zt`
HEAD: `2103321b`
Result: `PASS_WITH_RUNTIME_REVIEW_PENDING`

## Request

Let server-authorized developer flight travel outside authored map bounds up to
9999 m/s without repeated out-of-bounds corrections, while retaining collision
against solid map geometry. Keep movement at 60 Hz, have client and server use
the same speed and Shift boost, and stop safely if the full-body sweep cannot
finish within its work budget.

## Changes

- Shared the flight speed formula: `fly <multiplier>` remains 5 m/s times the
  multiplier; the multiplier ceiling is 1999.8, Shift applies the existing 3x
  boost, and resulting speed is capped at 9999 m/s.
- Reused movement-report flag bit 18 to carry Shift state without changing
  packet size. The client and server now use the same speed helper.
- Skipped authored map-bound checks only while server-authorized flight is
  enabled. Finite/catastrophic position checks and solid-geometry validation
  remain active; flight geometry validation includes horizontal surfaces.
- Routed local flight through the actor full-body collision owner, limited to
  static map geometry and slide-only response. The continuous swept-triangle
  SAT covers long translations without adding simulation ticks. Existing pose
  rotation sampling remains bounded. If the narrowphase work budget is hit, the
  actor returns to its verified tick-start position and stops.
- Added a server flight world sweep that clips and slides along static geometry,
  including floors, as a fallback before client correction.
- Preserved the existing `aa` command alias (`fly 25`) in
  `config/command-aliases.json`.

## Files changed for this request

- `src/engine/engine-tick-net.cpp`
- `src/network/movement-validation.cpp`
- `src/network/movement-validation.h`
- `src/network/multiplayer-context.h`
- `src/network/multiplayer-tick.cpp`
- `src/network/server-packet-chat.cpp` (flight command limit only; other dirty
  hunks predate this request)
- `src/network/server-packets.cpp` (flight flag only; other dirty hunks predate
  this request)
- `src/network/server-players.cpp` (flight simulation only; other dirty hunks
  predate this request)
- `src/network/server.h` (flight input field only; other dirty hunks predate
  this request)
- `src/physics/movement/actor-triangle-solver.cpp`
- `src/physics/movement/actor-triangle-solver.h`
- `src/physics/movement/movement-step.cpp`
- `src/physics/movement/movement-step.h`
- `src/physics/movement/physics-collision-dispatch.cpp`
- `src/physics/movement/physics-collision-glb-main.cpp`
- `src/physics/movement/physics-collision-mesh.cpp`
- `src/physics/movement/physics-collision-shared.h`
- `src/physics/movement/physics-collision.h`
- `src/physics/physics-mini.cpp`
- `src/terminal/player-commands.cpp`

## Validation

- Canonical dev-loop build request succeeded: build 3002.
- The exact artifact `C:\mimita-v9\.dev\builds\3002\mimita.exe` ran
  `--versioninfo` successfully. Its preflight journal is
  `C:\mimita-v9\logs\10-23-2026\20261023_230751\events.jsonl`.
- `git diff --check` reported pre-existing whitespace in
  `config/movement.json` and `docs/jorj-docs/20261023plan.md`; it also emitted
  line-ending notices for other dirty workspace files.
- No flight runtime scenario was run. The visible dev-loop session
  `20261024_024705` remains active on build 2989 and was left untouched. Build
  and preflight identity do not establish beyond-bounds travel, wall/floor
  sliding, correction behavior, `fly 1`, the `aa` alias, or Shift parity.
  Validate those in a separate networked runtime session and inspect its live
  event journal before claiming runtime acceptance.

## Existing workspace state

The checkout had unrelated dirty files and untracked assets before this work.
Those files were preserved. The listed server files also contain unrelated
pre-existing changes; only the flight hunks above belong to this request.
