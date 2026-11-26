# Bloodtest

**Purpose:** define Bloodtest's repeatable TDM loop, physical-projectile combat,
body damage and gore, and bodycam presentation.
**Audience:** gamemode, movement, camera, weapon, actor-damage, effects, and
multiplayer contributors.
**Does not:** create a second movement, damage, projectile, camera, or match
lifecycle owner, or claim runtime/visual acceptance without a pinned live run.

## Purpose

Bloodtest is a slower, sightline-focused team mode and a testbed for physical
weapons, body damage, blood/viscera, and body-worn camera presentation. It uses
the shared team match, spawn, movement, projectile, actor-damage, camera, and
effects owners. Retrograd is an inspiration for articulated bodies and physical
reactions, not a promise to reproduce another game's internals.

## Mode contract

- Players use shared multiplayer spawning, respawning, inventory, damage,
  projectile, and match lifecycle systems.
- RED and BLUE each have a configurable roster capacity, initially 25 actors
  per team. Players and any NPC roster fillers count toward that capacity.
- An active match lasts 180 seconds or until a team reaches 50 kills. If neither
  team reaches 50 when time expires, the result is a stalemate regardless of
  which team has the higher score. Actor respawns remain enabled during ACTIVE.
- The repeat loop uses a three-second JSON-configured intermission, ACTIVE, a
  JSON-configured results display (initial duration: five seconds), and another
  intermission. The three-second intermission is the only pre-match wait; there
  is no second countdown unless a future mode explicitly selects one.
- Results show the winner or stalemate and both team scores. The server repeats
  this lifecycle indefinitely while Bloodtest remains selected.
- Bloodtest uses the existing TDM team spawn and safe-fallback rules. Its map
  list begins with the maps in the TDM rotation. Any additional compatible map
  must provide valid RED/BLUE spawn groups or pass the shared validated fallback
  spawn rules; the mode must not depend on a map-specific code path.
- Players and NPCs share the mode's weapon set. Every Bloodtest firearm is a
  physical ballistic projectile with finite travel time and bullet drop;
  hitscan, instant beams, and hitscan fallback damage are forbidden. The
  planned roster is AK-47, M4A1, FAMAS, Desert Eagle, Judgment revolver,
  Glock 17, sawed-off double-barrel, AA-12, Uzi, and the existing projectile
  shotgun. Individual specs define their distinct roles. The user has not yet
  selected which subset ships in the first playable slice; the mode loadout
  must allow a smaller staged roster without changing projectile authority.
- The mode uses the `config/movement/movement-slowdm.json` movement profile.
  The file's current internal preset name is `heavy`; the runtime mapping must
  resolve this requested file unambiguously and give the Bloodtest profile a
  stable mode-facing identifier. Sprint enablement, speed multiplier, and
  stamina drain/recovery are mode-owned, JSON-configurable values.
- The mode forces first person and the `competitive_no_headless` avatar group
  for players and NPCs. The group is authoritative at spawn and presentation.
- The mode selects the bodycam camera profile and physical/hybrid weapon aim.
  Mouse-look target, body/head orientation, arms, and held weapon ease smoothly
  toward the requested facing. Each response is independently configurable in
  live JSON, bounded, and time-step aware. Visual smoothing must not redirect
  authoritative weapon shots or smooth fixed-tick collision/damage state.
- The mode forces first person and starts at a JSON-configured 100-degree FOV.
  Screenshot review may tune the value after the first prototype.
- Crosshair settings are mode-owned: thin red arms and alpha 200/255 as the
  initial visual target. Crosshair style is a HUD setting, not a wall-visibility
  aid.
- Player outlines, friendly outlines, enemy/friendly health bars, damage
  numbers, and hit markers are disabled. Actor hits are communicated through
  visible blood/body feedback; world hits use the world-impact presentation
  below.
