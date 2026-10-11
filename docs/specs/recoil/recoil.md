# MiMITA recoil specification

**Status:** design plan; implementation not yet started

**Audience:** gameplay, camera, weapon-viewmodel, animation, ragdoll, configuration, and runtime-validation owners.

**Purpose:** define recoil as a complete weapon-presentation system instead of treating it as only a camera punch. This specification is primarily driven by the Heavy Shotgun, but the architecture must work for every weapon that uses recoil.

## What this document does and does not do

This document does:

- define the desired player experience;
- define the separate recoil channels and their owners;
- define the editable configuration that should control those channels;
- identify the current caps and overwrite behavior that prevent accumulation;
- define the implementation order, diagnostics, and acceptance evidence.

This document does not:

- claim that the feature is implemented;
- choose final balance numbers without human playtesting;
- make the camera, animation, or ragdoll system authoritative for weapon damage;
- turn recoil into a network gameplay result or a client-authoritative damage decision;
- require literal real-world anti-aircraft physics.

The Heavy Shotgun is intended to feel like a fictional anti-aircraft cannon held in the player's hands. The reference is a very large projectile or shell, rifle-like muzzle velocity, an enormous report, a violent kick, and a visible recovery motion. The weapon should feel excessive and dangerous without making the game simulation unstable.

## Player-facing goal

The Heavy Shotgun should communicate all of the following immediately:

1. **A shot is a major event.** The muzzle flash, sound, camera motion, weapon motion, and arm motion must agree in time.
2. **The weapon has mass.** The gun should kick backward and upward; the player's arm and held weapon should visibly react instead of staying in the same pose.
3. **Automatic fire adds pressure.** Every shot adds another recoil impulse. Holding the trigger must not repeatedly replay one fixed maximum pose.
4. **Recovery is visible.** When firing stops, the weapon and arm gradually settle. A short burst should leave a larger recovery motion than a single shot.
5. **Aiming does not remove the weapon's force.** ADS may reduce or reshape the presentation, but it must not replace recoil with camera-only movement.
6. **The weapon remains readable.** Recoil must not permanently lock the camera, teleport the model, or make the player unable to understand where the weapon is pointing.

The intended style is closer to Counter-Strike's accumulated automatic recoil than to a single repeated animation, but heavier, louder, and more physical. Counter-Strike is the reference for repeated shot accumulation and recovery timing; the Heavy Shotgun adds the exaggerated weight, blast, arm response, and self-push of a much larger weapon.

## Current confirmed problems

The current implementation has multiple independent limits:

- `Camera::addPunch()` clamps accumulated camera punch to approximately `±10` pitch and `±6` yaw in `src/camera.cpp`.
- `WeaponFire::applyRecoil()` clamps each computed shot contribution and caps the weapon's accumulated recoil state at a small fixed maximum in `src/combat/weapon-fire-effects.cpp`.
- `WeaponSystem` decays its accumulated recoil state from a weapon custom parameter, but the camera punch itself is decayed through the camera tick using a separate default rate.
- The existing player recoil settings are loaded, but they are not the single owner of the camera-punch decay path.
- `WeaponViewModel::update()` receives the accumulated recoil value, but currently uses it mainly as a fire-event test. It does not apply that value as a persistent additive weapon transform.
- The optional viewmodel fire-animation trigger is based on whether recoil is above a fraction of itself. That condition can remain true across multiple updates, causing a fire animation to be restarted or held instead of receiving a new additive impulse per shot.
- The Heavy Shotgun reuses the shotgun pose and currently has no dedicated recoil animation definition in its weapon presentation configuration.
- The RMB/right-arm pointing path continuously solves the physical arm toward the camera/weapon target. It does not currently receive a distinct recoil impulse, so ADS can appear to show camera recoil while the arm returns to the same target pose.

These are separate problems. Removing only the camera clamp will not create physical arm recoil, and adding only an animation will not create unbounded automatic accumulation.

## Recoil model

Recoil must be represented as a set of additive states, not one value that every system interprets differently.

### Per-shot impulse

Each accepted shot produces a recoil impulse containing at least:

