# Dash / down-dash: no input buffer, hot-owned press edge

Date: 2026-09-23
Status: cold build succeeded; deterministic tests pass; human acceptance pending

Related regression:
`docs/regressions/2026-09-23/dash-down-dash-press-buffer-cooldown-REG.md`

## Work performed

- Removed the dash / down-dash input press buffer entirely.
  - `src/input/input-commands.cpp`: `isDashPressed()` / `isDownDashPressed()` now
    return the raw `pressed` only (no `mDashBuffer` / `mDownDashBuffer`).
  - `src/input/input-poll.cpp`: the frame carries the raw key-down state
    (`dashHeld`, `downDashHeld`) plus the raw press edge; no buffer is OR'd in.
  - `src/input/input-frame.h`: added `dashHeld` / `downDashHeld`.
  - `src/sim/simulate-tick.cpp`: the movement intent is written from the held
    fields, so the hot movement owns the press edge.
  - `src/engine/engine-tick-net.cpp`: network input uses the raw press edge.
- The hot movement already computes `dashEdge` / `downDashEdge` from the held
  intent and its own `heldPreviously` state, so the edge logic is hot-editable;
  no cold rebuild is needed to change the edge/ability semantics from now on.
- Wrote the regression record with the full investigation and the attempted fix.
- Added a loud hard rule to `docs/specs/movement/movement.md`, `AGENTS.md`, and
  `docs/architecture/collision/collision.md`:
  no cooldowns, no buffers, no timer balancing; act on the tick the press is
  sampled.
- Appended `Cold-build occurrence 2` to
  `docs/regressions/2026-09-20/cold-build-required-REG.md`.

## Evidence

Build evidence:

- Cold build `Status: SUCCESS` -> `mimita-20260922T232349.exe` (input layer is
  EXE code; the change is not hot).

Test evidence:

- `--movement-selftest`, `--movement-parity-selftest`, `--live-code-selftest`,
  `--collision-selftest`, `--afad20a-parity-selftest`: PASS.

Human acceptance:

- Pending. Rapid Q taps should each down-dash; rapid Shift taps each dash.

## Hot-reload note

Future edits to the dash/down-dash edge and ability rules are in
`src/hot-reload/modules/movement-system.cpp` and
`src/hot-reload/packages/collision/*`, which are hot. Editing them updates the
running EXE live with no cold build. Only raw key sampling
(`src/input/*`, `simulate-tick.cpp`) remains EXE-side.
