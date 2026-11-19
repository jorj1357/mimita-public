# Bloodtest

**Status:** Shared actor body-part damage, replication, procedural skin/meat/bone
layers, part destruction, and force-scaled blood presentation are implemented.
Runtime and visual acceptance remain open until exercised in a real multiplayer
run. The body-rig editor, living capsule-profile editor, Bloodtest sprint/stamina
controls, and repeatable projectile test bench described below are target
features; this status does not claim they exist.

## Purpose

Bloodtest is a close-quarters combat sandbox for testing direct weapon contact,
movement, blood presentation, and destructible geometry. It uses the shared
gamemode lifecycle and free-for-all damage rules. The first configured arena is
`mimita-duels-map-v3`; world destruction persists across matches and clears on
map reload. The requested Retrograd-inspired direction is articulated bodies
and expressive physical reactions, paired with a deliberate living capsule
movement profile; it is a MiMITA implementation target rather than a promise
to reproduce another game's internals.

## Mode contract

- Players use shared multiplayer spawning, respawning, inventory, damage,
  projectile, and match lifecycle systems.
- Players can damage one another. The mode does not use teams, a score target,
  or a match timer.
- `features.destructible_body_parts: true` enables shared player/NPC body-part
  health and destruction for this testing mode. Head/torso destruction is lethal.
- The mode uses the shared `deliberate` movement preset as its slow baseline;
  Bloodtest role data adds its jump-repeat delay, sprint ramp, and stamina
  settings. The mode also selects Bloodtest-specific projectile revolver and
  shotgun definitions, plus Swordsword.
- NPCs use the shared `beginner` combat behavior profile. Their health and
  damage remain controlled by the normal role and weapon owners.
- NPCs respawn on the first eligible fixed simulation tick after death through
  the shared respawn lifecycle. Bloodtest's `respawn_seconds: 0.01` is the
  minimum nonzero mode respawn timer; it is not a zero-delay same-tick revive.
- Map destruction is enabled and uses reset policy `never`, so a match boundary
  does not restore the map. A map reload uses the shared world reset owner.
- Blood presentation and client blood feedback are enabled through the existing
  presentation policy.

## Weapons

- The revolver fires one physical sphere from its actual muzzle position.
- The shotgun fires fifteen independently simulated, deterministic physical
  sphere pellets from the same muzzle position, using its configured spread.
- Both use the shared authoritative projectile and contact paths. Their
  configured mass and speed determine physical world impact; direct actor
  damage scales with impact energy relative to the weapon's configured impact
  profile.
- Swordsword remains the shared always-active physical blade contact weapon;
  contact follows its blade shape and does not require an attack press.

## Actor damage layers

Each visible body part has its own authoritative health in the shared actor body
state. Players and NPCs use the same six parts: head, torso, left arm, right arm,
left leg, and right leg. Each part starts at 100 health.

- Above two thirds health the part shows skin.
- At or below two thirds health the part exposes meat.
- At or below one third health the part exposes bone.
- At zero health the part is destroyed and disappears from rendering and hit
  validation.
- Head or torso destruction kills the actor.
- Arm or leg destruction removes that part while the actor may continue;
  destroyed parts remain gone on the corpse. A destroyed part is not left as a
  persistent physical limb.
- Part health resets on a new life and is replicated to every client through the
  shared snapshot path. Confirmed-damage events identify the damaged part and
  the resulting part state.
- Skin, meat, and bone are procedural nested shells generated from the existing
  part mesh; no separate authored layer geometry is required.

Blood and gore presentation scale with impact force and damage. Destroying a part
produces a larger spray, and surface splatter places larger decals near the
impact center with smaller decals toward the cone edges.

## Authored body, collision, and role movement

Bloodtest is also the sandbox for authoring and replaying body damage. The
creator workflow uses the shared actor/avatar rig data and body-part damage
owner; it must not create a second damage model. Each actor definition exposes
editable body dimensions, part transforms, mass, and named skeleton joints.
Skin, meat, and bone are render/damage layers bound to those same named parts.
The editor previews the skeleton and each damage layer independently, and a
saved body revision is the one used by the live test.

World movement collision is configured separately from damage hit geometry.
For the Bloodtest capsule locomotion profile, a creator chooses either one
whole-body capsule or a small set of torso/head/leg capsules. These capsules
drive world contact and the actor's shared movement state; they do not replace
the visible articulated body, skeleton joints, per-part damage traces, or
ragdoll body after death. The selected collision proxy is shown as an overlay
in the editor and is swept through the normal cached world broadphase at the
fixed 60 Hz gameplay tick. Ragdoll mode continues to use its articulated
physical-body collision rules; the capsule profile is a living-actor movement
mode, not a claim that a ragdoll is one capsule.

All roles use the same shared movement implementation and choose a movement
preset plus role-owned health/stamina values. The initial Bloodtest role uses
the existing `deliberate` preset and its 1 m/s base walk speed. The current
`bloodtest` preset is not the target: it currently configures
`max_speed: 12.0`, `jump_velocity: 16.0`, and `jump_buffer_time: 0.2`. The
initial Bloodtest role is a
deliberate, slower preset with a weaker jump, configurable grounded jump repeat
delay, sprint acceleration/deceleration ramp, and editable stamina capacity,
drain, and recovery. This explicit Bloodtest profile is a scoped exception to
the generic no-timer-gating movement rule: only its authored jump-repeat delay
may gate a fresh jump edge. A press during the delay is discarded; it is not
buffered and holding jump does not auto-repeat. Sprint input ramps speed
smoothly, and releasing sprint ramps back to the role's normal target.

