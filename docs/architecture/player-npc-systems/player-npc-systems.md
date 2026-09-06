// 09 06 2026, 00 00
/* purpose
* define the shared player and NPC gameplay architecture
* ensure humans, NPCs, AI agents, and TAS bots use the same gameplay paths
* keep actor differences in input and decision generation
* this file DOES NOT define separate player-only or NPC-only gameplay systems
* this file DOES NOT replace movement, weapons, networking, or GUI specifications
* this file DOES NOT require every actor to make the same decisions
*/

# Player and NPC Systems

## Core rule

A player is an actor and an NPC is an actor. Human input, AI input, scripted
input, and TAS input may be produced differently, but they must be consumed by
the same gameplay systems.

Players and NPCs must use the same exact paths for movement, aiming, firing,
jumping, dashing, down-dashing, freezing, collision, damage, knockback,
animation, inventory, interaction, death, respawn, and network replication.

There must not be a player implementation and a parallel NPC implementation
that gradually develop different rules. If a behavior is valid for one actor,
the shared actor/action path should make it valid for the other.

## Input boundary

The allowed difference is the input source:

- a human produces input from keyboard, controller, touch, or another device;
- an NPC produces input from an AI decision;
- a scripted actor produces input from a script;
- a TAS bot produces input from recorded or generated commands.

All of these inputs should become the same shared action or input structure
before movement and gameplay simulation runs.

The gameplay system must not ask whether the actor is human or NPC to decide
how physics, weapons, damage, or movement work. It should consume actor state
and input, then produce the same kind of result.

## Ownership rules

Keep actor identity, decision generation, and gameplay execution separate:

1. input or AI code chooses an action;
2. the shared actor system validates and applies the action;
3. shared physics, collision, combat, animation, and replication process it;
4. presentation displays the resulting state.

If a player-only or NPC-only branch is proposed, document why it cannot use the
shared path. Prefer extending shared action or actor state over adding another
gameplay implementation.

## Terminal and test requirement

Every shared actor action should be reachable from terminal commands and a
deterministic test harness. This makes movement, combat, NPC behavior, replay,
and multiplayer state testable without requiring a visual test for every edit.

## Future platforms

The shared actor boundary must not depend on a desktop-only input device or
windowing API. Platform adapters should translate keyboard, controller, touch,
mobile, console, or other device input into the same shared actions.
