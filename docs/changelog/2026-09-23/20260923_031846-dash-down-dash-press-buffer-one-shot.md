# Dash / down-dash press buffer made a one-shot edge (rapid Q works)

Date: 2026-09-23
Status: cold build succeeded; deterministic tests pass; human acceptance pending

## Reported symptom

Pressing Q very fast produced no down-dash, but waiting a little between presses
worked every time. It felt like a ~0.1 s cooldown.

## Cause

`InputCommandSystem` keeps a press buffer (`BufferedAction::bufferSeconds = 0.15`
= 150 ms). `isDownDashPressed()` returned `pressed || mDownDashBuffer.active`, and
that signal was written into the movement intent as a held bool. The hot edge
detection (`downDashEdge = mi.downDash && !downDashHeldPreviously`) therefore saw
the held signal stay true for the whole 150 ms window, so a second press inside
that window produced no new edge. The same applied to Shift dash.

## Fix

`src/input/input-poll.cpp` `buildInputFrame` now consumes the dash and down-dash
buffers **unconditionally** (one-shot) and ORs the raw press with the consumed
pulse:

```cpp
const bool dashBuffered = cmd.consumeBufferedDash();
const bool downDashBuffered = cmd.consumeBufferedDownDash();
frame.dashPressed = cmd.getState("dash").pressed || dashBuffered || override;
frame.downDashPressed = cmd.getState("down_dash").pressed || downDashBuffered || override;
```

- The buffer still covers a press that lands between fixed ticks (the pulse is
  consumed on the next frame and delivered to that frame's ticks).
- The held signal is now true for a single frame per press, so each fresh press
  produces a new edge and rapid taps each fire.
- Consuming unconditionally avoids the `||` short-circuit leaving the buffer
  armed and producing a second pulse.

`cmd` in `buildInputFrame` changed from `const` to a non-const reference so the
consume calls compile.

## Evidence

Build evidence:

- Cold build `Status: SUCCESS` -> `mimita-20260922T231811.exe`. This is input
  layer (EXE) code, so the fix needs the new EXE; it is not hot.

Test evidence:

- `--movement-selftest`, `--movement-parity-selftest`, `--live-code-selftest`,
  `--collision-selftest`, `--afad20a-parity-selftest`: PASS.

Human acceptance:

- Pending. Rapid Q taps should each down-dash; rapid Shift taps should each dash.

## Limits

- The buffer duration is still 150 ms; it now only delays a press to the next
  frame, not merge presses.
- The buffer is still shared with `pollInputState` (hot `input.read`); the
  consume happens in `buildInputFrame`, which is the movement's source.
