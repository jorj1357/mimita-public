# Monster and Game Content Authoring

**Purpose:** define one creator workflow for authoring, testing, saving, and
sharing maps, monsters, and data-driven game modes.
**Audience:** creators, gameplay, avatar, animation, editor, and distribution
contributors.
**Does not:** replace the owners of map JSON, avatar rigs, animations, NPC
behavior, actor presets, gamemode rules, or asset loading. It does not allow
downloaded packages to execute native code or scripts.

## Creator outcome

From inside the game, a creator can make or open a project, place map geometry
and gameplay entities, author a monster and its behavior, preview the result,
and save the project as readable JSON plus referenced assets. The project can
be played locally before it is shared. A creator can spawn any authored
monster individually in the editor or a local test session without starting a
wave or full campaign.

The editor is a single project workflow with focused views for map, avatar
rig/collision, animation, attacks, sounds, and mode flow. These views edit
existing system-owned data through the same validated document/save path; they
do not maintain parallel editor-only copies of game state. Animation editing
reuses the Avatar Animation Editor's live preview. The map view reuses the Map
Editor's selection, transform, undo/redo, and autosave behavior.

## In-game create, edit, and play workflow

The target path starts from the normal MiMITA executable. A creator should not
need a terminal, an external JSON editor, a separate game executable, or a
manual build to make and play a content project.

1. Open MiMITA and choose **Create → New Map**. Name the project and choose
   **Blank Map** or **Clone World**. Blank Map creates a starter floor and
   player spawn, loads the new working map, and enters Map Editor. Clone World
   shows eligible installed worlds and creates an independent local fork
   before opening it. Invalid or existing project IDs are rejected without
   overwriting anything.
2. Choose **Edit Map** to enter the existing Map Editor on that project's
   active map. Place and transform blocks, choose materials/textures from the
   installed asset library, place supported lights, and add player spawns,
   individual monster placements, and monster spawn volumes. Outliner search,
   property editing, undo/redo, validation, and save operate on this same
   project's JSON and referenced assets.
3. Choose **Create Monster** or open a monster already in the project. Start
   from an editable primitive rig or import an existing supported model. Map its
   mesh nodes to body parts, preview collision, create locomotion clips, and
   author attacks with animation keyframes, active spheres, sound events, and
   cooldowns. Edit body dimensions, named skeleton joints, damage layers, and
   living world-collision capsules as separate views of the same rig. Save the
   monster into the project and make it available to map placements and
   spawn-volume pools. Bloodtest-specific damage, role movement, and projectile
   replay behavior is owned by `docs/specs/gamemodes/bloodtest.md`.
4. Choose **Create Mode** or **Play Test**. A mode wizard selects the project's
   map, roster, and flow. Play Test may use a default local survival mode so
   the creator can try the map before authoring a custom mode.
5. Play the real server-authoritative simulation using the current saved
   project revision. The creator can stop/pause the test and return to the same
   editor selection and camera state, change map/monster/mode data, save, and
   play again without closing or relaunching MiMITA.

The terminal command remains available as a power-user and first-slice route:
`mapeditor_create <project-id> [title]` and
`mapeditor_clone <source-world-id> <project-id> [title]` call the same project
service and actions as the Create screen. They are not separate workflows.

The editor and local play test use one running application and the same
project root. **Edit Mode** suspends gameplay simulation; **Play Test** hides
authoring panels and runs normal gameplay at the configured tick rate. Editor
rendering, previews, and asset browsers must not run during Play Test unless
shown, and authoring tools must not add work to fixed-tick gameplay loops.
Returning from Play Test restores the editable project, not the test's
temporary actor/map state.

## Delivery slices

Implement this as vertical slices that each leave a usable path in the normal
executable:

1. **Map loop:** Create screen, blank project creation, block add/select/
   move/rotate/scale, player spawn, save/reopen, Play Test, and return-to-edit.
2. **Monster placement:** monster spawn volumes, individual spawn/reset,
   primitive six-part monster, per-part material selection, and role/health
   setup. Prove a volume can spawn the selected monster in local Play Test.
3. **Monster authoring:** supported GLB import, rig/body/collision views,
   `idle`/`walk`/`attack_1`/`hurt` clips, attack sphere timeline, sound
   references, and optional weapon attachment.
