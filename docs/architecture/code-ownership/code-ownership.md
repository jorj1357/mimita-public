# Architecture Direction

**Purpose:** guide ownership, code placement, and the order in which an
implementation should change the codebase.

**Audience:** contributors designing or changing runtime systems, tools, and
shared capabilities.

**Does not:** prescribe exact file paths, forbid justified new owners, or
require a new concept to fit an abstraction that cannot express it correctly.

Target architecture. Use judgment.

## Preferred implementation order

When adding or changing a capability, use this order as the default:

1. **Delete what is in the way.** Remove obsolete, duplicated, or accidental
   complexity when it is no longer needed. Preserve required behavior and
   verify callers and ownership before deleting code. Do not keep a parallel
   offline/local gameplay owner when the client/server owner exists; see
   [`../../specs/networking/networking.md`](../../specs/networking/networking.md)
   (“Remove offline/local gameplay paths”).
2. **Strengthen an existing owner.** Search for the current file, function,
   subsystem, or data model that already owns the behavior. Prefer making that
   owner more capable and reusable over creating parallel files or functions.
3. **Add a new owner only when needed.** Create a new file, function, type, or
   subsystem when the current owners cannot express the capability clearly and
   safely. State why an existing owner is insufficient and give the new owner
   one responsibility.

This is a preference order, not a ban on new code or a requirement to preserve
a poor abstraction. If extending an existing owner would create tangled or
unsafe responsibilities, define a clear new owner instead.

### Do not force a genuinely new domain into the current model

Before reusing an abstraction, check that its assumptions fit the new behavior.
For example, a future four-dimensional or node-based environment that does not
use XYZ coordinates or a fixed tick rate must not be made to pretend that it
does just to reuse today's movement, transform, or timing code. First remove
unneeded constraints, then strengthen truly general capabilities, and add a
new domain owner where the existing model has no honest representation. Share
lower-level pieces only when their contracts still make sense in both domains.

## Small Files

Prefer small files.

Ideal:

* under 100 lines
* one responsibility
* one obvious owner

Examples:

gravity.cpp
doGravity()

movement.cpp
doMovement()

send-packet.cpp
sendPacket()

spawn-blood.cpp
spawnBlood()

Avoid giant files that own many unrelated systems.

---

## Ownership

Every feature should have a clear owner.

Good:

src/replay/
src/duel/
src/npc/
src/weapon/
src/network/
src/ui/

Bad:

Feature logic spread across many unrelated files.

Searching for:

npc
weapon
replay
duel

should immediately reveal the owning subsystem.

---

## Main Files

Main files should orchestrate.

They should not own feature logic.

Example:

main.cpp should contain:

* initialization
* registration
* update loop
* shutdown

Not:

* replay implementation
* weapon implementation
* npc implementation
* duel implementation

Move ownership into subsystems.

---

## Public APIs

Subsystems should expose small APIs.

Example:

Replay:
saveReplay()
loadReplay()

Gravity:
doGravity()

Movement:
doMovement()

Weapon:
fireWeapon()

Keep interfaces small and obvious.

## General behavior ownership

Shared behavior must be owned and named for the behavior itself, not for the
first mode that used it. A server gamemode runtime may support duel, FFA, TDM,
Bomb Tag, sandbox, and future JSON-defined modes, so its state and functions
must use general gamemode, match, participant, actor, spawn, score, map, and
results terminology. A `duel-only` or `bomb-tag-only` function is incorrect when
it handles shared lifecycle behavior.

Individual gamemodes provide data and narrowly scoped rules. They do not own the
shared server tick, participant registry, player/NPC lifecycle, map rotation,
weapon-set application, countdown, results, or generic GUI presentation.

When a generic function is needed, first generalize the existing owner and
rename its API and callers. Do not add a second mode-specific implementation
that duplicates the same lifecycle behavior.

## Engine-first gamemode boundary

MiMITA should behave like an extensible engine with a game defined inside it.
Shared actor lifecycle, map loading, spawn reset, loadouts, combat events,
scoring, membership, networking, countdowns, results, and GUI bindings belong
to general engine services. JSON-defined modes select rules and data through
stable IDs. New modes must not require functions named for a single existing
mode, because that makes the runtime scale with hardcoded cases instead of
with data.

---

# Code Architecture

## Function Reuse First

Before writing a new function:

Search the entire repo.

Assume the function already exists until proven otherwise.

## Prove Necessity

Every new function must answer:

Why can't an existing function be reused?

## Delete Duplicates

If two functions perform substantially the same job:

Prefer merging them.

Prefer deleting one.

## One Source of Truth

Common behaviors should have exactly one implementation.

Examples:

weapon grip position

hand attachment

socket lookup

collision queries

world-space labels

NPC targeting

spawn selection

map loading

## Reuse Over Creation

Preferred order:

1. Reuse existing function
2. Expand existing function
3. Generalize existing function
4. Create new function only if required

## Minimize Code Growth

Adding code is not automatically progress.

If a feature can be implemented with:

+0 functions

instead of

+3 functions

prefer +0.

## Expand Before Creating

Before creating a new function with its own arguments, logic, and responsibilities:

Prefer expanding an existing function's parameters instead.

Examples:

* Adding an optional bool parameter to an existing function.
* Adding a defaulted enum argument to control behavior.
* Extending an existing signature rather than writing a parallel function.

A function with 8 parameters that already exists is better than 3 new functions with 3 parameters each.

Existing functions already have:

* Call sites.
* Test coverage.
* Known behavior.
* Integration with surrounding systems.

New functions need all of the above from scratch.

Prefer:

```cpp
// existing, expanded
void doJump(Player& p, bool jumpHeld, float dt, bool allowWorldContactJump = false);
```

Over:

```cpp
// new, separate
void doWorldContactJump(Player& p, float dt);
void doWallJump(Player& p, float dt);
void doCeilingJump(Player& p, float dt);
```

This does not mean functions should never be split.

It means:

Expanding an existing function should be the default choice.

Creating a new function requires justification.

==================================================

---

## Generalization and composition

Prefer a shared capability with data or policy inputs over copied one-off
implementations when the behavior and invariants are genuinely shared. For
example, use a reusable gamemode flow parameterized by a mode definition when
duel behavior is one case of that flow. Keep functions atomic in responsibility
and compose them so new behavior can emerge without hardcoded locks between
systems, including animation and gameplay actions. Do not generalize for its
own sake: each abstraction must have a clear owner, contract, and real reuse
case. See [`../vision-mentality/vision.md`](../vision-mentality/vision.md) for
the wider project direction.

## Spec-first and authority enforcement

For gameplay, networking, collision, damage, or lifecycle behavior:

1. Write or update the specification first.
2. Update the authoritative function map in
   [`../../specs/networking/contact-authority.md`](../../specs/networking/contact-authority.md)
   or the relevant owning specification.
3. Implement code to match the specification.
4. Prove it through the runtime scenario matrix.
5. Record the result in the changelog and regression history.

The specification is the desired behavior. Code is wrong when it disagrees
until a human changes the specification.

There is one authoritative owner per behavior. Do not add a parallel local,
online, client, server, host, replay, hitscan, projectile, or melee damage path.
Weapon-specific behavior belongs in configuration and small geometry query
adapters, not in weapon-ID branches inside shared resolution code.

Combat damage must use the shared damage sink. Direct health, death, or
authoritative knockback mutation outside that sink is forbidden except in
spawn/reset/lifecycle code, tests, and explicitly documented migration code.
The repository enforces this with the scripts described in
[`../../specs/dev-loop/quality-gates.md`](../../specs/dev-loop/quality-gates.md).
