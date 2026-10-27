# Long-range MiMITA roadmap intake

Time: 2026-10-27T06:35:52Z (2026-10-27 02:35:52 EDT)
Branch: `20261011zt`
HEAD observed during final validation: `a1f1334b`

## Result

Appended `Longer-range game and project ideas — 2026-10-27` at
`docs/specs/open-gameplay-issues.md:1333`. The 12 items record animation-library production; Payload expansion; the Zombie Tower's desired 100-floor scale and prerequisites; a two-car vehicle mode; melee collision and restrained hit feedback; false reconnect messaging; replay editor/export improvements; in-game music, generated voice, and video creation; social/cozy/trippy/minigame ideas; ambitious space, infinite-world, and RPG prototypes; storage/VPS/decentralization; durable architecture and operating-system exploration; intentional jank/community art direction; and transparent governance, monetization, and creator livelihood.

These are explicitly labeled roadmap ideas, not approved contracts or verified behavior. Zombie Tower's order is captured as reusable monster animations and behavior, then checkpoint/spawn authoring, then integrating those into the tower and expanding it. 100 floors is recorded as the player's preference, with 50 still an open scope alternative. Payload is recorded as working according to the player's report, with expansion remaining.

## Exact change

Before, the issue file ended with the existing 2026-10-27 melee cadence entry:

```md
  architecture/weapon documentation with an explicit owner and invariant so
  future weapon fields cannot reintroduce hidden melee cadence limits.
```

After that entry, appended the following exact text:

```md
### Longer-range game and project ideas — 2026-10-27

The following are roadmap ideas and priorities from the player, not approved
feature contracts or claims that existing behavior has been verified. Keep
them in the issue tracker until each effort has an owner spec, a small first
milestone, and acceptance scenarios.

- **Build a reusable animation library for monsters and actors:** The player
  would like to spend a dedicated stretch of time creating a broad set of
  monster animations, then make those clips reusable and composable through
  `animations.json`: idle, walk, run, crouch, jump, fall, hurt, death, attacks,
  and matching sounds. NPCs should use the same authored sets and have stronger
  behavior so monsters feel like distinct characters. This builds on the
  shared animation and Blender authoring work above; define a batch-friendly
  authoring/review workflow and reusable clip/state conventions before a large
  content push. [`animations/animations.md`](animations/animations.md),
  [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md),
  [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md)
- **Expand Payload's map and experience:** The player says Payload works now
  and wants to keep expanding it, especially toward a large, polished map.
  Define the map scale, route, objectives, and authoring milestones from a
  playable baseline. [`gamemodes/payload.md`](gamemodes/payload.md)
- **Grow Zombie Tower into a large authored mode:** The player prefers a
  100-floor tower, while 50 floors remains a possible smaller scope. Build the
  tower after establishing monster animation and behavior foundations. Add
  flying, floating, running, and swarming monsters; stronger NPC collision;
  monster attack animations and sounds; and checkpoint-aware checkpoint
  placement and monster spawn volumes, designed for players who move very
  quickly. Put the resulting authored checkpoints and spawn volumes into the
  tower itself. Work out floor variety, progression, authoring effort, and
  checkpoint/spawn rules before locking the 50-versus-100-floor scope.
  [`gamemodes/zombie-tower/zombie-tower.md`](gamemodes/zombie-tower/zombie-tower.md),
  [`animations/animations.md`](animations/animations.md),
  [`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md)
- **Design a vehicle-focused gamemode:** The player imagines two complete cars
  with teams of roughly four to six players per car. Players should shoot
  rockets and guns between vehicles and be able to damage or destroy the other
  car. The vehicle rules, seating/respawn, weapon use, damage model, and match
  objective are open; make a small playable mode contract before expanding
  vehicle systems. [`vehicles/vehicles.md`](vehicles/vehicles.md),
  [`gamemodes/vehicles/police-chase.md`](gamemodes/vehicles/police-chase.md)
- **Improve melee weapon collision and impact readability:** The player says
  the new melee weapons pass through walls and register hits inconsistently,
  and wants stronger contact visuals such as damage numbers or world-hit
  effects without overwhelming the screen. Treat wall collision, player hit
  consistency, and restrained impact feedback as separate acceptance cases;
  trace each weapon through shared collision and presentation owners.
  [`weapons/melee/melee-weapons.md`](weapons/melee/melee-weapons.md),
  [`weapons/weapons.md`](weapons/weapons.md),
  [`visuals/visuals.md`](visuals/visuals.md)
- **Resolve repeated reconnect messaging while still in a server:** The player
  reports that the game says it is reconnecting even while they remain in the
  server. Reproduce with client/server identity and connection-state evidence;
  determine whether the message is stale, incorrectly triggered, or reflects a
  real transport recovery before changing reconnect behavior.
  [`networking/networking.md`](networking/networking.md)
- **Improve replay export and the replay editor:** The player finds replay
  editing fun and wants export fixed and the editor made substantially better
  and smoother for creating edits. Define the current export failures and
  editor workflow separately, then prioritize a reliable end-to-end edit and
  export path. [`replay/replay.md`](replay/replay.md)
- **Make in-game story/music, generated voices, and video creation easier:**
  Explore in-game story or MIDI/music, live voice generation, plus scripts and
  smoother workflows for longer videos such as “meet the character” pieces,
  edits, and movies.
  Decide what the game should generate or record, what creators author, and
  which rights, controls, and technical limits apply before selecting tools.
  [`../architecture/live-development/hot-audio-contract.md`](../architecture/live-development/hot-audio-contract.md),
  [`replay/replay.md`](replay/replay.md)
