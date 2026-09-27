# 2026-09-22 → 2026-09-23 — afad20a behavior parity, the input-buffer lesson, and hot-reloadable feel

Status: `GOLD / REFERENCE` — the human-observed behavior-parity push from
`afad20a`, the biggest win (removing the dash/down-dash input buffer), and the
hot-reload direction that makes future fixes fast. Written from the human's
perspective, as a record of intent, not just of code.

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11). This is the
project's behavioral reference: the behavior a human remembers and wants back.

## The goal in one sentence

Make the behavior a human *observes* in the running game match `afad20a`, while
the code underneath stays hot-reloadable, so the time between "I want this" and
"I can see it" shrinks toward zero.

The human's framing:

> I want to shrink the time between intent and observable outcome accurate to
> that intent as much as possible, like get it down to milliseconds,
> microseconds, as small as we can.

Two levers were named for that:

- **Lowering inputs** — using AI to write and change code instead of typing all
  of it by hand.
- **Raising outputs** — the AI does not get sleepy or need rest; it can just
  keep going.

This is the same "live runtime" direction as
`docs/gold/2026-09-13-live-runtime-generic-bootstrap.md`: edit the running
system, see the change, keep the same EXE/world/session.

## The loop that produced this work

Every fix followed the same human loop:

1. **Observe** in the running game ("I press Q fast and nothing happens").
2. **Describe** the intended behavior in plain words.
3. **Trace** the full data path before blaming the binary.
4. **Fix** the smallest owner of the wrong step.
5. **Re-observe** in the running game.

The most valuable skill in this stretch was not writing code; it was turning a
vague "this feels wrong" into the exact tick, flag, or value that was wrong. The
journal (`events.jsonl`) and the throttled movement/collision records were the
tools that made that possible.

## The biggest thing: the 150 ms input buffer

This was the single most frustrating bug and the one the human most wants
remembered.

**What the human felt:** rapid Q (down-dash) and Shift (dash) presses did
nothing; waiting a little between presses worked every time. It felt like a
~0.1 s cooldown.

**What it actually was:** `InputCommandSystem` buffered dash/down-dash presses
for `150 ms` (`BufferedAction::bufferSeconds = 0.15`). `isDashPressed()` /
`isDownDashPressed()` returned `pressed || buffer.active`, and that signal was
written into the movement intent as a **held** bool. The hot movement's edge
detection (`downDashEdge = mi.downDash && !downDashHeldPreviously`) therefore saw
the held signal stay true for the whole 150 ms window, so a second press inside
that window produced no new edge.

**The fix:** the input frame now carries the **raw key-down state**
(`dashHeld`, `downDashHeld`) plus the raw press edge. The hot movement owns the
press edge. There is no buffer and no cooldown anywhere on the dash/down-dash
path; the press acts on the tick it is sampled.

Full investigation and the attempted intermediate fix are in
`docs/regressions/2026-09-23/dash-down-dash-press-buffer-cooldown-REG.md`.

**The rule that came out of it** (now loud in `AGENTS.md`,
`docs/specs/movement/movement.md`, and
`docs/architecture/collision/collision.md`):

> If the user presses a button, do the thing on the tick it is sampled. No input
> buffer, no cooldown, no timer-based gating anywhere in the movement path.
> Abilities are gated only by availability and a fresh input edge.

The human's own words, kept because they carry the intent better than a spec:

> I was working on making the behavior I observe as a human match the afad20a
> commit, the behavior there. The biggest thing was there was an added input
> buffer — no input buffer, that was frustrating and made it so I can't dash and
> down-dash over and over even though I like to do that, and that is the behavior
> I want. Dashing over and over should be possible.

So the desired behavior is explicit: **repeated dash and down-dash are a
feature, not a bug.** Touching anything resets the ability, and a fresh press
fires immediately, every time.

## What the 9/22 → 9/23 parity push covered

The work moved through the afad20a restoration phase by phase (changelogs under
`docs/changelog/2026-09-22/` and `docs/changelog/2026-09-23/`):

