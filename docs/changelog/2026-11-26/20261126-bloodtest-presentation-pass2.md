# Bloodtest presentation pass 2: lens mode, wish camera, ADS, projectiles, corpse gore

## Request

The user asked for the whole world to have chromatic aberration as one selectable
lens mode, a moving camera that matches the settled camera angle, no ADS arm
jitter with usable iron sights, longer/brighter/faster tracers and visible
bullets with dynamic light, larger bullet holes and a stronger six-tick
shockwave, and corpse gore that advances skin -> meat -> bone -> explosion.

## Change

- **Chromatic aberration mode:** added
  `presentation.bodycam.lens.chromatic_aberration_mode` with `full_world`
  (default in Bloodtest) and `background_only`. The PostFX shader receives
  `uChromaticAberrationBackgroundOnly`; `full_world` applies the shifted image
  to the complete scene, while `background_only` masks it to dark pixels. The
  two modes are exclusive and are not layered together.
- **Camera:** Bloodtest now uses `orientation_mode: mouse`. The bodycam spring
  runs on the plain first-person path even while ragdoll aim is active, so the
  rendered camera follows camera yaw/pitch rather than the physical head pitch.
- **ADS:** added a per-weapon `ads` block and parsed it in
  `WeaponViewModelConfig`; the viewmodel blends a camera/sight offset with
  `weapon.ads.state` evidence. The right-arm ADS target is locked when
  `stability_multiplier >= 30`, removing residual solver jitter. ADS FOV is
  enabled at `0.55x` over `0.18 s`.
- **Tracers/projectiles:** AK speed `520 -> 780 m/s`, Judgement
  `420 -> 650 m/s`; larger visual projectiles, stronger glow, longer trail
  history, thicker/brighter tracers. Projectile dynamic lights are now enabled
  for any configured weapon, not only grenades. Muzzle lights were brightened
  and lengthened.
- **Impacts:** Bloodtest bullet holes use `bullet_hole_scale: 0.65`; impact
  shockwaves now use start scale `1.2`, end scale `4.0`, alpha `0.5 -> 0.0`,
  lifetime `0.1 s` (6 ticks), and a larger normalized model base scale.
- **Corpse gore:** `queryCorpseHitPart` now returns the hit part and closest
  point. `RagdollModeSystem::applyCorpseHit` applies damage to the corpse body,
  reuses `HitEffects::onHit`, and logs `corpse.gore.stage`. Bloodtest's
  destroyed gore multipliers were increased for a larger red mist.
- **Weapon schema:** added `ads` to the weapon masterlist as an optional object;
  without the `required: false` fix every weapon without an `ads` block was
  rejected.

## Evidence

- `20261126_061412` generation 686 and `20261126_062048` generation 688:
  Bloodtest loadout resolved with all four weapons including
  `bloodtest_judgement`; no `config.weapon.invalid` after the masterlist fix.
- `20261126_062048`: `weapon.ads.state` reported the AK ADS config
  (`position_offset [0,0.02,-0.01]`, rate `18`) and
  `impact.surface.spawn` reported `bullet_hole_scale 0.65`,
  `shockwave_start_scale 1.2`, `shockwave_end_scale 4.0`,
  `shockwave_start_alpha 0.5`, `shockwave_end_alpha 0.0`, lifetime `0.1 s`.
- `20261126_062213` generation 688: `camera.bodycam.path` reported
  `orientation_mode=mouse`, `third_person=false`,
  `force_first_person=true`, `ragdoll_aim_active=true`, and
  `bodycam_override=true`. `camera.look.trace` reported stiffness `200`,
  damping `28`, zero rotation/position error while settled.
  `gamemode.bodycam.applied` reported
  `chromatic_aberration_mode=full_world`.
- `20261126_160721` generation 693: `agent.corpseprobe` spawned a local corpse
  and produced the complete gore ladder on the torso:
  `gore_damage=250 layer_after=1 part_health_after=66`,
  `gore_damage=500 layer_after=2 part_health_after=33`, and
  `gore_damage=1000 layer_after=3 part_health_after=0 destroyed=true`.
  In the same run, `agent.hold secondary 120` produced
  `weapon.ads.state ads_requested=true` with the configured camera/sight offset
  and a rising blend.

## Limits

- The corpse probe is a bounded dev-only terminal command
  (`agent.corpseprobe`); it intentionally drives the same corpse owner for
  evidence. Live projectile corpse hits use the same `applyCorpseHit` path but
  were not separately captured.
- The held-RMB run captured `ads_requested=true` and a rising blend, not a full
  `blend=1.0` sample or a human visual review of the sight picture.
- The camera proof used a temporary `config/debuglogger.json` camera level and
  restored it to `off` afterward.
- The full `ads` schema addition required a restart to load; the first run
  correctly reported the missing-field rejection before the optional fix.
