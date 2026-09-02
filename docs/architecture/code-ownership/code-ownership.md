# Architecture Direction

Target architecture. Use judgment.

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