4. **Live tune and launch:** safe live data revisions, My Creations list,
   Play Local launch, and the Community screen's separate Servers and Content
   lists. Keep local launch independent of publishing/network availability.
5. **Sharing and discovery:** portable package export/import first, then remote
   catalog, dependency fetch, signing, and moderation under the Distribution
   specification.

Each slice must pass its own `events.jsonl` acceptance scenario before the
next one is treated as complete. Later slices may not be claimed from earlier
map-only evidence.

## Native project format for the first version

Use the existing native world-project directory and JSON format as the
editable source of truth. Do not invent a second `.map`, `.mod`, or archive
format for the first editor slice, and do not put creator projects under the
repository's `config/maps/` directory.

The project store owns `project.json`, `working/map.json`, immutable version
snapshots, and recovery drafts. Map JSON references models,
textures/materials, sounds, animations, and other assets by stable IDs or
project-relative paths. Project-owned assets are inventoried through the
project dependency/blob store; built-in assets remain references. The editor
changes the working revision; explicit save/version actions preserve prior
versions. Existing project
export/import is the later sharing path. Automatic download/discovery and a
polished portable archive wait until the local create-edit-play loop and asset
dependency inventory are reliable.

This makes sharing easier later without blocking creation now: the editable
project already has a manifest, version identity, and asset references. Export
can bundle or reference permitted assets without changing the in-game editing
format.

## Create action and terminal command

`mapeditor_create <project-id> [title]` is the terminal entry to the shared
Create action. The normal in-game entry is Create → New Map. Both use the
map-editor/project command module, existing `WorldProjectStore`, and
`MapConfigRegistry`; the GUI must not create a separate project path.

On success it:

1. validates the project ID and confirms the project does not already exist;
2. creates a starter map document containing a floor block and player spawn;
3. writes the native project manifest, working JSON, and initial immutable
   version under the per-user world-project root;
4. loads the new working map and enters Map Editor on that map; and
5. reports the project ID and working-file path; emits
   `world-project.created` after the initial documents are safely written and
   `map-editor.create.applied` after the new map is loaded and the editor is
   active.

If local world or editor activation fails, the command reports the failed step
and leaves the project recoverable. It must not silently leave the editor on a
different map. `worldproject create` remains the lower-level action for
capturing the current map as a project; it does not replace creating and
entering a new starter map. The GUI and terminal call the same validated
operation and return the same structured result.

`mapeditor_clone <source-world-id> <project-id> [title]` creates an independent
project from a named world variant. For `infinite_xxity`, the clone includes
the map JSON, generation-zone settings, worldgen profile, referenced chunk
definitions, seed/version/dependency identity, and permitted asset references.
The source remains unchanged. It keeps the shared `connected_world` lifecycle
unless the creator explicitly creates a separate mode; world-specific
streaming and fog overrides are stored with the cloned project rather than
mutating the shared mode defaults. The clone operation reports source and
destination revisions and each copied dependency hash.

Map editor commands should also cover open/list/save/playtest/return-to-edit
with stable arguments and structured results. Visual controls call these same
operations.

## General animation and hitbox authoring

Animation and hitbox editing applies to any supported actor or animated
object, not only monsters. Select content by stable ID and target kind (actor,
weapon, or supported object), then edit the referenced rig, animation clips,
and action data.

### Body rig and collision data

The imported avatar rig remains the source of truth for mesh nodes, named
parts, skeleton joints, and render dimensions. Store the authorable body block
in that avatar's existing versioned definition; do not add a second body file
that can drift from the rig. Each part has a stable ID, parent/joint binding,
local transform, dimensions, and damage-layer binding. A separate collision
profile references one whole-body capsule or several named-part capsules in
avatar-local space. Collision profile data identifies `living_world`,
`actor_damage`, or `ragdoll_world` purpose so the visible body, damage surface,
and living movement proxy remain distinct. The editor shows all three layers
with separate toggles and reports which profile is active for the selected
role.

The first editor version supports capsule dimensions and offsets for the
living movement proxy; it does not infer physical limbs from render meshes.
Damage continues to query the authored body parts. Ragdoll simulation continues
to use the articulated physical-body definition owned by the ragdoll system.
Bloodtest owns its role and acceptance behavior in
`docs/specs/gamemodes/bloodtest.md`.

