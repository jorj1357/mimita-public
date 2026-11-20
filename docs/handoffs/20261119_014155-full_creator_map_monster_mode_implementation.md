# End-to-End Creator, Monster, Mode, and Infinite World Implementation Handoff

**Purpose:** give one implementation owner a complete, reviewable goal from the
normal MiMITA executable through map/monster authoring, live play, package
launch, and runtime proof.
**Audience:** the single agent assigned to implement and close this feature.
**Does not:** authorize a separate checkout, branch, dev-loop, compiler/build
request, executable code in content packages, or claims based on config/build
output alone.

## Ownership directive

The assigned agent owns this feature end to end until its required source work,
live executable scenarios, `events.jsonl` evidence, visual checks, and final
changelog are complete. Do not split shared-source editing among agents. Do
not stop after updating specifications, compiling, showing a UI mock, or
passing JSON validation. When a slice cannot be proven, report the exact
missing source/build/runtime/human-review evidence and continue independent
work; never label the whole feature complete prematurely.

This handoff is an implementation brief, not a replacement specification.
The linked specifications own detailed behavior and event fields. Resolve any
contradiction using `docs/skills/spec-behavior-review-v1.md` and the
specification's stated decision. Do not silently change the product contract
to match partial code.

## Product result

From one ordinary MiMITA executable, a creator can:

1. Choose **Create → New Map**, name a project, choose a blank map or clone an
   installed world, and enter the Map Editor without a terminal or restart.
2. Place, move, rotate, and scale blocks; assign installed textures/materials;
   place supported lights, player spawns, individual monster placements, and
   monster spawn volumes; save and reopen the same project revision.
3. Make a monster from an editable six-part primitive blockout or import a
   supported model, edit its parts, joints, dimensions, textures and collision,
   create idle/walk/attack/hurt clips, add attack hitboxes and sounds, and
   optionally attach a weapon model.
4. Spawn that monster individually or through its authored spawn volume, run a
   local server-authoritative Play Test, inspect it live, tune data such as
   health/damage/attack hitboxes, and see a validated new revision activate
   without restarting MiMITA.
5. Author/run a data-driven Zombie Attack wave mode and the Bloodtest sandbox
   using shared actor, damage, movement, and spawn systems.
6. Clone an Infinite XXITY world and edit chunk sizes, whole-part materials,
   streaming radius, LOD/budgets, collision coverage and fog without mutating
   the source world.
7. Open **My Creations** and play a saved local project offline. Use the
   Community surface to distinguish local/content launch from joining a live
   network server. Publish/import packages using the existing data-only
   distribution model; never execute downloaded native code or scripts.

Switching editor/play mode and applying supported content edits does not
restart the game. Development changes to non-reloadable engine code may still
need the canonical dev-loop to publish a new executable; do not describe that
development build boundary as a content-edit restart.

## Required reading before implementation

Read current `AGENTS.md` and `docs/ROUTER.md`, then the relevant sections of:

- `docs/specs/creators/monster-authoring.md`
- `docs/specs/world-projects/world-projects.md`
- `docs/specs/entity-editor/entity-editor.md`
- `docs/specs/avatar/avatar-editor.md`
- `docs/specs/animations/animations.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/specs/actor-presets/actor-presets.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/gamemodes/zombie-attack.md`
- `docs/specs/gamemodes/bloodtest.md`
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/procedural-infinite-world/procedural-infinite-world.md`
- `docs/specs/gui/guiv2.md` and `docs/specs/distribution/distribution.md`
- `docs/specs/debug-logging/debug-logging.md` and
  `docs/specs/debug-logging/canonical-jsonl.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md` and
  `docs/specs/dev-loop/dev-loop.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/spec-writing-v1.md`, `docs/skills/logging-checker-v1.md`, and
  applicable collision/performance/asset review skills from the Router.

Read the latest relevant changelog and inspect `git status` before editing.
Identify and preserve every pre-existing change. Do not overwrite or claim
unrelated source, config, asset, or changelog work.

## Current ownership and implementation starting points

These are existing owners to audit and extend, not invitations to create
parallel systems:

| Responsibility | Existing source/owner to trace first | Current foundation and gap to resolve |
|---|---|---|
| Map editing | `src/mapeditor/map-editor.*`, `src/mapeditor/map-editor-commands.cpp`, `MapConfigRegistry` | The loaded-world editor already has selection, block/entity edits, transform properties, undo/redo, save/reload, collision rebuild, and `map-editor.autosaved`. Reuse those operations. |
| Project persistence | `src/mapeditor/map-editor-projects.cpp`, `src/worldproject/world-project-store.*` | `worldproject` supports list/create/open/save/fork/version/export/import/recovery. Its `create` captures the current map. It does not by itself create a blank starter world, load it into the live runtime, and enter the editor. |
| Editor activation | `MapEditor::setActive` in `src/mapeditor/map-editor.cpp` | Activation currently requires `GAME_PLAYING`, a valid `worldPtr`, and `WORLD_LOADED`. Trace the game/world lifecycle and create/load/activate in the correct order; do not bypass these invariants. |
| Runtime map edits | `src/gamemode/map-config.*`, `src/gamemode/map-edit-applier.*`, `src/worldproject/world-edit-session.*` | Reuse the map registry and typed world edit path where applicable. Keep saved project revision, active runtime revision, render geometry, and collision revision correlated. |
| Avatar and animation | `src/entities/player-avatar-animations.cpp`, avatar registry/loaders, and `config/avatars`/`config/animations` owners | `anim.set.reloaded` already exists. Trace exact supported rig, clip, asset, and reload formats before extending them. |
| NPC spawn and behavior | `src/network/server-gamemode.cpp`, `src/network/server-npcs.cpp`, `src/npc/*` | Existing encounter spawning emits `monster.spawn-zone-activated` and `monster.spawned`; extend these owner events instead of creating a duplicate spawn path. Trace map entity and NPC role resolution. |
| Body damage/blood | shared actor damage, body-part, projectile and presentation owners | Bloodtest's existing body-part damage/layer specification is separate from new body authoring, capsule editing, stamina/sprint, and repeatable projectile test-bench work. Trace before adding fields or geometry owners. |
| Infinite World | `src/procedural/infinite-world-runtime.*`, `src/world/streamed-world-layer.*`, active worldgen/mode configs | Existing stream/profile/chunk/collision events and budgets exist. The current XXITY generation radius and visual/far radius are different controls; keep those meanings distinct. |
| GUI and entry/launch | shared JSON UI primitives, current menu/server-browser actions | The requested Create/My Creations/Community actions must call shared named actions. Preserve the normal server-browser Join semantics. |
| Diagnostics | `StructuredLogger` and each feature's existing owner | Use canonical per-process `events.jsonl`; no new unowned log file and no success event before the operation has actually applied. |

The following target event names had no source implementation when this brief
was written: `mapeditor_create`, `mapeditor_clone`,
`creator.project.action.applied`, and `monster.tuning.applied`. Search again at
implementation time. An equivalent feature may have landed since this audit.
Existing events `world-project.created`, `map-editor.autosaved`,
`anim.set.reloaded`, `monster.spawn-zone-activated`, `monster.spawned`,
`world-stream.profile`, `world-stream.update`, `world-stream.chunk.applied`,
`world-stream.collision.sync`, `performance.fps`, and
`performance.server-tick-window` should be traced and extended at their owners
where applicable. The fog-application and combined Infinite World performance
summary contracts still require explicit verification/implementation.

## Full implementation scope

### A. Create flow and map editing

- Implement the visible Create screen as the normal path. `mapeditor_create`
  and `mapeditor_clone` are terminal aliases to the same project service, not
  alternate implementations.
- Create a new native project with a starter floor and player spawn, load its
  working map into the actual running game, activate Map Editor, and report
  each durable/load/activation result separately.
- Keep edit and play in one process. Entering Play Test uses the saved project
  revision and the real authoritative simulation. Returning restores project,
  selection, and camera state without retaining temporary test damage/spawns.
- Ensure create/load/activation failures leave recoverable project data and do
  not silently keep editing a different map.
- Finish map interaction using the existing Map Editor: create/select/edit
  blocks; move/rotate/scale; assign installed material/texture; place lights,
  player spawns, individual monster placements and monster spawn volumes;
  undo/redo, autosave, reopen, and collision/render synchronization.
- Clone Infinite XXITY into a new project with its world/map/generation
  dependencies and per-project overrides while keeping the source unchanged.

### B. Monster, weapon, animation, and attack authoring

- `New Monster` creates an animatable, spawnable primitive rig with head,
  torso, arms, legs, stable part IDs and named pivots. Support part transform,
  dimensions, material/texture and replacement from supported model nodes.
- The first imported model format is rigid multipart GLB. Preserve the source;
  show the node tree, mapping, missing nodes and validation. Do not silently
  flatten skeletal/armature files. Expand importer formats only after a
  separate tested schema/rig decision.
- Keep render mesh, body damage hit geometry, living movement/world collision,
  attack hitboxes, and articulated death ragdoll as visibly distinct layers.
  Reuse the existing actor/avatar rig and collision owners.
- Create/edit `idle`, `walk`, `attack_1`, and `hurt`; allow optional
  `attack_2`, `death`, `run`, and movement-specific `fly`, `crawl`, or `roll`
  clips. Timeline ticks match the fixed 60 Hz runtime sampler. Missing optional
  clips are explicit and do not masquerade as loaded.
- Author attack windup/active/recovery, event markers, damage settings, and
  sphere hitboxes per active simulation tick. The first attack format uses
  dense samples; display current and swept hit shapes. Preview is
  non-damaging; actual hit/damage is evaluated by the authoritative server.
- Bind idle/attack/hurt/death sounds from installed/project asset folders and
  prove the sound reached playback. Human listening is still required.
- Allow an optional weapon model attached to a named hand/socket. Model
  appearance does not create a second weapon/damage system.
- Provide individual spawn/reset for any monster. Also prove a map spawn
  volume creates the same selected monster/role/preset in live play.
- Implement required movement capabilities through shared movement/NPC owners
  (including authored flying Wasp and bouncing/rolling Griever behavior where
  those species are configured). Unsupported capabilities fail validation;
  they are never silently presented as working.

### C. Live tuning, Zombie Attack, and Bloodtest

- In local Play Test, edit health, damage, cooldowns, movement values, and
  attack hitboxes; validate and apply a complete revision at a fixed tick.
  Preserve last-good data on rejection. Specify whether changes affect the
  selected actor or future spawns; use the controls already defined in the
  creator spec. An in-progress attack retains its captured revision; the next
  attack uses the new revision.
- Implement Zombie Attack from `docs/specs/gamemodes/zombie-attack.md`: exact
  five successful spawns in wave 1, increasing count/speed/damage on later
  waves, configurable stat curves, newly unlocked monster types, clear/breather
  transitions, co-op defeat/end results, all on shared lifecycle/NPC/combat
  owners.
- Implement Bloodtest's requested Retrograd-inspired *direction* using shared
  actor systems, not an asserted exact recreation of another game. The target
  living body moves with the deliberate baseline and one or a configured set
  of torso/head/leg world-collision capsules; damage still follows body parts,
  and death ragdoll remains articulated. Add weaker jump, scoped fresh-press
  jump-repeat delay (presses during delay are discarded, not buffered), smooth
  sprint ramp, and role-configurable stamina. Use one shared movement method
  for fighter (100 HP), juggernaut (2,000 HP, very slow), and monster (1,000 HP,
  faster and stronger jump).
- Add the Map Editor local destruction test bench: select a reusable physical
  object definition, launch it with recorded shape/dimensions/mass/velocity at
  a test NPC/ragdoll, reset and replay the same recipe, and inspect per-tick
  contact/body-layer/blood effects on the existing animation timeline. This
  runs shared projectile/contact/damage/effects and cannot mutate a public
  session or saved project state.

### D. Infinite XXITY, launch browser, and sharing

- Expose chunk part dimensions, position/rotation, whole-part material/texture
  dropdown and collision update together in the cloned project's editor.
- Expose generated/connector radius, detail/LOD/proxy radius, collision radius,
  lookahead, queue budgets, and fog as separate controls. Do not call them one
  generic radius. Preserve the source project and existing shared mode config.
- Meet the acceptance profile in the Procedural Infinite World spec: 1,000 m
  horizontal world radius; approximately 1,000 m/s traversal through repeated
  cells; at least one active NPC; 60 Hz server; 240 FPS rendering target; a
  ten-minute measured workload with at least 99.99% of client frames at or
  below 4.167 ms and server ticks at or below 16.667 ms, and actual server rate
  59.9–60.1 Hz. Keep work bounded and use near/mid/far representations.
- Show visually that fog hides generation and no unready chunk gap is visible.
  Preserve a screenshot/short video with camera position, run ID and build ID;
  logs/config alone are not visual proof.
- Implement Create, My Creations, and Community/Play entry. My Creations offers
  Edit and Play Local offline. Community separates Content from Servers; Join
  remains network join. Installed/public packages resolve verified dependencies
  before play. Follow `distribution.md` for consent, hash checks, attribution,
  licenses and safe installation. No downloaded executable or arbitrary code.

## Delivery order and ownership boundaries

Follow these slices in order, while retaining the full end-to-end owner:

1. **Project bootstrapping:** blank project, same-action UI/CLI, live map load,
   editor activation, recoverable failure; prove project-created and editor
   active separately.
2. **Map authoring loop:** transforms/materials/spawns/volumes/save/reopen,
   Play Test/return; prove render/collision revisions and persistence.
3. **Monster basic loop:** primitive body, material selection, individual and
   volume spawn, then supported GLB import and sound references.
4. **Animation/combat loop:** clips, weapon attachment, attack timeline and
   actual server hit. Add runtime animation/hitbox proof and visual review.
5. **Live tuning and modes:** apply revisions at fixed ticks; finish Zombie
   Attack and Bloodtest behaviors and their dedicated journal scenarios.
6. **Infinite World:** clone, resize/material change, streaming/fog, NPC fast
   traversal and performance/visual evidence.
7. **Launch/share:** local My Creations launch and distinct server Join;
   package import/export and community content availability under Distribution.

Keep one owner per source of truth. Reuse `WorldProjectStore`,
`MapConfigRegistry`, `MapEditor`, avatar/animation registries, NPC role and
movement resolution, shared attack/damage/projectile paths, `StructuredLogger`,
and canonical world streaming. Before adding any state field/file/command or
event, search for an equivalent and record why extending it is insufficient.

Respect hard runtime rules: gameplay collision/damage/physics use the fixed
60 Hz tick; collision uses the cached broadphase and no per-query allocations;
players and NPCs share movement/combat behavior where practical; GUI is a
presentation/action caller, not a second gameplay implementation; VSync stays
forced off globally.

## `events.jsonl` runtime proof contract

Use the per-process server and client journals from one canonical dev-loop
run. Every record must carry its normal identity (`run_id`, `instance_id`,
`role`, `pid`, `build_id`, `tick`, `wall_time`). Success events mean the
operation was actually applied. Keep high-volume traces bounded to one
project/actor/attack/scenario; ordinary play logs transitions, not every
render-frame pose. Reuse existing events and extend fields at their owners;
never log a second success event for the same transition.

| Slice | Required success evidence (existing or target event) |
|---|---|
| Create/load | `world-project.created` then `map-editor.create.applied` with the same project/map and `world_loaded:true`, `editor_active:true`. |
| Map edits | `map-editor.entity.transform.applied` with before/after transforms and render/collision revisions; `map-editor.autosaved` with matching saved revision; `map-editor.spawn-volume.applied`. |
| Play/return | `map-editor.playtest.started` and `map-editor.returned`, matching project revision, mode, process and run. |
| Model/body | `avatar.model.imported`, `bloodtest.body.definition.loaded`, `bloodtest.collision.profile.applied`; fields include rig/body revision, part/joint counts, dimensions hash and selected collision proxy. |
| Animation/attack/sound | `anim.set.reloaded` plus bounded `anim.preview.tick`; `monster.attack.definition.loaded`; bounded `monster.attack.hitbox.sample` on each required active tick; server `monster.attack.hit` with health before/after; `monster.sound.started` plus human listening. |
| Spawn/live edit | Existing `monster.spawn-zone-activated` and `monster.spawned` extended with project/revision/source-volume/wave fields; `monster.tuning.applied` with prior/active revisions and activation tick. |
| Bloodtest test bench | `bloodtest.test-projectile.launched`, `.contact`, `bloodtest.body-layer.changed`, `.blood-effect.applied`, `.test-projectile.replayed`, all correlated by capture/recipe ID. |
| Zombie Attack | `zombie-attack.wave.started`; exactly five distinct successful `monster.spawned` records in wave 1; `zombie-attack.wave.modifiers.applied` per actor; `zombie-attack.roster.unlocked`; `zombie-attack.wave.cleared`; `zombie-attack.run.ended`. |
| Infinite XXITY | `world-project.infinite-world.forked`; size/material apply; `world-stream.profile`, `.update`, `.chunk.applied`, `.collision.sync`; `graphics.fog.applied`; one `infinite-world.performance.summary` with `passed:true`. |
| Launch hub | `creator.project.action.applied` for `edit` and `play_local`; prove the selected project/version actually loaded/launched. Server Join is separately confirmed by its network join/connection events. |

The authoritative event-specific field contracts live in:
`docs/specs/creators/monster-authoring.md`,
`docs/specs/gamemodes/zombie-attack.md`,
`docs/specs/gamemodes/bloodtest.md`, and
`docs/specs/procedural-infinite-world/procedural-infinite-world.md`. If source
uses different existing names, preserve the canonical name, extend its fields,
and update the owning spec before relying on it.

## Required live acceptance and artifacts

The owner must run the real executable against the current source generation
through the root dev-loop once the canonical loop publishes an eligible build.
Use actual UI/actions and ordinary gameplay; config parsing, project-file
presence, synthetic unit/self-tests, screenshots of a mock, or a successful
build do not substitute for live game behavior. For each scenario:

1. Identify the exact run manifest and server/client PIDs, executable paths,
   `build_id`, and journal paths. Ensure client and server build identities
   match.
2. Exercise only the feature named by that scenario; record which maps,
   actors, attacks, render/collision revisions, and wave transitions were
   actually reached.
3. Inspect the live server and client `events.jsonl`, then the final journals.
   Report exact records/line numbers for each pass condition and any first
   divergence. An unvisited actor/feature is `NOT_COVERED`, never a pass.
4. Retest rejected/invalid edits and verify the last valid active/saved
   revision remains intact.
5. Keep visual/audio/multiplayer acceptance separate from journal evidence.
   Capture the fog/no-visible-gap view, model/limb textures, animation/attack
   hitbox overlays and resulting blood response; human listening/viewing is
   required for appearance and sound quality.
6. Produce a final feature matrix with `PASS`, `PASS_WITH_HUMAN_REVIEW`,
   `BLOCKED`, `NEEDS_SPEC_DECISION`, or `NOT_APPLICABLE` for every acceptance
   item; include the exact executable identity and per-process journal paths.

Each bounded scenario must finish with a correlated `scenario.finished`
record whose `passed:true` is written only after all named assertions have
been evaluated. A launch, build, event sample, or open-ended journal without a
terminal scenario verdict is incomplete evidence.

Required end-to-end scenarios:

- **Create/editor:** make a new blank map from the UI; edit block transforms,
  materials, spawns and a monster volume; save, reopen, Play Test and return.
- **Monster:** create primitive and import supported GLB; author requested
  clips, body/collision, attack and optional weapon/sounds; spawn individually
  and from a volume; confirm a real server hit and successful live damage edit.
- **Zombie Attack:** five wave-one spawns; later wave count/speed/damage rise;
  later monster pool unlock; clear/breather/next wave; defeat results.
- **Bloodtest:** compare roles on the shared movement method; verify capsule
  world collision vs per-part damage; jump delay, smooth sprint and stamina;
  replay a projectile recipe into a resettable NPC/ragdoll and observe actual
  layer/blood changes.
- **Infinite XXITY:** fork without changing source; edit size/material; run
  1 km radius and 1 km/s with an NPC; meet frame/server-tick gates; inspect fog
  visually for hidden generation and zero exposed gaps.
- **Launch/distribution:** use My Creations Play Local offline; separately
  join a server requiring a package, verify package identity/dependencies and
  expected prompts or opted-in fetch, then start the named content.

## Shared-checkout, build, and process rules

These are mandatory for the assigned agent under the current project rules:

- Work in `C:\\mimita-v9` on its current checkout/branch. Do not create a
  worktree, branch, mode-2 lane, second checkout, or parallel editing agent.
- Inspect current status and preserve unrelated changes. Only this assigned
  owner edits shared source files during this feature.
- Agents edit and observe; agents do not run builds or compilers. Do not run
  `build.py`, `build_agent.py`, `buildv2.py`, `module-build-worker.py`,
  `ccache`, compiler commands, `dev-loop.py --request build`,
  `--request game-dll`, `--request cold-link-trial`, or `--wait-for-build`.
  Never set `MIMITA_BUILD_OVERRIDE`.
- The root dev-loop owns automatic build/publication. Do not start a second
  loop, poll in a loop, or wait on a build. Allowed inspection/control commands
  are `--status`, `--list-instances`, `--inspect`, and `--ensure-running`.
  Preserve unrelated or user-owned runtime processes; inspect ownership before
  any launch/control action.
- The user explicitly requires real runtime proof. When a matching current
  executable becomes available through the root loop, run the target scenarios
  and read that run's exact journals. If the executable/build is unavailable,
  keep implementing and report the validation blocker; never substitute an
  older unrelated run for proof.
- Do not add synthetic tests as the first proof path. Pure isolated
  serialization checks may supplement but cannot replace live executable
  evidence. Keep diagnostics bounded and at the feature owner.

## Completion conditions

The feature is done only when all required slices above have source
implementation, canonical dev-loop runtime evidence, and required human visual
or audio acceptance. The work must also:

- leave one final changelog with source/build/runtime/human-review claims
  separated, exact journal locations, and any unresolved TODOs or blockers;
- update the owning specs when implementation discovers a necessary
  product-level correction, without silently weakening acceptance;
- preserve pre-existing unrelated work and report it as such;
- state any remaining capability (for example unsupported skeletal GLB import
  or remote discovery) as incomplete instead of claiming total completion.

## Related specifications and procedures

- [Creator/monster authoring](../specs/creators/monster-authoring.md)
- [World projects](../specs/world-projects/world-projects.md)
- [Zombie Attack](../specs/gamemodes/zombie-attack.md)
- [Bloodtest](../specs/gamemodes/bloodtest.md)
- [Infinite World](../specs/procedural-infinite-world/procedural-infinite-world.md)
- [Runtime scenario validation](../workflows/runtime-scenario-validation.md)
- [Agent build authority](../specs/dev-loop/agent-lanes-build-authority.md)