- World geometry destruction is forced off for Bloodtest. Ordinary collision
  and projectile impacts remain active. The cause of reported floor
  fall-through is not attributed to destructible geometry or capsule collision
  until a pinned reproduction identifies the first failed collision stage; see
  `issue-2e7b0cd6f868`.
- `features.destructible_body_parts: true` enables shared player/NPC body-part
  health and destruction for this testing mode. Head/torso destruction is lethal.
- NPCs use a team-aware sightline behavior profile. They may travel to and hold
  useful angles, but may aim/fire only when the shared perception owner confirms
  a visible target. They do not track or shoot through walls. Health and damage
  remain controlled by normal role and weapon owners.
- NPCs respawn on the first eligible fixed simulation tick after death through
  the shared respawn lifecycle. Bloodtest's `respawn_seconds: 0.01` is the
  minimum nonzero mode respawn timer; it is not a zero-delay same-tick revive.
- Bodycam settings and Bloodtest weapon overrides are mode-owned JSON. Valid
  live updates affect the next applicable camera render, fixed-tick aim step, or
  weapon action without restarting the match or resetting team score, actor
  life, ammunition, reload state, or weapon cooldown. Field-specific migration
  rules are required for values that change existing weapon state, such as
  magazine capacity.
- Blood presentation and client blood feedback are enabled through the existing
  presentation policy.

## Weapons

- Every rifle, pistol, and shotgun round is a physical ballistic projectile
  with finite travel time, gravity drop, and authoritative swept world/actor
  collision at the fixed tick. No Bloodtest firearm uses hitscan, instant-beam,
  or hitscan fallback damage.
- Individual weapon contracts define role, fire mode, projectile count, recoil,
  reload, sound, and magazine values. The initial live roster may be smaller
  than the planned list; the selected subset remains a mode-owned weapon set.
- Automatic fire may batch projectile transport records and reuse pooled
  fixed-tick simulation work, but every round keeps an independent trajectory,
  collision, impact, damage event, and lifetime. Clients may predict projectiles
  but cannot author damage. See the shared projectile-firearm networking spec.
- Each weapon has JSON-configurable unequip and equip durations. A weapon is no
  longer fireable once unequipping begins; the incoming weapon becomes fireable
  after equipping completes. Mode overrides may tune these timings live without
  resetting ammunition, reload, cooldown, or match state.
- Firing and impact must not spawn rocket/grenade-style explosion effects. On
  world contact, reuse the shared impact/decal owners to place a material-aware
  bullet-hole mark and emit a short, pooled debris burst. The first pass may use
  a 2D debris sprite; small 3D debris is optional if the shared system already
  supports it within the same fixed-tick and performance limits.
- Mode weapon overrides cover the weapon definition's gameplay and presentation
  fields, with the master weapon schema defining valid fields. Override
  resolution is a deep merge by weapon ID and is applied atomically to players
  and NPCs. It must not silently ignore fields or reset live weapon state when
  JSON reloads.
- Victim and shooter knockback are zero for these guns. The weapon impact still
  produces its specified world/body hit and gore response; any physical body
  impulse from the shared projectile contact path must be separately accounted
  for so it does not reintroduce unintended gameplay knockback.
- The Uzi starts at 30 rounds loaded and 180 reserve rounds. Its requested
  target empties a magazine in about 0.5 seconds; measure the cadence and server
  cost before treating that target as accepted.
- The Judgment starts with six rounds. The Glock 17 starts with 20 loaded and
  100 reserve. Other weapon capacities and final weapon ordering remain
  per-weapon balance decisions.
- The initial reload behavior uses the existing reload system. The proposed
  staged T/R cylinder and magazine procedures are follow-on work and require a
  separate per-weapon state-machine contract before implementation.

### Bodycam and hit presentation

- The first-person view is driven by the simulated head pose (`head_world`) and
  shows the actor's arms and weapon. Mouse look supplies the target pose; the
  head, arms, and physically aimed weapon settle toward it using separate
  JSON-tunable response/smoothing values.