- **Create social, cozy, trippy, and short-form modes:** Ideas include survive-
  the-disaster rounds, a voice-chat bar hangout, fractal exploration, and bar-
  sized games such as chess, checkers, Connect Four, boxing, and possibly
  wrestling. Also explore more cozy and trippy experiences. Select a few
  small prototypes and specify their social and input loops before building a
  broad minigame collection. [`gamemodes/survive-the-disasters.md`](gamemodes/survive-the-disasters.md),
  [`gamemodes/4d-world-fractals-etc.md`](gamemodes/4d-world-fractals-etc.md),
  [`gamemodes/gamemodes.md`](gamemodes/gamemodes.md)
- **Explore larger technical challenge modes:** The player is interested in a
  space/relativity-style experience, including firing a huge weapon at the
  Moon and seeing realistic fragmentation; infinite-world generation with a
  strong inventory and building loop, using Minecraft-like play as a quality
  benchmark; and a 2012-style RPG loop where defeating monsters earns gold to
  buy better items. Treat these as separate prototypes with bounded physics,
  world-generation, construction, combat, and progression goals.
  [`gamemodes/4d-world-fractals-etc.md`](gamemodes/4d-world-fractals-etc.md),
  [`procedural-infinite-world/procedural-infinite-world.md`](procedural-infinite-world/procedural-infinite-world.md),
  [`gamemodes/minecraft.md`](gamemodes/minecraft.md),
  [`gamemodes/rpg.md`](gamemodes/rpg.md)
- **Plan data storage, VPS cleanup, and decentralized progression:** The player
  wants to understand and reduce unnecessary VPS storage, estimate what
  persistent progression costs to save, and explore distributed or
  decentralized storage instead of relying on one central server. Measure
  current storage use and identify retained data first; then document cost,
  consistency, recovery, ownership, and trust requirements before selecting
  an architecture. RPG gold and purchased items are one motivating use case.
  [`data-saving/data-saving.md`](data-saving/data-saving.md),
  [`../operations/vps-deployment/vps-audit.md`](../operations/vps-deployment/vps-audit.md)
- **Keep MiMITA durable and reusable as a long-lived engine:** The player wants
  MiMITA to persist indefinitely, using hydrogen and helium persisting since
  the Big Bang as a metaphor, and to be “atomic” enough for future uses such as
  tap water, electricity, games, and entertainment. Record this as a long-term
  architecture value: favor durable, composable foundations and clear ownership
  so the project can keep serving new purposes. Define practical maintenance,
  compatibility, backup, and stewardship goals rather than treating literal
  indefinite operation as a guarantee. The player also wants to explore
  extending MiMITA into an operating system; scope and technical boundary are
  entirely open.
- **Preserve intentional jank and a community-made visual identity:** The
  player prefers MiMITA's rough, crunchy, small-window look to polished
  realism. Explore expressive but intentionally rough meshes, busy community-
  made icons designed as large, busy images that look crunchy when reduced to
  app-icon size, and retaining the character of crunchy text and the small
  window. Higher fidelity graphics may be explored where they support this
  identity; realistic presentation is not a settled direction.
  [`visuals/visuals.md`](visuals/visuals.md)
- **Make governance and creator sustainability transparent:** The player wants
  public, understandable moral and monetization principles; community voting
  wherever practical; and the ability for people who disagree with a decision
  to clone the project and make their own version. They also want a sustainable
  way to earn enough USD to support themselves and spend more time on MiMITA.
  Treat governance, contribution/voting scope, forkability, monetization, and
  creator livelihood as connected but distinct design questions to document
  openly with the community.
```

## Documentation and validation

- Read `docs/ROUTER.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/skills/documentation-checker-v1.md`, and the referenced animation, NPC, Payload, Zombie Tower, vehicle, melee, networking, replay, audio, gamemode, infinite-world, RPG, data-saving, VPS audit, and visual specifications.
- Documentation review: PASS for the new section's organization, issue-vs-contract labels, and new relative links. A link check found the first VPS audit candidate path was wrong; corrected it to `docs/operations/vps-deployment/vps-audit.md`, and all newly added relative links resolve.
- Specification review: PASS_WITH_HUMAN_REVIEW. This records the player's priorities without claiming implementations or silently fixing open choices. Major future ideas still need owner specs and acceptance scenarios before implementation.
- `git diff --check`: PASS. No code, runtime configuration, or assets changed; no build or gameplay test was applicable.
- The documentation check surfaced existing relevant TODO notes, left unchanged: `docs/architecture/code-ownership/code-ownership.md:3` (“todo maybe expand this? explain what exact kinds of files should go where...”) should become concise ownership categories and placement principles; `docs/architecture/code-ownership/code-ownership.md:259` (“todo explain generalization of functions...”) should be rewritten as a durable architecture principle for generic owners and composable behavior; `docs/architecture/vision-mentality/vision.md:1` (“todo explain hmmmmmm”) should state the project's concrete vision and implications; `docs/specs/gamemodes/zombie-tower/zombie-tower.md:1543` (“todo explain better enshittification ?”) needs its intended meaning clarified or removed; and `docs/specs/gamemodes/zombie-tower/zombie-tower.md:5681` (“todo - explain zombie tower”) should be replaced by a concise mode overview and goals. These are separate documentation follow-ups; no existing TODO was changed.
- Worktree at initial inspection had `docs/specs/open-gameplay-issues.md` modified by earlier work in this thread; that existing tracker content was retained. HEAD advanced during the task from `a1f1334b` observed at validation; no other source files were edited by this task.