**New Monster** creates a blockout avatar with editable head, torso, two arms,
and two legs joined to named pivots. Each part can be resized, moved, rotated,
assigned a material/texture from the installed asset library, or replaced with
a node from an imported supported model. The blockout must be animatable and
spawnable before the creator adds custom meshes. Per-part texture selection
and the model's UVs are supported; texture painting is not required for the
first editor version.

- Clips store named-part poses at integer fixed simulation ticks. Locomotion
  such as walk loops; attacks are one-shots with windup, active, recovery, and
  end markers.
- Each attack has one or more stable hitbox tracks. A sphere track stores its
  radius, center sample, and active flag per tick in actor-root or named-part
  local space. The editor draws the current and swept sphere samples beside
  the animation timeline. Other hitbox shapes can extend the same schema when
  needed.
- Body-part collision and attack hitboxes are separate: body collision handles
  world contact and being hit; attack tracks define where an active move can
  hit a target. Give them distinct overlays and visibility controls.
- Monsters, weapons, and supported objects use shared fixed-tick action and
  hitbox execution. A weapon may reference the same clip/hitbox format as a
  monster attack, while its action owner retains eligibility, damage,
  cooldown, and effects.
- Timeline scrub and one-tick stepping sample exactly the fixed-tick data the
  runtime consumes. Preview cannot change authoritative hit timing or apply
  damage.

### Minimum attack-track JSON shape

The implementation needs a concrete, versioned data shape before the editor
is built. Keep attack timing and hit geometry together in a monster-owned
`attacks.json` beside its avatar data; the runtime resolves the named
animation clip from the avatar's existing animation registry.

```json
{
  "version": 1,
  "attacks": {
    "claw_swipe": {
      "animation_clip": "attack_swipe",
      "windup_end_tick": 8,
      "active_end_tick": 16,
      "end_tick": 30,
      "cooldown_ticks": 60,
      "hitboxes": [
        {
          "id": "right_claw",
          "shape": "sphere",
          "space": { "part": "rightClaw" },
          "samples": [
            { "tick": 8, "active": true, "center": [0, 0, 0], "radius_m": 0.4 },
            { "tick": 9, "active": true, "center": [0, 0, 0.4], "radius_m": 0.4 },
            { "tick": 10, "active": true, "center": [0, 0, 0.8], "radius_m": 0.4 }
          ]
        }
      ]
    }
  }
}
```

`space.part` is an optional rig-part ID; absent means actor-root local space.
Samples are dense across every active tick in the first version. Each attack
validates finite centers, positive radii, unique IDs, ordered phase bounds,
and a sample for every active tick. Damage/effect and target eligibility use
the existing shared combat definition/owner, not a second damage formula in
the editor. The editor can later offer sparse keys and interpolation, but it
must bake/preview the same resolved per-tick samples the server consumes.

## Runtime event evidence contract

Canonical `events.jsonl` is the proof record. Use the existing event envelope
(`run_id`, `instance_id`, `role`, `pid`, `build_id`, `tick`, and
`wall_time`) and the event fields below. A success event is emitted only after
the named operation has actually been applied, never when a button or command
was merely requested. Rejections use a distinct `*.rejected` event with a
reason and must not emit the success event.