- Camera/arm smoothing is presentation and control response. It must not alter
  the 60 Hz authoritative movement/contact simulation or make the weapon fire
  along a direction different from its authoritative physical muzzle.
- Fall damage is mode-configurable and evaluated from a landing event at the
  fixed simulation tick. It scales from vertical impact speed above a configured
  safe threshold; ordinary steps, walkable slopes, and authored stair routes do
  not count as damaging falls. Initial tuning targets are: no damage below a
  five-meter fall; about 10 damage for a six-meter fall landing at 30 m/s; and
  about 90 damage for a twenty-meter fall landing at 60 m/s. These are
  provisional test points, not a validated curve or instant-kill rule; actual
  landing speed governs damage and must be measured and tuned.
- Actor hit presentation uses the shared server-confirmed impact position,
  direction, body part, and damage result. Clients render the blood spray and
  body-layer/part changes immediately from that event, without adding outlines,
  health bars, damage numbers, or hit markers as confirmation.

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
preset plus role-owned health/stamina values. Bloodtest selects the dedicated
`slowdm` profile sourced from `config/movement/movement-slowdm.json`; it must
not silently select the separately named `bloodtest`, `heavy`, or
`deliberate` profiles. The source file currently declares the name `heavy`, so
implementation must establish an unambiguous stable `slowdm` identifier while
keeping the requested tuning values. The profile provides Bloodtest's slower
ground pace, configurable jump behavior, sprint enablement and
acceleration/deceleration ramp, plus editable stamina capacity, drain, and
recovery. Its actual speed and jump values must be compared with the standard
TDM profile in a matched movement scenario before numerical acceptance is set.
This explicit Bloodtest profile is a scoped exception to the generic
no-timer-gating movement rule: only its authored jump-repeat delay may gate a
fresh jump edge. A press during the delay is discarded; it is not buffered and
holding jump does not auto-repeat. Sprint input ramps speed smoothly, and
releasing sprint ramps back to the role's normal target.

The data contract is: movement preset owns the movement kernel and base
quantities (`max_speed`, `jump_velocity`, acceleration, and friction). The role
may supply `movement_overrides` for `jump_repeat_delay_ticks`,
`sprint_speed_multiplier`, `sprint_ramp_up_ticks`, and
`sprint_ramp_down_ticks`; it also owns `max_health` and a `stamina` object with
`capacity`, `drain_per_second`, and `recovery_per_second`. When the role
resolves its movement preset, these values are passed to the same shared
player/NPC movement step. There is no separate monster or juggernaut movement
function. The authoring UI labels units and rejects negative durations/rates
and nonpositive capacities. The initial `slowdm` profile is the Bloodtest
baseline; role variants can select other existing presets for normal,
juggernaut, or monster speed/jump scale.

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

## 2026-11-24 vision update

These decisions refine the existing contract; they do not replace the shared
movement, projectile, damage, camera, or match owners.

- **Roster:** the first playable slice is AK-47, Judgment revolver, and the bat.
  Other listed weapons remain planned.
- **Capacity:** 25v25 is the target, configurable, with `npc_count` 0–25. Load
  validation starts at 10v10 and scales only after the server holds 60 Hz.
- **Sandbox:** a separate `bloodtest_sandbox` mode spawns free-roaming NPCs with
  a `passive`/`aggressive`/`mixed` toggle, instant respawn, and no score.
- **Respawn:** mode 2 (instant) is the default. Mode 1 (timer) is retained for
  compatibility. Mode 3 lets the downed player shoot a highlighted respawn
  target to return immediately; no timer is required. No mode forces dead air.
- **AK-47:** 65 damage at 0–20 m, 55 at 50 m, 45 at 80 m+, 3× head. Two body
  shots kill at 50 m. 30-round magazine, 9999 reserve.
- **Judgment:** 1-shot head, 2-shot body, 3× head, 9999 reserve.
- **Bat:** mace-like always-active capsule; incidental 1–5 damage scaled by
  run-in speed; active window 60–120; white/red active tint; windup, active,
  recovery, and return phases.
