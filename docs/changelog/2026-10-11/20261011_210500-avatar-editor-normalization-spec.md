# Avatar editor and player-model normalization specification

- Date: 2026-10-11
- Status: `SPECIFICATION_COMPLETE_IMPLEMENTATION_PENDING`
- Scope: `docs/specs/avatar/avatar-editor.md`

## Request

Document a complete Blender-to-GLB contract and an opt-in normalization plan for avatar and player-body models. The plan must account for incorrect authored rotations, scales, locations, pivots, and coordinate bases without changing unrelated GLB resources.

## Result

Added `docs/specs/avatar/avatar-editor.md` with:

- Explicit model-class and policy gates so only default/custom player-body resources use player canonicalization.
- A required Blender hierarchy, coordinate frame, object-origin rules, transform-application rules, and GLB export checklist.
- Optional GLB metadata for declaring source basis and authored pivots.
- A staged, deterministic normalizer that parses immutably, resolves the source basis, bakes root/node transforms, derives pivots and axes, validates scale/determinants, and emits an immutable canonical model.
- One normalized output consumed by rendering, procedural animation, weapon/hand binding, collision, hybrid/ragdoll, replay, and replication paths.
- Explicit handling for the Boomer failure class: root 90-degree corrections, arm geometry on the wrong local axis, and centered/wrong pivots.
- Failure behavior, structured diagnostics, editor behavior, a Blender helper plan, pure/model tests, real runtime validation through the dev loop, acceptance scenarios, migration rules, and implementation slices.

## Evidence

- Source review: the edited specification contains 800 lines and covers Blender export through runtime acceptance.
- `git diff --check`: passed.
- Build/runtime: not run; this turn changed documentation only, and the specification explicitly marks implementation as pending.
- Human acceptance: not claimed.

## Existing documentation TODOs observed

The repository-wide documentation scan found these pre-existing markers and intentionally did not edit them:

- `docs/features/TEMPLATE.md:11`: `todo make this nicer and clearer cleaner` — replace with the intended feature-template organization and completion criteria.
- `docs/features/spawnvelocity/spawnveloc.md:11`: `todo make this nicer and clearer cleaner` — replace with a concrete explanation of spawn-velocity ownership and examples.
- `docs/features/README.md:29`: `this todo explain how this is like` — explain the feature-record relationship and required fields.
- `docs/specs/dev-loop/dev-loop.md:1`: `todo explain what` — state the missing concept and its operational owner.
- `docs/specs/dev-loop/dev-loop.md:16`: `todo explain like what the point of hti` — explain the purpose and user-visible role of the referenced mechanism.
- `docs/specs/hotreload/hotreload.md:1-3`: `todo explain` / `todo explain the direction...` — document the intended hot-reload direction and boundaries.
- `docs/specs/destructible-world/destructible-world.md:1`: `todo - explain the destructible world vision...` — document the vision, collision relationship, and balancing constraints.
- `docs/specs/debug-logging/AI-fix-live.md:43`: `todo ... put this in here generally...` — move the general workflow into the appropriate centralized logging specification.
- `docs/specs/music/music-midi.md:1`: `todo explain ummmm` — replace with the intended MIDI behavior and ownership.
- `docs/specs/music/music-fadein.md:1`: `todo explain` — document the fade-in contract and timing.
- `docs/specs/gui/guiv2.md:1,3`: GUI/terminal ownership TODOs — specify command-first behavior and the GUI delegation contract.
- `docs/specs/gravity-gun/gravity-gun.md:1`: `todo - implement... like explain...` — define the adapted gravity-gun behavior and implementation boundary.
- `docs/specs/colliisons/collisions.md:1`: `todo explain` — add the missing collision-system explanation.
- `docs/specs/moving-physical-objects/moving-physical-objects.md:1`: `todo - explain moving...` — document ownership and collision integration.
- `docs/specs/collectibles/collectibles.md:1`: `todo explain` — add the missing collectible lifecycle and behavior contract.
- `docs/specs/weapons/tool-weapon-ideas.md:1`: `todo explain like` — define the intended tool-weapon behavior.
- `docs/architecture/player-npc-systems/player-npc-systems.md:1`: `todo explain` — add the missing player/NPC shared-system explanation.
- `docs/specs/monetization/monetization.md:1`: `todo explain better enshittification` — replace with the intended product principle and constraints.
- `docs/specs/visuals/visuals.md:1`: `todo explain this more like` — expand the visual-system direction.
- `docs/specs/gamemodes/counterstrike.md:2385,2530`: hot-reload and aggregated-debug TODOs — define the desired workflow and summary cadence.
- `docs/architecture/player-npc-systems/npc-movement.md:1`: `todo - explain` — document NPC movement ownership and behavior.
- `docs/specs/gamemodes/caravan.md:1`: `todo explain` — add the missing mode specification.
- `docs/specs/terminal/terminal.md:3`: `todo explain` — document terminal ownership and command behavior.
- `docs/specs/john-fister/john-fister.md:1`: `todo make better` — replace with concrete completeness criteria.
- `docs/architecture/player-npc-systems/npc-agent-work-20261006.md:1`: `todo explain ok` — replace with the missing design explanation.
- `docs/specs/gamemodes/gamemodes.md:191`: `Todo explain this` — explain the referenced gamemode concept.
- `docs/specs/gamemodes/freeforall.md:1`: `todo explain` — add the missing free-for-all contract.
- `docs/specs/gamemodes/infinite-dungeon-slayer.md:1`: `todo - explain...` — document the mode vision and relationship to the RPG loop.
- `docs/specs/replay/replay.md:1`: `todo explain -` — add the missing replay behavior.
- `docs/specs/gamemodes/flood-escape.md:1`: `todo explain` — add the missing mode contract.
- `docs/specs/gamemodes/highway-swim.md:1`: `todo explain` — add the missing mode contract.
- `docs/specs/gamemodes/defend-telamon.md:1`: `doto todoexplain...` — document the intended tower-defense mode.
- `docs/specs/gamemodes/zombie-tower.md:1183,5304`: TODOs about explanation — expand the relevant product/mode descriptions.
- `docs/specs/gamemodes/mode-packs.md:168`: `todo make it look nice and better` — define the desired mode-pack presentation and completeness criteria.
- `docs/specs/gamemodes/minecraft.md:1`: `todojorj explain` — add the missing mode explanation.
- `docs/specs/gamemodes/workatasandwichshop.md:1`: `todo explain` — add the missing mode contract.
- `docs/specs/gamemodes/teamdeathmatch.md:1`: `todo explain` — add the missing mode contract.
- `docs/specs/gamemodes/supersmashbrosmelee.md:1`: `todo explainbetter` — expand the intended adaptation.
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:1`: `todo explain ragdoll...` — document the ragdoll behavior and constraints.
- `docs/specs/procedural-infinite-world/procedural-infinite-world.md:1`: `todo - explain...` — document the generation vision and boundaries.
- `docs/specs/networking/networking.md:13,15,936`: product-direction and 60 Hz logging TODOs — split product vision from the operational/network logging contract.
- `docs/architecture/collision/actor-vs-actor-collisions.md:1`: `todo explain...` — add the actor-collision ownership and behavior explanation.
- `docs/architecture/code-ownership/code-ownership.md:3,259`: expansion/generalization TODOs — define the intended ownership and generalization rules.