| Success event | Owner and required event-specific fields | What it proves |
|---|---|---|
| `world-project.created` | Project store; `project_id`, `working_path`, `initial_version`, `manifest_hash`, `map_id`, `result` | The initial native project documents were persisted successfully. It does not alone prove the game loaded the map. |
| `map-editor.create.applied` | Project/map owner; `project_id`, `map_id`, `working_path`, `initial_version`, `editor_active:true`, `world_loaded:true`, `floor_id`, `spawn_id` | `mapeditor_create` created the starter project, loaded that project's map into a live world, and activated the editor. A project-file write alone is insufficient. |
| `map-editor.entity.transform.applied` | `project_id`, `map_id`, `entity_id`, `before_position`, `after_position`, `before_rotation`, `after_rotation`, `before_scale`, `after_scale`, `render_revision`, `collision_revision`, `result` | Move/rotate/scale edits reached both the live viewport and collision world. |
| `map-editor.spawn-volume.applied` | `project_id`, `map_id`, `volume_id`, `monster_ids`, `max_alive`, `spawn_interval_ticks`, `activation_policy`, `revision`, `result` | The saved volume resolves the selected monster pool and spawn limits. |
| `monster.spawn.applied` | `actor_id`, `monster_id`, `source_volume_id`, `project_revision`, `rig_revision`, `mode_id`, `tick` | The runtime created the selected monster from the authored volume. |
| `creator.project.action.applied` | `project_id`, `project_revision`, `action` (`edit` or `play_local`), `map_id`, `mode_id`, `run_id`, `pid`, `result` | The selected saved creation opened in the editor or launched into its local game session. |
| `map-editor.autosaved` | Existing map-editor owner; retain `map`, `path`, `revision`, `saved_revision`, `dirty_after_save`, `entity_count`, `block_count`; add `project_id` and `project_revision` for projects | The edited revision was written and is no longer dirty. |
| `map-editor.playtest.started` / `map-editor.returned` | `project_id`, `map_id`, `project_revision`, `mode_id`, `run_id`, `editor_active`, `gameplay_active`, `pid` | The local server-authoritative test began and returned to the same project in the same game process. |
| `avatar.model.imported` | Avatar/import owner; `avatar_id`, `source_path`, `source_sha256`, `rig_revision`, `node_count`, `mapped_part_count`, `unmapped_nodes`, `result` | The model was parsed and the declared rig mapping was installed. |
| `anim.set.reloaded` | Existing animation registry; `avatar`, `revision`, `activation_tick`, `clip_count`, `rig_parts`, `valid:true` | The new animation set loaded. For gameplay proof, pair it with `anim.preview.tick` or the runtime action/timeline events showing the selected clip advanced on the intended actor. |
| `monster.attack.definition.loaded` | Attack registry; `actor_id`/`avatar_id`, `attack_id`, `revision`, `animation_clip`, `windup_end_tick`, `active_end_tick`, `end_tick`, `hitbox_count`, `valid:true` | The server installed the validated attack and its timeline. |
| `monster.attack.hitbox.sample` | Authoritative server, bounded trace only; `actor_id`, `action_id`, `attack_revision`, `clip_tick`, `phase`, `hitbox_id`, `active`, `center_world`, `previous_center_world`, `radius_m`, `swept_query_result` | The server evaluated each expected sphere at the expected fixed tick and used it in the swept query. Emit once per sphere/tick only during a bounded editor test capture. |
| `monster.attack.hit` | Authoritative server; `attacker_id`, `target_id`, `action_id`, `hitbox_id`, `tick`, `damage`, `target_health_before`, `target_health_after`, `contact_position`, `attack_revision` | The configured attack actually hit the target and changed authoritative health. |
| `monster.tuning.applied` | Authoritative server; `actor_id`, `project_id`, `project_revision`, `previous_revision`, `active_revision`, `changed_fields`, `activation_tick`, `result` | A validated local live-tuning revision was installed on the selected actor at the fixed-tick boundary. |
| `monster.sound.started` | Audio owner; `actor_id`, `event_name`, `asset_path`, `asset_revision`, `voice_id`, `result:"started"` | The requested sound asset reached the audio playback owner. Human listening is still required to prove it sounded correct. |

High-frequency pose and sphere samples are disabled in normal play. The editor
can enable a bounded capture for one named actor and one attack, with a fixed
tick/record limit. The normal journal still records load, start, hit, finish,
and rejection transitions. Do not log every actor's full pose or every
non-active sphere every frame.

### Live acceptance journal

The in-game acceptance run must use the real executable and the canonical
dev-loop. Its client and server journals are separate and must match the same
`run_id`, `build_id`, project revision, mode, and actor IDs. The bounded test
scenario passes only when it sees, in order:

1. `world-project.created` followed by `map-editor.create.applied` with the
   project persisted, loaded, and the editor active;
2. `map-editor.entity.transform.applied` for block move/rotate/scale and
   `map-editor.autosaved` for the block, player spawn, and monster volume;
3. `map-editor.playtest.started` with the same project revision;
4. `avatar.model.imported`, `anim.set.reloaded`, and the intended per-tick
   animation preview/runtime events, followed by `monster.spawn.applied` for
   an actor created by the authored spawn volume;
5. `monster.attack.definition.loaded`, one sample for every active sphere
   tick, and `monster.attack.hit` against the deliberate test target with
   health decreasing;
6. `monster.tuning.applied` after a live damage/hitbox edit, followed by a
   later attack using the new revision; and
