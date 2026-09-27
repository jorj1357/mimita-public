# Dash / down-dash press-buffer cooldown

Time created: 2026-09-23T03:27:04Z
Time last updated: 2026-09-23T03:27:04Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/movement/movement.md`

Related changelog:
`docs/changelog/2026-09-23/20260923_031846-dash-down-dash-press-buffer-one-shot.md`

Reference comparison:
`afad20a` versus the current working tree. At `afad20a`, pressing Q on the
ground down-dashed on every fresh press with no cooldown or input buffer.

---

## Regression Occurrence 1

### Observed

Time: `2026-09-23T03:00:00Z` (approximate, from human report)

The human reported: "I press q down super fast over and over, no down dash, but
waiting a little between each press, it does it every time. Is there like a
cooldown? Like if 0.1 sec have not passed then the down dash doesn't work?"

### Expected Behavior

Per `docs/specs/movement/movement.md`: down-dash is edge-triggered. A fresh Q
press down-dashes immediately, with no cooldown and no timer. Repeated Q presses
each down-dash. The same applies to Shift dash. The input path must not delay,
merge, or rate-limit a press.

### Actual Behavior

A second Q press within ~150 ms of the first produced no down-dash. Waiting
longer than ~150 ms made every press work. The same applied to Shift dash.

### Why This Is Bad

It is a hidden cooldown/rate limit on a core movement ability. It makes rapid,
deliberate chaining impossible, directly violating the movement goal of
immediate, exploitable, chainable input with "no cooldown or timer balancing".

### Specification

`docs/specs/movement/movement.md`

Relevant requirement: instant input; abilities act on the fresh press; no
cooldown timer; touching anything resets abilities. The specification is the
authority; the input buffer was code that contradicted it.

### Wrong Code

File:

`src/input/input-poll.cpp`

```cpp
frame.dashPressed = cmd.isDashPressed() || gTerminalInputOverride.dashPressed;
frame.downDashPressed = cmd.isDownDashPressed() || gTerminalInputOverride.downDashPressed;
```

File:

`src/input/input-commands.cpp`

```cpp
bool InputCommandSystem::isDashPressed() const {
    return getState("dash").pressed || mDashBuffer.active;
}
bool InputCommandSystem::isDownDashPressed() const {
    return getState("down_dash").pressed || mDownDashBuffer.active;
}
```

File:

`src/input/input-commands.h`

```cpp
double bufferSeconds = 0.15; // 150ms buffer
```

### Confirmed Cause

`InputCommandSystem` buffered dash and down-dash presses for 150 ms
(`BufferedAction::bufferSeconds = 0.15`). `isDashPressed()` /
`isDownDashPressed()` returned `pressed || buffer.active`, and that signal was
written into the movement intent as a HELD bool. The hot movement's edge
detection (`dashEdge = mi.dash && !dashHeldPreviously`,
`downDashEdge = mi.downDash && !downDashHeldPreviously`) therefore saw the held
signal stay true for the whole 150 ms window, so a second press inside that
window produced no new edge. This is the exact "0.1 sec cooldown" the human felt.

Evidence:

- Source: `src/input/input-commands.cpp` buffer set/expire logic and
  `bufferSeconds = 0.15`.
- Source: `src/hot-reload/modules/movement-system.cpp` edge detection consuming
  the held intent bool.
- Human report timing (~0.1 s) matches `bufferSeconds = 0.15`.

### What Was Checked / Tried Before the Cause Was Found

1. Checked the hot movement for a cooldown gate: `rs.dashCooldownSeconds` is
   decremented but is not used in the dash/down-dash activation gate (removed in
   the afad20a restoration). So the hot gate is not the cause.
2. Checked the afad20a oracle: `tryActivateDownDash` acts purely on
   `command.downDashPressed` with availability; no cooldown.
3. Checked ability reset / grounding: the journal showed
   `movement.contact_ability touch=1 grounded=1 collided=1 ... restored=1`, and
   `down_dash_available=1` on every `q_down`, so availability was not the cause.
4. Inspected the input layer and found the 150 ms press buffer OR'd into the
   held signal, which was the actual cause.

### Attempted Fix 1

Time: `2026-09-23T03:18:46Z`

Change:

Made the dash/down-dash buffer a one-shot consumed in `buildInputFrame`.

Result:

Cold build succeeded; deterministic tests passed. This removed the merging but
kept the buffer concept, and it was still input-layer (cold) code.

### Corrected Code

File:

`src/input/input-poll.cpp`

```cpp
// No buffer and no cooldown: the frame carries the raw key-down state and the
// hot movement owns the press edge, so a press acts as fast as possible.
frame.dashHeld = cmd.getState("dash").held;
frame.downDashHeld = cmd.getState("down_dash").held;
frame.dashPressed = cmd.getState("dash").pressed || gTerminalInputOverride.dashPressed;
frame.downDashPressed = cmd.getState("down_dash").pressed || gTerminalInputOverride.downDashPressed;
```

File:

`src/sim/simulate-tick.cpp`

```cpp
Ecs::setMovementIntent(playerEntity, frame.moveX, frame.moveY,
                       frame.movementPressed, frame.jump, frame.dashHeld,
                       frame.downDashHeld, frame.freezeHeld);
```

File:

`src/input/input-commands.cpp`

```cpp
bool InputCommandSystem::isDashPressed() const {
    return getState("dash").pressed;
}
bool InputCommandSystem::isDownDashPressed() const {
    return getState("down_dash").pressed;
}
```

File:

`src/engine/engine-tick-net.cpp`

```cpp
mpInput.dashPressed = cmd.getState("dash").pressed;
mpInput.downDashPressed = cmd.getState("down_dash").pressed;
```

### Fix

Dash and down-dash are now pure raw-key edges. The input frame carries the raw
key-down state (`dashHeld`, `downDashHeld`) into the movement intent, and the hot
movement owns the press edge (down transition). There is no buffer and no
cooldown anywhere on the dash/down-dash path. The press acts on the tick it is
sampled.

### Proof

Human review:

Pending. Rapid Q taps should each down-dash; rapid Shift taps should each dash.

Automated proof:

- Cold build `Status: SUCCESS` -> `mimita-20260922T232349.exe`.
- `--movement-selftest`, `--movement-parity-selftest`, `--live-code-selftest`,
  `--collision-selftest`, `--afad20a-parity-selftest`: PASS.

### Solution

`ATTEMPTED FIX (1)` — awaiting human confirmation. The buffer is removed; the
press edge is owned by the hot movement.

---

## Rule Derived From This Regression

There is **no 150 ms buffer**, and there are **no cooldowns or timer
balancing** on movement abilities. If the user presses a button, do the thing as
fast as possible on the tick it is sampled. See the loud rule in
`docs/specs/movement/movement.md`, `AGENTS.md`, and
`docs/architecture/collision/collision.md`.
