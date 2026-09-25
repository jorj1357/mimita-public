# Actor, player, NPC, and network chain audit

This is an initial source audit of the current working tree. It is not a
deletion approval and it is not runtime proof.

## Simple picture

The repository currently has two worlds living beside each other:

```text
older concrete world:
Player / Npc / ServerPlayer / ServerNpc
        -> many direct gameplay and network paths

newer generic world:
EntityId / components / actor state / hot policies
        -> hot modules and generic capabilities
```

The newer world is trying to become the real owner, but the older world still
does important work. That is the main reason the same behavior can feel like it
has many owners.

## Finding 1 — actor identity and state are duplicated

- Severity: high
- Type: architecture overlap
- Evidence: `src/entities/player.h` has `Player`; `src/npc/npc.h` has `Npc` and
  `NpcSystem`; `src/network/server.h` has `ServerPlayer` and `ServerNpc`;
  `src/ecs/components.h` and `src/ecs/entity-registry.*` add generic entities;
  `src/network/actor-state.h` and `src/hot-reload/game-api.h` add generic actor
  state and policy envelopes.
- Existing specification: the ECS migration explicitly calls the old and new
  state “temporary duplicated state.”
- Recommendation: keep the old structures temporarily as adapters, but make
  `EntityId` plus canonical components the destination. Do not add new
  gameplay state to both worlds.
- Confidence: high for the duplication; medium for which old fields are still
  live owners.

## Finding 2 — server files are doing too many jobs

- Severity: high
- Evidence: `src/network/server.h` is about 1,437 lines and contains both
  server infrastructure types and concrete player/NPC state. `server-players.cpp`
  is about 1,776 lines; `server-npcs.cpp` is about 1,657 lines.
- Recommendation: do not split files just to make them smaller. First mark
  each function as mechanism, policy, state ownership, serialization, or
  compatibility adapter. Move policy to hot systems first; keep transport,
  storage, and tick mechanics cold.
- Confidence: high.

## Finding 3 — NPC behavior has many decision pieces, but not necessarily one
## bad owner per piece

- Evidence: `src/npc/` contains mind, goal, navigation, traversal, state
  machine, combat, avatar, and difficulty code. Hot modules also contain NPC
  intent, target selection, weapon selection, combat decision, state selection,
  navigation, and lifecycle policies.
- Recommendation: separate the NPC into a simple pipeline:

  `actor components -> sense -> choose intent -> shared movement/combat -> effects`

  The NPC should choose intent. Shared gameplay systems should perform movement,
  collision, weapons, damage, and death. Delete or redirect duplicate behavior
  only after tracing the actual call path.
- Confidence: medium. The file structure proves overlap, but runtime ownership
  still needs tracing.

## Finding 4 — the hot ABI is becoming a second model of the game

- Evidence: `src/hot-reload/game-api.h` is about 3,947 lines and defines many
  actor, NPC, lifecycle, navigation, combat, death, and respawn structures.
  Several hot module files locally redeclare actor state shapes instead of
  visibly including one shared definition.
- Recommendation: keep the ABI stable and generic, but audit every repeated
  POD shape. One shared ABI definition should be read by all modules where
  possible. Do not create a new ABI type for every feature if a generic event,
  component, command, or policy can express it.
- Confidence: high for repeated definitions; medium for whether every repeat
  is an unsafe duplicate rather than an intentional ABI copy.

## Finding 5 — networking is both mechanism and gameplay authority

- Evidence: the networking specification wants client prediction, server final
  authority, shared player/NPC behavior, and hot policy. The code has network
  transport, server player/NPC structs, movement validation, reconciliation,
  snapshots, packets, actor state, and hot network policy modules.
- Recommendation: preserve cold networking infrastructure. Move decisions and
  rules hot in this order: actor lifecycle policy, movement policy, combat/damage
  policy, snapshot selection policy, then reconciliation policy. Do not move
  sockets, packet decoding safety, entity storage, or session lifetime merely
  to make the executable smaller.
- Confidence: high for the boundary direction; medium for the current active
  owner of each behavior.

## What to keep

- `docs/`, especially current specs and architecture documents.
- `src/ecs/` and the generic entity/component foundation.
- `src/hot-reload/` and its self-tests, while verifying which modules are truly
  active.
- Network transport and session infrastructure.
- Existing concrete Player/NPC code until each path has a replacement and a
  test.

## What to consolidate first

1. Actor identity: choose one canonical `EntityId` path.
2. Actor lifecycle: one spawn/reset/death/respawn owner.
3. Actor state shapes: remove repeated private copies where safe.
4. NPC decisions: one intent pipeline instead of parallel state decisions.
5. Server player/NPC mirrors: adapters only, not independent gameplay owners.

## What not to delete yet

- `Player`, `Npc`, `ServerPlayer`, or `ServerNpc` as a whole.
- Networking packet types.
- Hot ABI structures.
- Self-tests.
- Collision or movement code merely because a generic replacement exists.

The safe move is to prove the new path, redirect one caller group, test it,
and only then remove the old path.