- **Gore layers:** use `assets/entity/player/default/bodylayers/mimita-meat-v3.glb`
  and `mimita-bone-v1.glb`, matched by part node name. Meat uses a blood texture
  and red tint; bone uses a pale tint. Materials may be refined later.
- **Local damage:** each part stores recent hit rays; the skin/meat fragments
  inside the ray radius are discarded to expose the next layer. High force is
  required to punch through bone.
- **Organs:** high-force torso hits may emit 1–3 colored organ primitives
  (liver/heart/lung-like) plus a dark cavity and rib hints. No fluid simulation.
- **Blood:** every hit may place wall blood behind the victim, scaled by force
  and direction. Decals fade after 30 seconds with hard caps.
- **World impact:** bullets use a small bullet hole plus an optional surface
  shockwave model or primitive. The rocket explosion mark is not used for
  bullets. Profiles are per weapon and JSON-owned.
- **Camera:** strong but aimable bodycam; two JSON presets (`aimable`,
  `horror`); head lean from movement; recoil from shot impulse. Fisheye mode 1
  affects the world only; mode 2 (world + UI) is specified but deferred.
- **Audio:** AK uses `akfire` / `akreload` / `akequip`; Judgment uses
  `judgementfire` / `judgementreload` / `judgementequip`. 3D hurt, death, and
  limb-destruction sounds are required.
- **Evidence:** `combat.hit.resolved`, `impact.surface.spawn/draw`,
  `impact.decal.spawn/render`, `actor.replica.trace`,
  `match.phase.changed`, `server.capacity.window`, `camera.look.trace`, and
  `weapon.reload.mode.changed` are the acceptance stream in the per-instance
  `events.jsonl`.

## 2026-11-25 presentation and gore lock

- **Mode 6 HUD:** fullscreen mode 6 composes the world, game GUI, HUD, and
  developer console in the 1024x768 scene target and stretches the whole frame
  together. A crisp/output-resolution HUD is an explicit opt-in mode, disabled
  by default. Only one HUD path may draw per frame; the console must not be
  drawn once in-scene and again after PostFX.
- **Proximity fade:** every mode inherits a master `presentation.proximity_fade`
  value of `inherit|on|off`. Bloodtest is `off`. The held weapon never inherits
  a proximity-fade discard from the player/world pass.
- **First person:** the local client hides its head and all attached cosmetics
  for first-person views. Remote clients always see the normal head and
  cosmetics. This is local presentation only and is not proximity fade.
- **Weapon origin:** Bloodtest keeps the physical muzzle as the authoritative
  bullet origin. A camera-origin mode may exist for other modes, but it is not
  the Bloodtest default. Weapon size, grip, stock, arm placement, and pose are
  the alignment controls.
- **Weapon pose:** the AK uses the `rifle` pose (right hand on the grip, left
  hand on the handguard, stock at the shoulder), not the revolver pose. Pose
  values live in `config/animations.json` and are hot-reloadable.
- **Gore mode 2:** actor life HP is 100 for lethality; each body part has a
  separate 1000-point gore health for layers. A part exposes meat at 250
  cumulative gore damage, bone at 500, and explodes at 1000. Mode 1 remains the
  current 100-point part model. An AK headshot can kill through actor HP while
  showing little or no meat/bone; a point-blank grenade at 2500 damage
  explodes the body.
- **Gore scope:** Bloodtest uses the maximum blood/gore profile. Modes without
  an explicit profile inherit the default minimal profile. Reuse the existing
  `blood_scale`, `blood_lifetime_scale`, impact-decal, and hit-effect owners;
  do not add a second gore system.
- **Camera:** mouse look stores a wish direction. A critically damped spring
  advances it at the fixed tick and the render interpolates the spring state.
  The spring stiffness, damping, maximum angular speed/acceleration, settle
  deadband, head amplitude, arm response, weapon stability, and force
  multipliers are JSON-tunable. The 0.01 s feel must not be implemented as a
  per-frame snap that jitters.
