# Source Air Movement and Human Understanding

Time created: 2026-09-22T20:54:22Z
Time last updated: 2026-09-22T20:54:22Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/movement/movement.md`

Related changelog:
`docs/changelog/2026-09-22/20260922_205422-source-air-movement-json-live.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-22T20:54:22Z`

The human maintainer, `jorj1357`, reported that airborne WASD still felt like
direct Roblox-style air movement. The desired `movement-source.json` behavior
is CS/GoldSrc-style air movement: WASD expresses a wish direction, but does not
directly move the actor through the air; mouse turning while moving creates the
air-strafe speed gain.

The human also reported that the repository is difficult to understand while
editing and wants to record that problem without making edits artificially slow.

### Expected Behavior

The source movement JSON remains the authoritative tuning and mode input. The
hot C++ movement policy interprets it so airborne WASD cannot launch an actor
from rest, while a changing wish direction can gain speed against existing
horizontal velocity.

### Actual Behavior

The active JSON file was being found, but `movement_mode: "v206"` selected a hot
branch that directly accelerated toward the WASD wish direction. The branch did
not honor the intended no-launch air-strafe behavior.

### Why This Is Bad

The live game feel did not match the selected source movement behavior or the
human's remembered CS/1.6-style movement.

### Wrong Code

File:
`src/hot-reload/modules/movement-air.cpp`

The previous v206 branch always applied velocity toward `wishDir` whenever
there was remaining speed:

```cpp
const float addSpeed = in.wishSpeed - projected;
float accelSpeed = in.airAcceleration * in.wishSpeed * in.dt * in.surfaceFriction;
outVelocity[0] = in.velocity[0] + in.wishDir[0] * accelSpeed;
outVelocity[1] = in.velocity[1] + in.wishDir[1] * accelSpeed;
```

### Confirmed Cause

The hot v206 branch implemented direct additive air movement instead of the
projection-limited, no-launch air-strafe rule. The JSON path itself was also
previously ambiguous; it is now centralized to `movement-source.json`.

Evidence:

- `config/movement.json` selects `behaviorSource: "json"` and preset `source`.
- The resolver maps `source` to `config/movement/movement-source.json`.
- `movement.main` passes `movementModel = 1` to the hot air policy.
- The old `movementModel == 1` branch added velocity directly toward `wishDir`.

### Attempted Fix 1

Time:
`2026-09-22T20:54:22Z`

Change:

- Kept `movement-source.json` values unchanged.
- Kept `movement-cs.json` out of the path.
- Changed the hot v206 air branch to refuse launch from near-zero horizontal
  velocity and use projection-limited acceleration against existing velocity.
- Kept JSON path loading and live timestamp tracking on the shared resolver.

Result:

Hot candidate build and automated proof are pending human in-game review.

### Corrected Code

File:
`src/hot-reload/modules/movement-air.cpp`

```cpp
const float horizontalSpeed = std::sqrt(
    in.velocity[0] * in.velocity[0] +
    in.velocity[1] * in.velocity[1]);
if (horizontalSpeed <= 0.1f)
    return;

const float projected =
    in.velocity[0] * in.wishDir[0] + in.velocity[1] * in.wishDir[1];
const float addSpeed = in.wishspd - projected;
```

### Fix

The source preset now keeps its values in JSON while the hot C++ policy uses
those values with the intended air-strafe rule. The actor must already have
horizontal movement before airborne WASD can contribute; changing the wish
direction through mouse steering can then create speed gain.

### Proof

Human review:

Required: test live in the same running game and verify standstill jump plus
WASD does not produce direct horizontal air movement, while moving and turning
the mouse produces CS-style strafe gain.

Automated proof:

Hot candidate generation 5 built successfully as
`build/hotreload/p24400/gen5/mimita-live-g000005.dll`. The live-build path did
not write `mimita.exe`. Focused movement behavior tests and human in-game feel
review remain required.

### Human Understanding Note

The maintainer stated that the repository is hard to understand while editing
and that slower editing is not preferred merely for its own sake. The proposed
solution is not to stop work or require perfect understanding before every
change. Instead, each change should be small, live-testable, source-traceable,
and recorded with the exact owner, input, output, and rollback point. This lets
rapid work continue while reducing accidental behavior changes.

### Solution

Not confirmed. Human live testing is still required.