7. `map-editor.returned` for the same project, map, and process ID.

Also require a terminal `scenario.finished` with `passed:true` and every
assertion evaluated. Build success, JSON parsing, command receipt, client-only
echoes, or a hitbox sample without an actual server hit do not prove the whole
feature. Record the executable identity and exact client/server journal paths
in the implementation changelog.

### Live content updates and restart boundary

- Project data, animation clips, sound references, monster attack tracks, and
  other validated content are versioned documents. A successful save installs
  a complete validated revision at a safe runtime boundary and keeps the last
  valid revision on failure.
- An edit made during Play Test may be applied live when its owner supports a
  safe hot reload. Changes to an active attack or its sphere tracks take
  effect on a later attack; an in-progress attack keeps its captured revision
  through recovery. Structural map edits may request a bounded geometry and
  collision refresh, then report when the new revision is active.
- Switching Edit Mode and Play Test never compiles or relaunches the game. A
  content creator does not need to rebuild for JSON, model, sound, texture, or
  animation edits.
- Adding new engine behavior is code work, distinct from content authoring.
  That behavior is implemented and validated through the project's normal
  development/build workflow. The product goal is to keep editor and gameplay
  behavior in reloadable modules where their ownership allows it; a host/ABI
  change can still need a development build and restart. Do not promise
  zero-cost hot reload for code that is not reloadable.

The user-visible goal is no measurable gameplay slowdown from inactive editor
panels or preview tools. File parsing, asset import, validation, and collision
rebuilds are queued outside the fixed-tick hot path and must report progress or
failure rather than stall gameplay silently.

## Project and file ownership

A project has a stable ID and a root folder. Its project manifest references
content by package-relative paths and stable IDs; it never relies on absolute
paths from the creator's machine. Keep each concept in its existing owner:

| Content | Project data and owner |
|---|---|
| Map base model, authored blocks, materials/textures, lights, spawnpoints, and spawn volumes | Map JSON and assets, through the Map Editor / `MapConfigRegistry` |
| Monster mesh, named rig parts, collision geometry, and avatar identity | Avatar assets and `avatar.json` |
| Idle, locomotion, hurt, death, and attack clips | Avatar `animations.json` and shared animation clip library |
| Sound effects and voice sounds | Referenced sound assets plus the monster sound profile |
| Health, role, movement selection, and loadout | Actor preset and role |
| Attack selection, target rules, cooldowns, and behavior | NPC behavior profile and data-driven attack definitions, using shared combat owners |
| Mode phases, teams, wave rules, roster unlocks, and map selection | Gamemode JSON |

Adding a new document type or field requires a versioned schema and a clear
runtime owner. Do not copy textures, sounds, movement values, attack logic, or
animation clips into the map when a stable asset or definition reference works.

## Monster authoring workflow

1. **Create/import avatar.** Import a supported multipart model, map mesh nodes
   to stable part names, and set its normalized rest pose in the Avatar Editor.
2. **Set collision by body part.** Mark gameplay-relevant rig parts as
   collidable and assign a collision mesh or supported primitive per part.
   Render-only parts can be excluded. Collision meshes may be simpler than
   render meshes, but must visibly cover the intended body region and remain
   within documented mesh and per-tick budgets.
3. **Preview physical shape.** Show per-part collision in the viewport over
   world geometry and in an x-ray/debug view. The creator can toggle each
   part, move the animation timeline, and see the collision pose follow the
   final animated pose. A root capsule can remain for actor-pair broadphase;
   it cannot replace limb/body collision against the world or attacks against
   a monster's body parts.
4. **Author animations.** Use the existing animation editor to create and
   preview named clips such as `idle`, `walk`, `fly`, `roll`, `attack_1`,
   `hurt`, and `death`. Attack animation markers provide presentation timing;
   the authoritative attack definition owns when hit geometry becomes active.
5. **Assign sound set.** Select sound assets by package-relative path for
   idle/ambient, attack, hurt, and death events. A missing or invalid sound
   reports against the monster and falls back silently; it must not prevent the
   avatar or mode from loading.
6. **Define movement and attacks.** Choose a supported movement capability
   and tune its data, then add named attacks through shared attack/damage
   systems. Each attack declares its trigger, target/range conditions,
   cooldown, telegraph, active interval, hit shape, damage/effect reference,
   and recovery/interruption behavior. The preview can display telegraph and
   hit volumes without applying damage.