- upward rotation impulse;
- lateral rotation impulse or deterministic spread direction;
- backward weapon/arm translation impulse;
- optional roll impulse;
- optional player self-impulse for physical pushback.

The impulse is generated once per accepted shot by the weapon fire owner. It is not regenerated every render frame.

### Accumulated recoil state

The active weapon owns a persistent recoil state for the local presentation path. Each shot adds to it:

```text
accumulated += per_shot_impulse
```

The state must not be silently clamped to a small universal maximum. A weapon may define an explicit presentation limit, but the default must make the limit visible and intentional. The Heavy Shotgun's design target is continued accumulation through the entire eight-shell magazine when fired automatically.

The state is reset or transitioned deliberately when:

- the weapon changes;
- the player dies or respawns;
- a reload begins, if the weapon design calls for the weapon to settle during reload;
- the presentation owner is reinitialized.

Changing weapons must not carry Heavy Shotgun recoil into the next weapon.

### Recovery

Recovery is not the same as a shot impulse. It is a configurable spring or decay policy:

```text
if recovery is active:
    accumulated moves toward zero at recovery_rate
```

The configuration must support a recovery delay so automatic fire can build recoil instead of having recovery cancel every shot immediately:

```text
time_since_last_shot < recovery_delay  -> hold/build recoil
time_since_last_shot >= recovery_delay -> recover toward zero
```

The implementation may use a critically damped spring instead of linear decay, but it must preserve the same observable contract: repeated shots add, release allows recovery, and no hidden universal cap stops the buildup.

## Separate presentation channels

One recoil event must feed separate channels with separate scales and recovery behavior.

### Camera channel

The camera channel changes the player's view direction or view offset. It must:

- receive the per-shot impulse;
- accumulate according to the active weapon definition;
- have editable ADS scaling;
- avoid the current small universal `Camera::addPunch()` cap being the weapon's accumulation limit;
- preserve the camera's safety limits so the camera cannot become numerically invalid.

The camera's safety limit and a weapon's recoil accumulation limit are different concepts. A large accumulated recoil value may be retained by the weapon while the camera applies it through a controlled presentation mapping. This prevents the camera from being permanently stuck at its Euler-angle boundary while still allowing the weapon and arm to show continued recoil.

### First-person weapon/viewmodel channel

The viewmodel channel applies recoil after the base weapon attachment and ADS transform:

```text
final_viewmodel_transform = base_attachment * ADS_transform * recoil_transform
```

The recoil transform must be additive. It must not replace the base pose every tick. It should support:

- backward translation;
- upward rotation;
- optional roll and lateral rotation;
- a spring return;
- a separate scale while ADS is held.

The Heavy Shotgun should visibly move as a large, heavy object. The current shotgun pose can remain the base handling pose; recoil should be layered on top of it rather than copied into a new repeated fire pose.

### World-visible physical arm channel

The physical arm channel applies the same shot event to the local player's physical right arm and, where appropriate, the supporting left arm. This belongs in the aimbody/ragdoll presentation owner, not in the camera.

The arm recoil should be an impulse or target offset added to the arm's existing physical state:

```text
arm_target = base_aim_target + recoil_offset
arm_velocity += recoil_angular_impulse
```

The physical solver must then recover the arm toward the normal aim/animation target. It must not overwrite the recoil state by rebuilding the same ADS pose every tick.

The arm channel must respect the existing weapon attachment contract. `weaponLocalToArm`, grip, muzzle, and attachment offsets remain weapon-owned. Recoil adds a transient physical response; it does not invent a second weapon coordinate system.

### Player self-impulse channel

Self-push is distinct from visual recoil. It may push the player backward or upward for a weapon with extreme force, but it remains gameplay physics and must run at the fixed gameplay tick. The Heavy Shotgun may use a strong self-impulse, but the final amount requires playtesting because it affects movement, collision, and multiplayer behavior.

## Proposed editable configuration

The exact JSON names can be finalized during implementation, but the weapon definition needs one clearly owned recoil object rather than unrelated custom parameters spread across camera, player settings, and weapon code.

Proposed shape:

```json
"recoil": {
  "enabled": true,
  "pitch_per_shot": 1.0,
  "yaw_per_shot": 0.0,
  "roll_per_shot": 0.0,
  "accumulation_scale": 1.0,
  "max_accumulation": 0.0,
  "recovery_delay": 0.20,
  "recovery_rate": 5.0,
  "camera_scale": 1.0,
  "camera_ads_scale": 1.0,
  "viewmodel_back_per_shot": 0.0,
  "viewmodel_pitch_per_shot": 0.0,
  "viewmodel_roll_per_shot": 0.0,
  "viewmodel_recovery_rate": 5.0,
  "viewmodel_ads_scale": 1.0,
  "arm_back_per_shot": 0.0,
  "arm_pitch_per_shot": 0.0,
  "arm_roll_per_shot": 0.0,
  "arm_recovery_rate": 5.0,
  "arm_ads_scale": 1.0,
  "self_impulse": 0.0,
  "self_up_impulse": 0.0
}
```

Recommended meaning:

- `1.0` is the authored baseline for a channel, not a hidden maximum.
- `0.0` for `max_accumulation` means no weapon-level presentation cap, subject to finite-value validation and presentation safety mapping.
- Camera, viewmodel, and arm values are not interchangeable.
- Self-impulse is gameplay-affecting and must have fixed-tick/server-authority rules.
- The Heavy Shotgun should define its own values rather than inheriting the revolver or ordinary shotgun values.

The final schema must document defaults, validation ranges, hot-reload behavior, and whether a value applies immediately, on the next shot, on weapon equip, or on respawn.

## Heavy Shotgun target profile

The Heavy Shotgun's existing identity is:

- 40 pellets;
- high per-pellet damage;
- eight shells in the magazine;
- large reserve ammunition;
- automatic fire support;
- a long reload compared with ordinary weapons;
- a large muzzle flash and heavy sound;
- shotgun pose reuse as the base pose.

The recoil plan adds the missing physical identity:

- the first shot gives a clearly visible heavy kick;
- each automatic shot adds another kick before recovery can erase it;
- the viewmodel moves backward and upward as one heavy object;
- the physical arm briefly loses its perfect aim pose and then catches up;
- ADS reduces or reshapes the motion only through an explicit weapon setting;
- firing the full magazine produces a visibly larger accumulated recovery than firing one shot;
- the weapon must not freeze at one identical fire pose after the first shot.

The desired feel is “a miniature anti-aircraft cannon in the player's hands,” not “a normal shotgun with a louder sound.” Sound, flash, camera, viewmodel, arm, and self-impulse must all reinforce the same event.

## Ownership and implementation boundaries

### Weapon definition/configuration owner

`config/weapons.json` and its existing weapon-definition loader own weapon-specific recoil values. The weapon definition should expose a typed recoil structure rather than requiring callers to interpret arbitrary custom-parameter keys. In JSON this structure is named `recoil_definition`; the legacy numeric `recoil` field remains available to older weapons during migration.

### Shot event owner

The weapon fire owner creates one recoil event per accepted local shot. It passes the event to the presentation channels and to the gameplay self-impulse path where applicable.

### Camera owner

The camera owns view application and safety limits. It must not own weapon-specific accumulation policy.

### Viewmodel owner

`WeaponViewModel` owns first-person weapon transform composition. It must apply recoil after base attachment and ADS transforms, and it must not use a “recoil is nonzero” test as a substitute for a discrete shot event.

### Physical arm owner

The aimbody/ragdoll system owns physical arm state, arm impulses, solver response, and recovery toward the normal target. It must accept recoil as an additive impulse or transient target offset.

### Gameplay physics owner

Player self-impulse, collision, damage, and knockback remain fixed-60-Hz gameplay systems. Render-frame recoil must not directly change gameplay collision or damage.

## Implementation phases

### Phase 1: remove ambiguity and add typed state

1. Add a typed recoil definition with safe defaults.
2. Identify and remove duplicate interpretation of `recoil`, `recoilAccumulation`, player recoil settings, and camera punch decay.
3. Replace the current implicit small accumulation cap with an explicit weapon-level policy.
4. Track a discrete shot sequence or shot event so animation cannot retrigger merely because a recoil value remains nonzero.
5. Add bounded structured diagnostics for definition load, shot event, accumulation, recovery, and channel application.