- **Evidence:** acceptance uses bounded `events.jsonl` records plus deterministic
  pixel probes. Required probes include opaque-black quad readback, held-weapon
  alpha/coverage, console/HUD draw count and stretch ratio, camera wish/current
  jitter, meat/bone AABB alignment, and corpse-hit position. A human visual
  review remains separate.

### 2026-11-25 runtime evidence

- `20261125_221051` (generation 651, mode 6): `ui.scene.draw_path` reported
  `whole_frame=true`, `scene_layer=true`, `hud_in_scene=true`,
  `postfx_hud=false`, and `console_draw_count=1`.
  `gamemode.presentation.policy` reported `proximity_fade=off`,
  `first_person_hide_cosmetics=true`, and `gore_mode=2`.
  `first_person.head_hidden` reported the local head and cosmetics hidden.
- `20261125_222447` (generation 654): `camera.bodycam.path` reported
  `third_person=false`, `force_first_person=true`, `ragdoll_aim_active=true`,
  `orientation_mode=head_world`, and `bodycam_override=true`.
  `camera.look.trace` reported `stiffness=200`, `damping=28`, and a settled
  sub-degree rotation error. The camera category was enabled only for this
  proof and restored afterward.
- `20261125_222546` (generation 654): `server.damage.applied` reported
  `gore_mode=2` with independent actor health and per-part gore accumulation
  (for example actor health `96->93` while head gore reached `3`). The
  250/500/1000 layer transitions still need a deterministic high-damage probe;
  see `issue-235e26db08aa`.

## 2026-11-26 presentation pass 2

- **Lens modes:** `presentation.bodycam.lens.chromatic_aberration_mode` is
  exclusive. `full_world` applies chromatic aberration to the complete final
  scene; `background_only` masks the shifted image to dark/background pixels.
  Bloodtest defaults to `full_world`. Do not composite a normal world in front
  of a separate CA ghost world.
- **Camera:** Bloodtest uses `orientation_mode: mouse`. The bodycam spring
  follows the camera wish yaw/pitch, while the head/eye supplies position only.
  Moving and settled camera angles therefore use the same aim direction.
- **ADS:** per-weapon `ads` maps a camera-aligned sight offset, blend rate, and
  optional FOV. While RMB is held, the right-arm ADS target is locked against
  solver jitter; authoritative muzzle and damage behavior are unchanged.
- **Projectiles:** AK and Judgement use faster authoritative projectile speeds,
  larger visible projectiles, stronger glow/trails, and per-projectile dynamic
  light. Tracer thickness, lifetime, and alpha are per-weapon JSON.
- **Impacts:** Bloodtest bullet holes use a larger per-weapon scale. Bullet
  shockwaves use start alpha 0.5, end alpha 0.0, and 0.1 s (six ticks) with a
  larger start/end scale.
- **Corpse gore:** corpse hits use the same gore mode 2 part pool and
  `HitEffects` owner as living actors. `applyCorpseHit` advances skin -> meat ->
  bone -> destroyed and spawns the destroyed mist. `agent.corpseprobe` in
  `20261126_160721` observed `gore_damage=250 layer=1`,
  `gore_damage=500 layer=2`, and `gore_damage=1000 layer=3 destroyed`.
- **ADS evidence:** the same run's `agent.hold secondary 120` observed
  `weapon.ads.state ads_requested=true` and a rising blend. A human visual
  review of the final sight picture and a full blend=1.0 sample remain useful.

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

## Related specifications

- [Team Deathmatch](teamdeathmatch.md)
- [Bodycam presentation](bodycam.md)
- [Gamemode runtime and JSON schema](gamemodes.md)
- [Weapons](../weapons/weapons.md)
- [Movement](../movement/movement.md)
- [NPC behavior](../npc-behavior/npc-behavior.md)
- [Destructible world](../destructible-world/destructible-world.md)