7. **Test individually.** Spawn this monster alone into the current test map,
   inspect its movement, world collision, attack markers, hit regions, and
   sounds, then save. Individual spawn is an editor/test action and does not
   silently change the authored map or live public match.

## In-game monster editor — first usable slice

The first useful editor is a set of tabs on the existing Avatar Editor, plus
the existing Map Editor's individual NPC spawn/test action. It is not a new
runtime. Each tab edits the same validated data consumed by the game.

### Import and rig

- `Import Model` accepts a supported multipart GLB and creates or updates an
  avatar folder without overwriting the source model. Show the GLB node tree,
  mesh preview, part names, transforms, and missing/duplicate-node warnings.
- Let the creator map imported nodes to semantic part IDs, set joint pivots,
  and choose `render only`, `body collision`, or `attack attachment` roles.
  Preserve unmapped nodes as render-only by default.
- For each collidable part, select its imported mesh or a supported primitive
  collision shape, then preview that shape against the current test map. Save
  the rig mapping in `avatar.json`; never infer that a model is valid solely
  from its filename.
- The first importer supports rigid multipart GLB nodes. If the input has
  skeletal animation or an unsupported skin/armature, explain the limitation
  and preserve the file; do not silently flatten or discard its rig.

### Animation timeline

- Show named parts on rows and fixed simulation ticks on the horizontal
  timeline. The creator can select a part, move/scrub the playhead, add/remove
  a keyframe, and edit its position/rotation/scale channels and interpolation.
- Provide looped locomotion clip preview (`idle`, `walk`, `run`, `fly`,
  `crawl`, or `roll`) and one-shot attack/hurt/death preview. Play, pause, step
  one tick forward/back, and reset to the imported rest pose.
- Walk clips are keyed per actor simulation tick. The author may use sparse
  keyframes with interpolation or dense one-keyframe-per-tick data. Preview
  sampling must match the runtime fixed-tick animation sampler.
- Save clips through the existing per-avatar `animations.json` / shared clip
  library format. Saving a valid clip reloads the active avatar at a fixed-tick
  boundary; malformed edits retain the last valid set.
- A new blockout monster starts with editable `idle`, `walk`, `attack_1`, and
  `hurt` clips. It can add `attack_2`, `death`, `run`, and movement-specific
  clips such as `fly`, `crawl`, or `roll`. Missing optional clips use an
  explicit fallback and are shown as missing in the clip list; they are never
  reported as authored or loaded.

### Attack and active-sphere timeline

- An attack is one timeline with named phases (`windup`, `active`,
  `recovery`), the attack animation, sound/event markers, and one or more
  sphere tracks. The timeline must visibly distinguish damage-inactive and
  damage-active ticks.
- Each sphere track has a stable ID, radius, and center expressed relative to
  the actor root or a named rig part. At every active tick, the editor shows
  the evaluated center and radius. Creators can add, move, resize, enable, or
  disable a sphere at the selected tick; use sparse keys with explicit
  interpolation or dense tick samples when precise motion is needed.
- Allow stepping through every simulation tick and scrubbing the attack while
  the monster preview plays. Draw the current sphere, its previous-tick
  position, and the swept volume used between ticks. A sphere is never active
  outside the attack's active phase. The preview is non-damaging.
- The server evaluates the same validated sphere track against actor body-part
  hit shapes on each active fixed tick. It owns eligibility, damage, cooldown,
  interruption, and hit results. Client rendering or editor frame rate cannot
  enable a hit or change its timing.
- Save attack definitions as versioned JSON beside the avatar and reference
  them by stable attack ID from the monster profile/role. Use the shared
  attack/action runtime and combat owner; do not implement an editor-only
  damage path. Reuse the existing Melee action timeline/phase-marker and
  animated hitbox contracts where their semantics match. Monster attack data
  must not be shoehorned into an unrelated player weapon loadout.

### Sound and individual runtime preview

- The Sounds panel lists available files under the selected package's sound
  folder and lets the creator bind event names such as `idle`, `attack`,
  `hurt`, and `death`. A play button auditions the selected sound. Runtime
  playback follows the same event markers/attack acceptance the server uses;
  sound preview alone is not proof that an attack event is wired.
