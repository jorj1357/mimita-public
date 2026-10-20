# RMB aim damping and actor-collision scope review

- Status: PASS_WITH_HUMAN_REVIEW
- Time UTC: 2026-10-20T21:28:14Z
- Display time: 2026-10-20 17:28:14 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD at start: `7556a218`
- Work type: specification update and source investigation; no gameplay code change

## Pre-existing work

The working tree already contained many unrelated changes. In particular,
`config/aimbody.json`, `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`,
and `docs/specs/npc-behavior/npc-behavior.md` were modified before this task.
Those edits were preserved. The current `aimbody.json` has
`right_arm_pointing.follow_damping` set to `10.0`; this task did not change it.

## Changed files and exact text

### `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:88-98`

Added a new subsection after the existing one-crosshair requirement. There was
no prior subsection at this location. Exact addition:

> ### RMB arm stability tuning (2026-10-20)
>
> The user reports that changing
> `config/aimbody.json` → `right_arm_pointing.follow_damping` from `1.0` to
> `10.0` seems to have fixed much of the RMB arm jitter: the arm jitters less.
> The working copy currently contains `10.0`. Keep this as a helpful tuning
> result, not the final solution. TODO: find a better long-term aiming solution
> so the physical arm and weapon settle reliably without depending on a high
> damping value. The improvement is user-observed; this note does not claim
> separate runtime verification.

### `docs/specs/npc-behavior/npc-behavior.md:195-223`

Replaced the prior ambiguous avatar-shape paragraph. Before:

> NPC physical collision must follow the selected avatar/role's authored body
> shape where that shape is supported. A different rendered avatar must not
> silently keep an unrelated generic body collision. Collision ownership,
> broadphase behavior, and safe fallback shapes remain governed by the collision
> and avatar specifications; this requirement does not imply that detailed
> per-limb collision is always appropriate.

After: the new section specifies one shared root capsule solver for all three
actor pairings, a separate flat-top support rule, per-limb shapes as later
work, a JSON ownership TODO, and first-slice server/support acceptance. The
exact addition is:

> ### Actor-to-actor collision scope
>
> For the first collision slice, every live actor pair uses the same root
> capsule-vs-capsule solver: NPC-vs-NPC, player-vs-NPC, and player-vs-player.
> Add a flat, standable-top support rule so actors can stand on one another; the
> main collision shape remains a capsule. This is the first step, not the final
> body-accurate shape. It does not replace the existing actor-vs-world collision
> rules.
>
> Later, actor body parts should have their own collision shapes that follow
> their animated or physical limbs. That work should follow the shared actor
> collision architecture after the root-capsule path works for all three pair
> types.
>
> TODO: decide the JSON owner and mapping for actor collision settings. Existing
> avatar manifests have capsule dimensions, and `config/collision.json` owns
> global collision settings. The actor-collision architecture draft proposes
> `config/actor-collision.json` plus actor-preset overrides, but neither is a
> confirmed active owner for actor-pair collision yet. Trace whether NPCs use
> avatar capsule dimensions before choosing. Add a new profile/file only if the
> current owners cannot express per-actor shape choices. Keep NPCs and players
> on the same resolved collision contract. Do not treat `npc-difficulty.json`
> as the owner for collision shape or solver rules.
>
> First-slice acceptance: each of the three pair types blocks and separates
> correctly, either actor can land on and stand on the other, and moving support
> does not leave the supported actor behind. The server produces the agreed
> result at the fixed tick and clients reconcile to it. Detailed per-limb
> collisions are not required in this first slice.

### `docs/specs/npc-behavior/npc-behavior.md:244-251`

Added a source-audit status note. There was no prior source-audit paragraph at
this location. It records that local simulation resolves local player versus
NPC capsules, the dedicated server's player collision function is a no-op,
and no server NPC actor-pair pass was found. It explicitly leaves multiplayer
acceptance open. Exact addition:

> Source audit (2026-10-20): the local simulation calls
> `resolveCapsuleVsCapsule` for the local player against each live NPC, including
> a special flat-top support check. It does not resolve NPC-vs-NPC pairs. The
> dedicated server's `resolvePlayerCollision` is currently a no-op, and the
> server NPC update does not add an actor-pair collision pass. Therefore this
> local path does not prove authoritative player-vs-NPC, player-vs-player, or
> NPC-vs-NPC collision in multiplayer. Runtime multiplayer acceptance remains
> open.

### New changelog file

There was no prior file at this path. This file records this session.

## Investigation findings

- `docs/specs/npc-behavior/npc-behavior.md` originally asked for capsule pairs
  and a flat standable top in its raw note. Its acceptance text elsewhere
  referred to authored avatar body shapes. This update clarifies the near-term
  capsule slice and leaves limb shapes for later.