- **Movement** — the afad20a movement oracle and its parity harness; the Source
  preset values; grounded contact and the ability reset restored.
- **Collision / limbs** — world-space per-limb colliders matching afad20a
  (`docs/gold/2026-09-23-afad20a-limb-collision-method.md`); whole-limb capsules;
  permissive touched-world ability reset; wall/ceiling bounce; ground settle;
  limb-height settle; collision logging (`limbSrc`/`limbCols`/`limbHits`).
- **Dash / down-dash** — the buffer regression above, then the hot-owned press
  edge.
- **Freeze** — afad20a freeze state persisted across the hot boundary; activation
  hard-stop; collision pass-through suppression with momentum preserved;
  availability consumed on use and restored by contact.
- **Grounded rest** — afad20a's landing vertical snap
  (`groundSnap && |vel.z| <= velocityClipEpsilon(1.01) -> vel.z = 0`) and its
  always-on `doGroundSnap`/`doFloorRecovery`, so a standing actor rests instead of
  oscillating; `groundResponse: "bounce"` is now the afad20a collision+grounded
  mode.
- **Landing sound** — plays only on the airborne -> grounded transition (each
  landing), no cooldown, naturally capped at one per tick; volume scales with
  impact speed.
- **Animation / poses** — afad20a animation evaluator, per-weapon arm pose table
  (still not 1:1; see limits).
- **Weapons** — v2.0.6 and afad20a weapon parity references.

## The small change with outsized feel: random walk sounds

The human calls this out deliberately, because on paper it looks insignificant.

> This current edit made the sounds randomized, and the notable thing about that
> is it seems insignificant — just sounds being random — but when it's random it
> gives novelty every time, so you never know what sound it'll do next. It adds
> extra spice to the game and makes it more fun.

What changed: the hot footstep composer picked `entity/player/walk1..4` in a
fixed `s_step++ % 4` cycle, so it was always the same 1-2-3-4 order. It is now a
random pick (`1 + rand()%4`) each step; the cadence, volume, and pitch logic are
unchanged.

The lesson is about **perceived variety**. Determinism is essential for gameplay
authority and tests; *presentation* (which of several equivalent sounds plays) is
where randomness is pure upside — it hides repetition and keeps the moment
novel. Keep the randomness on the presentation side only.

## Still open

- **Jump sounds and jump effects** are not done here; the human says that is
  being handled by another agent (GPT 5.6 "luna medium"). This document records
  it as the next expected slice, not as finished work.
- **Animation is not 1:1 with afad20a** (idle/walk/return-to-idle, dash/freeze
  pose overlays, exact springs) — only hot-reloadable with a C++/JSON selector.
- Human live acceptance of the grounded rest, landing sound, random walk, and
  dash/down-dash chaining is pending; the deterministic tests pass.

## Why this is gold

- The **behavior** is back to `afad20a` where it matters to a human (dash/down-
  dash chaining, grounded rest, freeze, landing sound), and the **code** that
  produces it is hot-reloadable.
- Future feel bugs can be edited in the raw hot C++ and observed live, instead of
  the slow loop: edit -> wait for rebuild -> test -> bug still exists -> rebuild
  -> still exists.
- The input-buffer regression is recorded so it is never reintroduced: no
  buffers, no cooldowns, no timer balancing on movement abilities.

## References

- `docs/regressions/2026-09-23/dash-down-dash-press-buffer-cooldown-REG.md`
- `docs/changelog/2026-09-23/20260923_032941-dash-down-dash-no-buffer-hot-edge.md`
- `docs/changelog/2026-09-23/20260923_101537-afad20a-freeze-grounded-hot.md`
- `docs/changelog/2026-09-23/20260923_130654-afad20a-grounded-rest-land-sound-random-walk.md`
- `docs/gold/2026-09-23-afad20a-limb-collision-method.md`
- `docs/gold/2026-09-21-hot-reload-progress-and-v206-parity.md`
- `docs/gold/2026-09-13-live-runtime-generic-bootstrap.md`