- `Spawn Preview` creates one selected monster on an isolated local test map,
  with controls to pause/resume its AI, request a named attack, and reset it.
  Show movement/attack state and current asset revisions. It must not add a
  permanent map spawn or affect a public match.
- A monster may reference an optional weapon model and a named hand/socket
  attachment. The weapon follows that attachment in preview and Play Test;
  collision and attack behavior still come from the selected shared weapon or
  authored attack definition. A missing weapon asset is a visible validation
  issue, not a reason to fail the monster's other clips.

### Live test tuning

During local Play Test, the creator can select a spawned monster and edit its
validated health, direct damage values, attack cooldowns, movement tuning, and
attack hitbox tracks. The editor submits a new project revision to the local
authoritative runtime. The server validates it and applies it at a safe fixed
tick boundary; a rejected revision leaves both the running monster and last
valid saved revision intact. Existing actors receive role/health/movement
changes only when the creator chooses **Apply to selected monster**; future
spawn-volume instances use the new saved revision. A running attack keeps its
captured attack revision through recovery, and later attacks use the new one.
The UI shows active and pending revisions, and undo restores a new validated
revision rather than mutating the past. This tuning action is local to Play
Test and never writes into a public session.

The new panels should call host-owned services for document edits, validation,
undo/redo, save, preview spawning, and reload. The UI owns presentation and
input only. Preserve the existing host/module boundary for map-editor code.

Runtime systems remain authoritative: movement capabilities describe intent
and select shared physics/navigation behavior; they do not teleport or bypass
collision. For example, a Wasp selects a supported flying movement capability,
and a Griever selects rolling movement with bounce/contact parameters. If the
required shared capability is not implemented, the editor labels the choice
unsupported and cannot claim that preview data works in gameplay.

Monster attack hit shapes, limb collision, and world collision run on the
authoritative fixed simulation tick. They use cached broadphase candidates and
bounded work. No per-render-frame gameplay damage or unbounded per-limb
allocation is allowed. Debug views distinguish actor-pair capsules, static
world body-part collision, and active attack hit shapes.

## Map and mode authoring

The map view supports creating a map project, choosing a base model or native
geometry, selecting available textures/materials for blocks, placing lights,
and authoring spawnpoints and bounded monster spawn volumes. Spawn volumes
reference monster IDs or pools and explicit count/limit rules. They do not
embed a private copy of monster definitions. All placement, selection,
visibility, undo/redo, validation, and save behavior follows the existing Map
Editor contract.

The mode view edits a gamemode JSON document: stable mode ID, display name,
map references, teams/roles, phase flow, and optional wave/encounter data. A
wave may reference individual monster IDs and unlock later monster IDs at
specified waves. The mode composes the existing match lifecycle and monster
systems rather than creating its own movement, damage, or actor registry.
Creators can launch a local test session from the project and inspect which
map, mode, monster, and assets resolved.

## Create, My Creations, and Community launch surface

The executable exposes three direct destinations from its home/play surface:
**Create**, **My Creations**, and **Community**. The in-game Community screen
keeps two clearly separate lists under one navigation surface:

- **Servers** lists joinable live servers and retains the existing server
  browser's player count, ping, tags, and Join action.
- **Content** lists locally saved projects and installed world/mode packages.
  A local project card provides **Edit** and
  **Play Local** actions. A published/installed content card provides
  **Inspect**, **Install/Update** when needed, and **Play** when its entry mode
  and dependencies are available.

Selecting **Play Local** starts a local authoritative session using the
project's saved map and entry mode, then launches directly into that session;
it does not require publication, a community listing, or an Internet
connection. Selecting **Join** on a server remains a network join and is never
silently redirected to local content. If a server requires a content package,
the existing distribution rules verify its manifest and hashes, show required
downloads, and only then permit joining. The content browser does not replace
the server browser's query, refresh, or join semantics.

This design makes newly made things launchable without making a creator
publish first. Remote public discovery, moderation, package signing, and
automatic fetch policy continue to follow
`docs/specs/distribution/distribution.md` and are a later stage than local My
Creations launch. GUI presentation remains
data-defined under `docs/specs/gui/guiv2.md`; Create, Edit, Play Local, Inspect,
and Join call shared named actions rather than separate hard-coded screens.

## Data validation, save, and recovery

- Validate the whole project and every referenced file before save/export.
  Show errors at the owning object and property, such as a missing mesh node,
  invalid collision mesh, unresolved sound, unknown movement capability, or
  attack reference.
