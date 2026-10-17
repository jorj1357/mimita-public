# MiMITA Roadmap — Proposed Review Copy

**Status:** Proposal for discussion. This file does not replace `ROADMAP.md` or
`VISION.md`.

## Purpose and audience

This proposal gives players, contributors, and AI agents a readable answer to
four questions: what MiMITA is now, what is being worked on, what the next
release is meant to contain, and what would count as finishing it. It turns the
current brainstorm into a dependency-aware release plan while keeping
uncertain ideas visibly undecided.

## Does not

- Change the product vision in [`VISION.md`](VISION.md).
- Approve or promise a release date or final release scope.
- Replace behavior specifications, architecture documents, or the open issue
  index with duplicated implementation detail.
- Treat a proposed item as implemented or verified.

## How to read this roadmap

- **Current** means the work is underway or the present release state is known.
- **Proposed next** means the recommended next release target, pending review.
- **Candidate** means a desired future direction without an approved release.
- **Needs a decision** means the roadmap must not pretend the design is settled.
- Each release has a **done when** section. A feature is not done because it
  builds; its required runtime evidence and human review must also be recorded.
- Release numbers group outcomes, not calendar promises. If a required system
  is not ready, move the release rather than silently dropping its acceptance
  criteria.

## Where MiMITA is now

