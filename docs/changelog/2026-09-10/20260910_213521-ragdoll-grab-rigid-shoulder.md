# Ragdoll: keep the shoulder rigid while grabbing (stretch only while reaching)

- Task ID: ragdoll-grab-rigid-shoulder
- Summary: Stop the grabbed arm's shoulder from becoming a one-sided
  max-distance ("stretch") joint. Stretch is now enabled only while the arm is
  reaching/extending and is not holding a grab, so a grab keeps the arm pivoting
  at the shoulder instead of floating and spinning about its center.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T21:35:21Z` (2026-09-10 17:35:21 EDT)
- Branch: `8292026stash`
- Base commit: `6141dfc` (working tree; no commit created)
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the rest of the uncommitted ragdoll work
  (`20260910_205900`, `20260910_211409`), the FFA/kill-event/countdown work, and
  the other `docs/changelog/2026-09-10/` files. Not claimed here.
- `config/ragdoll.json` hand tuning was not touched.

## Diagnosis (human playtest)

Human report: the misalignment happens **only while grabbing and holding onto
something**. The limb rotates freely during the hold, and after release it
freezes in the wrong orientation left behind by that free rotation.

Owner: `RagdollModeSystem::solveJoints`
(`src/ragdoll/ragdoll-mode.cpp`). The `211409` pass enabled the stretch joint for
both extending **and** grabbing:

```cpp
if (grabbing || extending)
    stretch = part.maxStretch;
```

With `stretch > 0` the shoulder uses `solvePointJointMaxDistance[Velocity]`
(`src/physics/physical-body.cpp`), which is a **sphere** constraint: inside the
`maxStretch` ball there is no constraint at all. While the grab pins the hand
(`solveGrabs`) and the extend motor spins the arm (`processExtend`), the arm has
no remaining shoulder constraint, so it rotates freely and its anchor slides off
the shoulder. On release, `stretch` returns to 0 and the rigid point joint
re-locks the anchor position, but a point joint never constrains relative
orientation, and `stop_angular_speed` freezes the arm where the free rotation
left it.

This is the same class as the regression recorded at
`2026-09-10T21:13:29Z — Ragdoll limbs lost their rigid attachment to the
capsule`; that entry only covered the wrong `restLength` base, not the grabbing
gate.

## Implementation change

`src/ragdoll/ragdoll-mode.cpp`, `solveJoints`, old:

```cpp
// Arms may stretch only while extending or grabbing; otherwise the
// joint is rigid so the limb stays locked at the attachment.
...
if (grabbing || extending)
    stretch = part.maxStretch;
```

new:

```cpp
// Arms may stretch only while reaching (extending) and not holding a
// grab. While grabbing the shoulder stays rigid so the pinned hand
// pivots the arm at the shoulder instead of letting it float/spin
// about its center. Otherwise the joint is rigid too.
...
if (extending && !grabbing)
    stretch = part.maxStretch;
```

No other code, config, or behavior changed. The `grabbing` local is still needed
to suppress stretch while a grab is active.

Spec note: `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md` allows a "tiny
configurable stretch/compliance" for a held grab (sections on unbreakable grips
and stability). The human explicitly decided for now: while grabbing the arm
just holds (rigid shoulder); stretch only while reaching without a grab. Body
stretch between two simultaneous grabs remains future work.

## Documents and skills

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md` — grab/reach/stretch intent.
- `docs/skills/spec-behavior-review-v1.md` — result: `PASS_WITH_HUMAN_REVIEW`.
  Finding: code enabled stretch for grabbing contrary to the human's stated
  intent for this pass. Code path: `RagdollModeSystem::solveJoints`. The held-grab
  "tiny stretch" line in the spec is informal and the human chose rigid for now;
  no spec rewrite made.
- `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`.

## Validation

- Build: `python build_agent.py` -> `BUILD SUCCESS`, return code 0, duration
  8.08s; only `src/ragdoll/ragdoll-mode.cpp` recompiled; `mimita.exe` relinked.
- Config: unchanged.
- Runtime: not performed this session.

## Human acceptance

- Confirm that while holding a grab (A/D) the grabbing arm no longer rotates
  freely and stays pivoted at the shoulder.
- Confirm that releasing the grab leaves the arm in a sensible orientation (no
  frozen wrong pose from free rotation).
- Confirm reaching with the extend mouse still stretches the arm when no grab is
  held, and that climbing still works with a rigid shoulder.