- Save human-readable, versioned JSON with stable IDs and relative asset
  references. Write atomically; retain the last valid document if parsing or
  validation fails. Editor undo/redo and map autosave behavior remain intact.
- Loading an older schema uses an explicit migration. Unknown required fields
  or unsupported schema versions fail clearly without partially installing a
  project.
- A local test or preview failure must not corrupt the saved project or change
  the active public match.

## Portable content package and download

A portable package is a data-only archive with a manifest, project JSON, and
the assets it owns or is permitted to redistribute. References resolve inside
the package or to documented built-in assets. The manifest includes package
ID, version, required MiMITA content/API version, entry mode/map IDs, asset
inventory, and hashes. Import/export is locally available without an account.

Packages contain no native libraries, scripts, arbitrary commands, or
automatic startup hooks. Import validates paths against traversal, file sizes,
schemas, asset types, and hashes before installation. A joining player's
client may offer or automatically fetch only the package data needed for a
session when the user has enabled that setting; first-time publishers and
unknown sources require a clear install prompt. Downloaded content is
installed in an isolated package directory and can be removed without
affecting built-in content. Network discovery, moderation, signing, and
publisher identity remain governed by the Distribution specification and are
not implied by local package support.

## Acceptance scenarios

1. From the executable's Create flow, make a blank map and enter Edit Map.
   Place a textured block, a light, an individual monster placement, and a
   monster spawn volume; save, close, reopen the project, and see the same
   authored data.
2. Import a multipart monster, assign limb collision, and verify debug shapes
   follow animation while the authoritative actor stays outside solid map
   geometry.
3. Import an existing multipart monster GLB, map its nodes, save, restart the
   editor, and confirm the original source and saved mapping both remain
   intact.
4. Author a per-tick walk clip and a one-shot attack clip, step through their
   ticks, save, and confirm the live monster uses the new validated revision.
5. Author an attack with active spheres, inspect every active tick and its
   swept preview, spawn the monster individually, and confirm the server's
   accepted hits align with the authored sphere track and active window.
6. Select sounds for idle/attack/hurt/death, trigger each event in a local
   scenario, and confirm the intended asset plays.
7. Author two attacks and distinct movement capabilities; verify attack
   cooldowns and hit windows use shared combat rules, and verify unsupported
   movement capability references produce a clear validation error.
8. Author a mode that spawns one monster type first and introduces another at
   a later wave; run it locally and inspect the resolved mode and monster
   roster.
9. Export and re-import a data-only package; missing files, invalid hashes,
   traversal paths, unsupported schemas, and executable content are rejected
   without damaging the existing project.
10. From the executable's Create screen, make a blank map without using the
    terminal; place, rotate, and scale blocks, create player spawns and a
    monster spawn volume, save, and reopen the project with the same transforms.
11. Create a monster from the primitive rig, assign per-part textures, author
    `idle`, `walk`, `attack_1`, `attack_2`, and `hurt` clips and hitboxes, then
    spawn it from the map's volume in local Play Test.
12. Change the selected monster's damage and attack hitbox during Play Test;
    confirm the server applies the validated revision at a fixed tick and the
    next attack/next spawn uses it without restarting MiMITA.
13. Open My Creations from the Community/Play surface and use **Play Local**;
    verify it launches the saved project directly. Confirm **Join** on a server
    remains a separate network action.

Compilation, editor preview, and JSON validation are separate from live
server behavior and human acceptance. Each must be reported as its own
evidence.

## Related specifications

- [Map Entity Editor](../entity-editor/entity-editor.md)
- [Avatar animation](../animations/animations.md)
- [Blender animation authoring/export](../animations/blender-animation-authoring.md)
- [Weapons and shared hitbox/action contracts](../weapons/weapons.md)
- [Avatar Editor](../avatar/avatar-editor.md)
- [NPC behavior](../npc-behavior/npc-behavior.md)
- [Actor presets and roles](../actor-presets/actor-presets.md)
- [Gamemodes](../gamemodes/gamemodes.md)
- [Creation, sharing, and distribution](../distribution/distribution.md)
- [World projects and live editing](../world-projects/world-projects.md)
- [Procedural Infinite World and XXITY](../procedural-infinite-world/procedural-infinite-world.md)