GitHub lists v2.0.7 as a pre-release and marks v2.0.6 as the latest release.
The v2.0.7 notes themselves say it is not the full release and call out Zombie
Tower and further polish as remaining work. See the
[`GitHub release list`](https://github.com/jorj1357/mimita-public/releases).

The practical next goal is to finish a complete, replayable Zombie Tower
experience. The current detailed target is
[`docs/specs/gamemodes/zombie-tower.md`](docs/specs/gamemodes/zombie-tower.md),
which is still labeled a draft whose authoritative target behavior is pending
human edits. The roadmap therefore proposes a first step of reviewing and
locking that scope before calling the release fully specified.

Related reported blockers and their evidence/readiness are tracked in
[`docs/specs/open-gameplay-issues-2026-10-16.md`](docs/specs/open-gameplay-issues-2026-10-16.md).
Do not copy every open issue into release scope: include an issue when it blocks
joining, playing, or completing the release's core mode, and retain the issue
index as the detailed owner.

## Proposed next release: v2.1.0 — Zombie Tower

**Goal:** deliver one complete Zombie Tower experience that players can join,
understand, play through, and want to replay. This is the release's center of
gravity; other modes and platform ideas should not expand this scope by default.

### Proposed scope

1. **A complete Tower run.** The map, lobby/start flow, tower progression,
   monsters, combat, pickups, checkpoints or recovery rules, boss content,
   audio, visual identity, and run ending work together as one playable loop.
   The exact required floors, monster roster, run length, and completion rules
   must be agreed in the Zombie Tower spec before the release is locked.
2. **Tower-ready monsters.** The required monster types load the intended
   models/textures and use the movement, attacks, and animation needed by the
   Tower. Improve the shared NPC movement and authoring path only as required
   by those monsters. Relevant owners include
   [`npc-behavior`](docs/specs/npc-behavior/npc-behavior.md),
   [`NPC movement architecture`](docs/architecture/player-npc-systems/npc-movement.md),
   and [`animations`](docs/specs/animations/animations.md).
3. **Usable avatar and monster authoring.** Fix the Avatar Editor problems
   that prevent producing, loading, or previewing the required Tower assets.
   Do not make unrelated editor expansion a v2.1.0 gate. The current behavior
   owner is [`avatar-editor.md`](docs/specs/avatar/avatar-editor.md).
4. **Reliable play-session basics.** The selected map and server agree; player
   and NPC spawns are inside the playable world; death/respawn and the Tower's
   required attacks work without manual recovery steps; leaving and rejoining
   does not strand the player in a broken session. Use the open issue index
   and its mapped networking/gamemode specs for detailed acceptance.
5. **Required persistence.** Decide exactly which account or run data the
   Tower needs to save, then prove that data survives the intended restart or
   reconnect. Reuse and verify the existing
   [`data-saving specification`](docs/specs/data-saving/data-saving.md) and
   [`persistence recovery guidance`](docs/operations/todo-sort-these/persistence-recovery.md)
   before proposing another database or storage subsystem.
6. **Performance and readability for the actual mode.** Measure a representative
   Tower encounter with its intended number of players, NPCs, effects, and
   ragdolls. Fix release-blocking frame-time or readability problems. Record
   the tested configuration and evidence; do not turn an aspirational FPS
   number into a pass condition without agreeing on hardware and workload.

### Recommended work order inside v2.1.0

This is a dependency order, not a promise that every task must be strictly
serial. Work may overlap when owners and acceptance criteria are clear.

1. **Lock the finish line.** Review the Zombie Tower draft with its detailed
   behavior owners. Name the minimum run, content, persistence fields, player
   count, and completion/failure states. Resolve which open issues truly block
   that run.
2. **Prove the play foundation.** Reproduce and resolve any blocking
   map-selection, in-world spawn, equip/fire/respawn, and leave/rejoin failures
   using the real dev loop and its live event journal. Keep these fixes at
   their existing owners; do not use a broad rewrite as a substitute for a
   traced cause.
3. **Complete only the required shared primitives.** Make the model/texture
   import, avatar normalization, animation, NPC movement/navigation, attack,
   collision, and effect paths that the selected Tower content needs work
   end-to-end. Reuse shared systems and define a small verified vertical slice
   before adding more monster types.
4. **Build the Tower loop on those primitives.** Integrate the map, monsters,
   pickups, checkpoints, boss, progression, and presentation; test a full run
   from join through its ending.
5. **Prove persistence and multiplayer behavior.** Verify the agreed saved
   fields across restart/rejoin and exercise the target player count. Record
   server/client state and the first divergence for network failures.
6. **Release review.** Resolve release-blocking issues, inspect performance and
   readability in representative play, gather human acceptance, and publish
   only the features whose done criteria passed.

### v2.1.0 is done when

- A new player can enter the intended Tower flow without needing a developer
  command or unexplained setup.
- The agreed full run can be completed, including its required monster
  encounters, pickups/checkpoints, boss or final challenge, and a clear ending.
- Required assets and animations load correctly for initial spawn and any
  respawn or reconnect path included in the release.
- The agreed player data persists across the specified lifecycle, and a failed
  save is visible and recoverable according to the data-saving spec.
- Map identity, spawn position, combat acceptance, death/respawn, and leave/
  rejoin behavior pass the release scenarios in a fresh runtime session.
- Performance, effects, and actor readability meet thresholds chosen for the
  documented test setup.
- The Zombie Tower specification's status and implementation evidence are
  updated, and a human has accepted the mode's feel and presentation.

**Not automatically included in v2.1.0:** Counter-Strike completion, Infinite
Dungeon Slayer, Payload, Bomb Tag, Juggernaut expansion, Duels changes,
vehicles, portals/gravity gun, full destructible worlds, general-purpose
creator tooling, or platform monetization. Add one only if it is explicitly
required to finish the agreed Tower run.

## Candidate release after v2.1.0: v2.2.0 — Infinite Dungeon Slayer

**Proposal, not a commitment.** Reuse Tower rooms and working monster,
animation, pickup, save, and encounter primitives to create a replayable
upward/stacked dungeon. Add room selection/connection, an ongoing difficulty
curve, and reliable distribution of health/ammo rewards. Prove that a long run
stays readable and performant before calling the mode complete.

**Done when:** a player can start a run, progress through varied connected
rooms, receive understandable escalating challenges and rewards, and finish or
fail cleanly; a second run can vary its route or content; save/recovery rules
are explicit and tested.

## Candidate release after that: v2.3.0 — Payload

**Proposal, not a commitment.** Build a readable team escort mode using the
proven NPC and actor systems. Decide separately whether TF2-inspired actor
profiles, team composition, and map authoring are needed; do not assume that
“Payload” settles those design choices.

**Done when:** both teams have clear goals and feedback, the payload advances
and can be contested predictably, matches end and reset correctly, and a
representative NPC/player load remains readable and performant.

## Longer-range candidate backlog — no release assigned

These are directions from the current brainstorm, not promises or a required
sequence. Give each a release only after identifying its user value, shared
primitives, owner, risks, and a testable first slice.

| Direction | Why it matters | Decision or first slice still needed |
|---|---|---|
| Reusable game primitives | Makes NPCs, movement, animation, pickups, and modes easier to compose and reuse. | Name the first consumer and avoid building generality without a working mode. |
| Vehicles, skateboard, BMX | Adds movement and game possibilities. | Choose one controllable vehicle and define physics, collision, replication, and camera acceptance. |
| Destructible world and heavy melee | Lets players alter routes and break objects with weapons/tools. | Prove one small authoritative object break and restoration path before whole-world destruction. |
| Broader animation and creator tools | Lets players and creators make distinct actors, attacks, and worlds. | Build on the existing animation/Avatar Editor owners; choose one complete authoring-to-runtime path. |
| Modding, maps, scripts, and sharing | Lets other people make and exchange experiences. | Define safe APIs, permissions, versioning, asset transfer, and compatibility. |
| Built-in distribution and social features | Moves discovery and updates closer to the game and supports healthier community goals. | Decide moderation, privacy, update trust, offline behavior, and what “healthy” success means. |
| Distributed storage and security | Could reduce dependence on one service. | Threat model, recovery/availability goals, abuse controls, and a small non-critical prototype first. |
| In-universe currency/economy | Could support useful creator/player exchanges. | Define a real use, governance, abuse prevention, accounting, and legal/security review before implementation. No speculative coin or financial promise is approved by this roadmap. |

## Decisions to settle before treating this proposal as the plan

1. Does v2.1.0 mean “Zombie Tower is complete,” or must it also include any
   named side modes? This proposal recommends the former.
2. What is the minimum complete Tower run: required floors/rooms, monsters,
   boss, player count, run length, win/failure, checkpoints, and replayability?
3. Which persistent fields belong to the Tower, and what must survive a game
   server restart versus only a player reconnect?
4. Which networking fixes are release gates for the Tower, and what are the
   target host/client and player-count scenarios?
5. What does “Avatar Editor fixed” mean for the Tower assets, and what
   collision accuracy is required for avatars/ragdolls? The roadmap should
   link to a decided collision spec rather than promise exact triangle
   collision without an agreed performance target.
6. Are v2.2.0 and v2.3.0 useful placeholders for Dungeon and Payload, or should
   those releases remain unnamed until v2.1.0 scope is approved?

## Keeping the roadmap honest

When work starts, add an owner spec, a small acceptance checklist, and the
evidence needed to mark it done. Update status only from observed evidence:
specification, code, build, automated/runtime scenario, and human acceptance
are separate states. When scope moves, record what moved and why so readers can
understand the tradeoff. Keep `VISION.md` broad and aspirational; keep this file
focused on the next outcomes and the reason for their order.
