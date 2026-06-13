Favor deleting code over adding code when both solutions achieve the same result.

# Mimita Engine

This is a C++17 OpenGL game engine.

## Repository Workflow

When solving coding problems:

1. Search the repository first.
2. Identify only relevant files.
3. Read only relevant sections.
4. Reason about the issue.
5. Implement the smallest correct fix.

Never answer from assumptions before searching.

Use repository search before reasoning.

Rules:

* Read repository files directly.
* Answer coding questions concretely.
* Prefer minimal patches.
* Avoid unnecessary rewrites.
* Do not ask for clarification unless required to proceed.
* Fix problems directly.
* Keep solutions practical and shippable.

Do not output chain-of-thought.
Do not narrate internal reasoning.

---

# Core Philosophy

Optimize for:

1. Performance
2. Simplicity
3. Readability
4. Debuggability
5. Extensibility

Performance is the highest priority.

Target hardware includes:

* Low-end PCs
* Integrated graphics
* Weak laptops
* Old hardware
* Small phones
* Low-memory devices

Avoid unnecessary allocations, copies, complexity, and hidden work.

If a simpler implementation achieves the same result, prefer the simpler implementation.

---

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

# Terminal Commands

Every gameplay action should eventually be callable through terminal commands.

The game should be playable and testable entirely through terminal commands.

Examples:

shoot
reload
spawnnpc
savedreplay
hostserver
joinserver

UI, hotkeys, AI, networking, and gameplay should reuse shared actions where practical.

Avoid duplicate implementations.

---

## Command Registration

Move command registration out of main.cpp.

Preferred:

registerReplayCommands();
registerWeaponCommands();
registerNpcCommands();
registerDuelCommands();
registerDebugCommands();

Feature systems own their own commands.

Example:

src/replay/replay-commands.cpp

void registerReplayCommands();

---

## Command Metadata

Commands should store metadata:

* name
* description
* usage
* category
* dateAdded

Support:

help
help2
help_recent

Commands should be grouped by feature.

Examples:

Replay
Weapon
NPC
Duel
Network
Debug
UI

---

# Debugging

Debug everything.

Visibility is preferred over guessing.

Every system should be able to explain:

* what it is doing
* why it is doing it
* what inputs it received
* what outputs it produced

Use centralized logging.

Prefer:

Debug::log(category, ...)

Avoid scattered printf() usage.

---

## Debug Categories

Examples:

UI
Render
Combat
NpcCombat
Physics
Collision
Replay
Networking
Audio
Animation
Geometry
World
Duel
Ragdoll

Categories should be independently enabled and disabled.

Example:

ui_debug 1

npc_damage_debug 1

ragdoll_debug 1

---

## Rate Limiting

Avoid log spam.

Support:

* rate-limited logging
* aggregated logging
* summary logging

Prefer:

one useful summary per second

instead of

sixty identical logs per second

---

# Accessibility

Design for the widest possible audience.

Consider:

* keyboard-only use
* controller use
* limited mobility
* visual impairments
* low-end hardware
* low frame rates

Accessibility should be considered during implementation, not added later.

---

# Player and NPC Philosophy

Players and NPCs should share systems whenever practical.

Preferred architecture:

# Player

Entity
+
Human Input

# NPC

Entity
+
AI Input

Shared:

* movement
* physics
* weapons
* animation
* inventory
* damage
* death

The primary difference should be who generates input.

Avoid maintaining separate gameplay systems when a shared system is possible.

---

# Performance Rules

Prefer:

* predictable execution
* cache-friendly structures
* simple data flow
* low allocation counts
* low memory usage

Measure before optimizing.

But when two implementations are equal:

prefer the simpler and faster one.

Avoid architecture that exists only for abstraction.

Abstractions should earn their cost.

---

# General Rule

If a feature becomes difficult to find, debug, test, or optimize:

the ownership is probably wrong.

Move it to the subsystem that logically owns it.