- `docs/architecture/collision/actor-vs-actor-collisions.md` still describes
  full-body triangles as the later architecture goal. Its opening line says
  `2026 10 08 1600 jorj todo explain how actors vs actor collisions work`.
  The router says to report that TODO and recommend wording, but not edit that
  document. Suggested addition: explain that the first delivery is root
  capsules with flat-top support for all three pairings, and that animated
  per-limb triangles are a later phase. The current draft already proposes
  `config/actor-collision.json` and actor-preset overrides as possible owners;
  those are not active implementation evidence.
- `src/sim/simulate-tick.cpp:209-216` calls
  `resolveCapsuleVsCapsule` for the local player against each live NPC, except
  while the player is in ragdoll mode.
- `src/physics/movement/physics-collision-block.cpp:406-477` implements the
  shared capsule overlap function and a special standable-top check. The top
  check uses `kTopTolerance=0.08` and `kMaxSnap=0.35`. Ordinary overlap splits
  positional correction 50/50 and clamps velocity against the contact normal;
  it does not use actor mass or a configurable actor-pair impulse.
- The support branch only carries vertical support velocity in the observed
  code. Horizontal movement of the lower actor is not inherited there.
- `src/network/server-players.cpp:440-445` documents
  `resolvePlayerCollision()` as a no-op because accepted player transforms are
  client-authoritative. `src/network/server.cpp:892` calls this no-op.
- `src/network/server-npcs.cpp:1304` updates NPCs individually; no actor-pair
  collision pass was found. NPC target selection/attacks against other NPCs
  are combat behavior, not body collision.
- Therefore: local player-vs-NPC overlap has a capsule path; NPC-vs-NPC has no
  found physical pair path; player-vs-player has no active server solver; and
  server-authoritative player-vs-NPC resolution is not established.
- The current overlap test runs after movement and checks current capsule
  overlap. A swept actor-pair check is not present in this path, so fast actors
  may cross between samples. This is a source-level risk, not a reproduced
  runtime failure.
- `config/collision.json` contains world/movement collision settings. Player
  capsule dimensions can come from avatar/rig movement data. No active
  actor-pair collision profile keyed by actor preset was found. Verify NPC
  capsule dimension selection before deciding whether to add JSON. Keep
  `npc-difficulty.json` for NPC behavior tuning, not collision geometry or
  shared solver rules.
- The shared collision owner should gather every live player/NPC capsule after
  fixed-tick movement, form candidate pairs, process them in stable actor-ID
  order, run bounded overlap/support response, and publish server results for
  client reconciliation. A broadphase is needed as swarm size grows; naive
  pair testing grows as N(N-1)/2 per tick. Profile before selecting an
  optimization.

## Documents and focused review

Read `docs/ROUTER.md`, the NPC behavior, movement, networking, collision,
actor-preset, ragdoll, and performance specifications/architecture, the
JSON configuration architecture, the 2026-10-08 actor-collision evidence
investigation, and the runtime-scenario workflow. Applied:

- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/efficiency-checker-v1.md` (source-level scaling review)

The docs scan also found:

- `docs/specs/avatar/avatar-editor.md:5`: `also todo clean this doc!!!!!! cant have so much stuff inthis doc just spam`.
  Suggested fix: separate current editor behavior, configuration ownership,
  and workflow from obsolete notes, preserving useful history.
- `docs/architecture/collision/actor-vs-actor-collisions.md:1`: the TODO
  reported above; it was not edited due to the router instruction.

The router links to `docs/doc-review-09-03-2026.md` and
`docs/regressions/regressions-v1.md`, but neither path exists in this checkout.
Used `docs/regressions/README.md` and the relevant 2026-10-08 investigation
changelog instead.

## Validation and acceptance

- Current JSON value inspected: `config/aimbody.json` has
  `right_arm_pointing.follow_damping: 10.0`.
- No source code or configuration was changed.
- No tests, build, dev loop, runtime journal, multiplayer scenario, or visual
  acceptance was run. The reported reduction in arm jitter remains the user's
  in-game observation.
- `git diff --check` reports trailing whitespace in pre-existing raw notes at
  the tops of the already-modified spec files and other pre-existing user
  changes. No newly added paragraph was among the reported whitespace lines;
  unrelated notes were left untouched.
- No regression record was added: this source audit found missing/incomplete
  actor-pair behavior, not a newly introduced regression.

Human review is still needed for all three pairings, landing and standing on a
moving actor, high-speed contact, and multiplayer reconciliation. The JSON
shape/profile owner remains an open design choice.
