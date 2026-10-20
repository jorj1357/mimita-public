# Actor collision implementation-ready specification

- Status: PASS_WITH_HUMAN_REVIEW
- Time UTC: 2026-10-20T21:49:31Z
- Display time: 2026-10-20 17:49:31 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD at start: `a3901adb9ce179fddf599d4ba6bd7c84af7800e1`
- Work type: behavior specification clarification; no gameplay code change

## Pre-existing and concurrent work

The working tree already had `docs/jorj-docs/20261020plan.md` modified. While
this turn was in progress, `docs/specs/gamemodes/juggernaut.md` also appeared
modified. Neither file was changed by this task; both edits were preserved.

## User decisions recorded

- Players, NPCs, and monsters are all actors and must use the same capsule
  collision path for player-player, player-NPC/monster, and NPC/monster pairs.
- The dedicated server owns the final result; clients reconcile to it.
- Use nearby candidate checks with a simple spatial grid, not all actor pairs.
- Any actor may stand on another; moving support carries the supported actor.
- Dead actors remain collidable with a ground-level, lengthwise capsule.
- Actors push one another in the direction implied by the impact angle. Make
  response values configurable and live-tunable.
- Suggested initial response: around 0.1 m total movement for a weak hit;
  around 5 m/s maximum knockback speed at a direct impact near/above 100 m/s.
  These are tuning starts, not locked balance requirements.
- A hard impact from above pushes downward; a gentle landing becomes support.
- Per-actor capsule dimensions belong in JSON.
- Per-limb collision is later work.

## Changed file

### `docs/specs/npc-behavior/npc-behavior.md:203-316`

Before, the collision requirement was:

> For the first collision slice, every live actor pair uses the same root
> capsule-vs-capsule solver: NPC-vs-NPC, player-vs-NPC, and player-vs-player.
> Add a flat, standable-top support rule so actors can stand on one another; the
> main collision shape remains a capsule. This is the first step, not the final
> body-accurate shape. It does not replace the existing actor-vs-world collision
> rules.

After, the section now defines one actor-neutral path, server authority and
client reconciliation, nearby spatial-grid candidates, 70-actor performance
evidence, corpse capsules, per-actor JSON profile ownership and hot reload,
impact-angle and speed response, support versus hard downward hits, and
first-slice acceptance.

Exact new requirements include:

> **Desired behavior:** players, NPCs, and monsters are all actors. There is no
> special “local player” collision rule. Every actor uses the same actor-pair
> collision path and can collide with every other actor:

> The dedicated server owns the final collision result for every actor pair.
> Resolve collisions in the authoritative fixed 60 Hz simulation and send the
> resulting actor state so clients converge to it.

> **Performance for the first version:** do not compare every actor with every
> other actor. Use a simple spatial grid or reuse an existing suitable spatial
> broadphase to find nearby capsule pairs.

> **Dead actors:** death does not remove actor collision. The ragdoll keeps one
> capsule, moved to the actor's ground-level origin and laid lengthwise along
> the ground to represent the body.

> **Collision data and live tuning:** use `config/actor-collision.json` as the
> source of truth for actor-pair capsule sizes and push-response tuning.

> Initial tuning targets, not locked balance values: a weak hit moves the
> target about 0.1 m total; a direct impact around 100 m/s or above can reach a
> maximum knockback speed around 5 m/s.

> A gentle landing on another actor registers support, and the actor on top
> remains standing and is carried if the support moves. A hard downward impact
> pushes the lower actor down instead of treating the impact as a gentle landing.

The source audit paragraph was clarified to label the client-only behavior as
observed implementation, not the desired rule. Existing source evidence remains
that the client simulation resolves its controlled player against NPCs, while
there is no server NPC-pair pass and the server player collision resolver is a
no-op. The new requirements intentionally describe work still to implement.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/collision/collision.md`,
  `docs/architecture/collision/actor-vs-actor-collisions.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/specs/actor-presets/actor-presets.md`,
  `docs/specs/networking/networking.md`,
  `docs/specs/performance/performance.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/documentation-checker-v1.md`.
- `docs/doc-review-09-03-2026.md` was absent, though listed by the router.
- `config/actor-collision.json` does not exist yet; the spec identifies it as
  the proposed implementation owner and does not claim it is loaded today.
- `git diff --check -- docs/specs/npc-behavior/npc-behavior.md` passed.
- No code, tests, build, dev-loop, or runtime scenario changed or ran.
- No multiplayer, 70-actor performance, or 240 FPS result is claimed.

## Open documentation item

`docs/architecture/collision/actor-vs-actor-collisions.md:1` says:

> 2026 10 08 1600 jorj todo explain how  actors vs actor collisions work

Router rule: report and suggest an expansion, do not edit that document here.
Suggested expansion: clearly label root capsules as the first implementation
phase (with all pairings, authority, broadphase, support, and tuning), and
full-body triangle collision as a later target. This distinction also prevents
its draft end goal from being mistaken for the simple first slice requested
here.