### Phase 2: camera accumulation

1. Feed the typed weapon recoil state into the camera channel.
2. Separate weapon accumulation from camera-safe application.
3. Add ADS scaling.
4. Verify that repeated Heavy Shotgun shots produce increasing accumulated values and that the camera does not stop changing because of the old `±10`/`±6` punch cap.

### Phase 3: first-person weapon recoil

1. Compose a recoil spring after the base shotgun/ADS attachment transform.
2. Apply translation and rotation from the weapon definition.
3. Return the weapon through a configurable spring or recovery rate.
4. Confirm that the rendered weapon, muzzle position, and aim direction use the intended transform owner and do not desynchronize collision or firing origin.

### Phase 4: physical arm recoil

1. Add an additive recoil impulse or transient target offset to the physical arm state.
2. Apply it after the normal aim target is created but before the physical solver integrates the arm.
3. Let the arm recover toward the base aim/animation target.
4. Apply an explicit ADS scale rather than bypassing the arm response while aiming.
5. Confirm that the arm remains attached to the weapon's real grip/muzzle frame and that the supporting arm does not receive an accidental duplicate or mirrored impulse.

### Phase 5: Heavy Shotgun tuning

1. Tune one-shot impact first.
2. Tune a short automatic burst next.
3. Tune a full eight-shot magazine last.
4. Tune sound and muzzle flash against the same shot event timing.
5. Compare hip fire and ADS separately.
6. Keep the base shotgun pose unless a dedicated Heavy Shotgun handling pose is proven necessary.

## Diagnostics and evidence

Diagnostics must be centralized and bounded. They should include:

- weapon ID and definition generation/hash;
- shot sequence number;
- fire tick and fire mode;
- per-shot recoil impulse;
- accumulated recoil before and after the shot;
- recovery delay and recovery rate;
- camera channel applied value;
- viewmodel channel applied value;
- physical arm impulse and current arm recoil state;
- ADS state and channel multipliers;
- reset reason;
- any safety clamp or invalid-value rejection.

The log must make it possible to answer:

1. Did the shot event happen once?
2. Did accumulation increase on every shot?
3. Did recovery begin at the configured time?
4. Did the camera, weapon, and arm each receive the event?
5. Did one channel clamp or overwrite the others?

For runtime validation, use the repository's dev-loop executable and live `events.jsonl`. Build success is reported separately from configuration-load evidence, runtime evidence, and human visual acceptance.

## Acceptance criteria

The feature is not complete until all of the following are true:

- a single Heavy Shotgun shot visibly kicks the camera, weapon, and physical arm;
- two or more automatic shots produce distinct additional recoil rather than the same pose repeated;
- a full eight-shot automatic burst has a larger accumulated/recovery response than one shot;
- stopping fire allows the camera, weapon, and arm to recover;
- ADS still shows weapon and arm recoil, with only the configured ADS scaling applied;
- changing the recoil values in the active weapon configuration changes the intended channel and does not silently change unrelated channels;
- changing weapons resets or transitions recoil according to the documented policy;
- the weapon's muzzle and aim direction remain coherent during recoil;
- no gameplay damage or collision result depends on render-frame recoil;
- invalid configuration is rejected atomically with a categorized diagnostic;
- the live journal proves the intended shot, accumulation, recovery, and channel sequence;
- human playtesting confirms that the weapon feels like a massive, dangerous cannon rather than a normal shotgun with camera shake.

## Human tuning questions

These decisions remain open until playtesting:

- Should `max_accumulation: 0` mean truly unbounded presentation state, or should each channel have a very high safety ceiling?
- Should recovery begin only after the trigger is released, or after a short delay while automatic fire continues?
- How much self-push should the Heavy Shotgun produce before it harms normal movement or causes unwanted wall collisions?
- Should ADS reduce recoil, redirect it more vertically, or mostly preserve the hip-fire force?
- Should the supporting arm move with the firing arm, or should it remain a stabilizing counter-motion?
- Does the Heavy Shotgun need a dedicated handling pose after additive recoil is working, or is the existing shotgun pose sufficient?