The data contract is: movement preset owns the movement kernel and base
quantities (`max_speed`, `jump_velocity`, acceleration, and friction). The role
may supply `movement_overrides` for `jump_repeat_delay_ticks`,
`sprint_speed_multiplier`, `sprint_ramp_up_ticks`, and
`sprint_ramp_down_ticks`; it also owns `max_health` and a `stamina` object with
`capacity`, `drain_per_second`, and `recovery_per_second`. When the role
resolves its movement preset, these values are passed to the same shared
player/NPC movement step. There is no separate monster or juggernaut movement
function. The authoring UI labels units and rejects negative durations/rates
and nonpositive capacities. The initial `deliberate` preset is the baseline;
role variants can select other existing presets for normal, juggernaut, or
monster speed/jump scale.

This shared profile family must support, at minimum, a normal fighter (100 HP,
normal speed), juggernaut (2,000 HP, very slow), and monster (1,000 HP, faster
with a stronger jump). These are role/preset data combinations, not separate
movement code paths. Use existing normal, `juggernaut`, and
`source_monster` movement presets as starting profiles where their measured
values fit those labels; the creator can tune the data without changing the
movement algorithm.

## Editor destruction and blood-effects test bench

In Map Editor Play Test, a creator can select a reusable projectile/object
definition, set its shape, dimensions, mass, initial velocity, impact material,
and launch point, then fire it at a live NPC or ragdoll. The test uses the same
fixed-tick projectile, collision, part-damage, and blood presentation owners as
normal play. It supports repeated shots against a resettable test actor and
replays a saved launch recipe so effect tuning is comparable. Object appearance
and physical shape can be authored independently of whether the launched
object is called a bullet. This is a local test tool; it cannot write damage
into saved map state or change authoritative combat rules.

The editor may reuse the shared animation timeline for body pose, impact pose,
and effect-timing preview. It shows per-tick projectile path, contact, damaged
part, body-layer transition, and blood-effect samples on one timeline. Scrubbing
is preview-only; applying a shot occurs only through the live local server test.
Reset restores the actor and test-scene state before another replay.

### Required Bloodtest runtime evidence

Emit these through the canonical per-process `events.jsonl` after the operation
has taken effect. High-frequency samples are bounded to a named actor and test
capture; ordinary gameplay records transitions only.

| Event | Required evidence |
|---|---|
| `bloodtest.body.definition.loaded` | `actor_id`, `role_id`, `body_revision`, `part_count`, `joint_count`, `part_dimensions_hash`, `mass_kg`, `valid:true` |
| `bloodtest.collision.profile.applied` | `actor_id`, `role_id`, `profile_id`, `proxy_kind`, `capsule_count`, `capsule_dimensions`, `fixed_tick:true` |
| `bloodtest.movement.profile.applied` | `actor_id`, `role_id`, `movement_preset`, `speed_mps`, `jump_impulse`, `jump_repeat_delay_ticks`, `sprint_ramp_ticks`, `stamina_capacity`, `stamina_drain_per_second`, `stamina_recovery_per_second` |
| `bloodtest.sprint.state` | `actor_id`, `tick`, `input_down`, `speed_target_mps`, `speed_current_mps`, `stamina_before`, `stamina_after` |
| `bloodtest.test-projectile.launched` | `capture_id`, `object_id`, `shape`, `dimensions`, `mass_kg`, `velocity_mps`, `target_id`, `replay_recipe_id` |
| `bloodtest.test-projectile.contact` | `capture_id`, `object_id`, `target_id`, `part_id`, `tick`, `impact_speed_mps`, `damage`, `body_revision` |
| `bloodtest.body-layer.changed` | `actor_id`, `part_id`, `health_before`, `health_after`, `layer_before`, `layer_after`, `tick` |
| `bloodtest.blood-effect.applied` | `capture_id`, `actor_id`, `part_id`, `impact_position`, `impact_force`, `effect_id`, `decal_count`, `result` |
| `bloodtest.test-projectile.replayed` | `capture_id`, `replay_recipe_id`, `shot_index`, `same_object_definition:true`, `same_launch_recipe:true`, `result` |

These events prove data resolution and simulation path only. Acceptance also
requires visual review of the collision overlay and body transitions, and a
human reviewer must watch repeated projectile trials to confirm the blood
response is visible and responds consistently to recorded impact. Multiplayer
and replication remain separate acceptance scenarios.

## Acceptance

1. Multiple human clients can join the arena and damage one another.
2. Point-blank muzzle contact is resolved by the physical projectile sweep.
3. Revolver shots and all shotgun pellets originate at the muzzle; pellets use
   deterministic spread and independent fixed-tick world/actor collision.
4. Faster impacts from the same configured weapon produce greater direct damage
   and stronger world impact inputs.
5. Swordsword damage follows its visible blade on any physical contact.
6. Arena destruction persists across match boundaries and clears after a map
   reload.
7. Blood presentation is visibly enabled in a real run.
8. Bloodtest NPCs resolve the `beginner` combat profile and respawn on the
   first eligible fixed simulation tick after death.
9. Part health crosses the meat, bone, and destroyed thresholds in a real run;
   head/torso destruction kills and limb destruction removes that part.
10. Both clients observe the same part health and visible layer.
11. Stronger impacts produce more and larger blood presentation than weak
    impacts, with splatter shrinking toward the cone edges.
12. A creator changes body dimensions and joint layout, saves, reloads, and
    sees the same revision in editor preview and the live test actor.
13. Each role uses the same movement path while health, speed, jump, sprint
    ramp, stamina, and living collision capsule differ as configured.
14. Repeating the same saved projectile recipe produces correlated contact,
    part transition, and blood-effect records and a visually reviewable result.

Body-layer exposure, part destruction, replicated part state, and force-scaled
blood remain runtime and human-review acceptance items. Build and configuration
evidence do not prove runtime, visual, or multiplayer acceptance.
