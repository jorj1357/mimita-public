# Bodycam Presentation

**Purpose:** define a reusable, physically attached body-worn camera
presentation profile for Bloodtest and other compatible modes.
**Audience:** camera, movement, weapon, audio, effects, and gamemode
contributors.
**Does not:** define a separate combat mode, replace shared camera ownership,
or claim documentary accuracy.

## Presentation goal

The view should feel attached to the actor's head and upper body rather than a
free camera placed at a fixed eye-height offset. The body and hands respond to
mouse look with a controlled physical delay; the view stays readable and
responsive rather than adding effects that hide threats or make input feel
delayed.

The user-provided visual references (2026-11-23) show body-mounted game views
with visible arms/weapons, broad field of view, dark interior exposure, lens
fringing at the image edges, and strong but bounded peripheral shading. They
are visual direction only. Player controls, streaming-service controls,
watermarks, dates, and branding in the screenshots are not target game UI.

The profile may select slower movement tuning, physical weapon aiming, and
Bloodtest's body-hit presentation. These are mode rules and shared system
settings, not camera-only effects.

## Shared behavior

- Use the shared replay-aware camera and effects paths so recordings and
  exports reproduce the same presentation.
- In a bodycam mode, select `head_world` so the rendered camera orientation
  follows the simulated head pose. The mode must activate this path during
  normal alive first-person play; camera pose must not depend on the actor being
  dead or in a separate ragdoll-only mode. Mouse look remains the desired head
  direction. The camera position is attached to an authored eye point on the
  head, with a configurable local offset.
- Keep physical head, arms, and weapon linked to that same desired look. Expose
  distinct live JSON tuning for head response, arm follow, weapon stability,
  camera position/rotation smoothing, and camera sway. Values must be bounded
  and validated. Smoothing must be time-step aware and must never snap when a
  valid value changes.
- Body-led look is the Bloodtest default: mouse look is the torso wish
  direction, the torso turns toward it, and the head follows the torso. The
  camera stays attached to the head, so the player sees the body lead and the
  head settle rather than an independently mouse-driven head that drags the
  torso. `head_follows_body` selects this behavior; when false the previous
  head-led response remains available for other modes.
- A small `settle_deadband_degrees` suppresses aim-motor torque and bleeds
  angular velocity when the body is already within that angle of its target.
  This removes standing-still jitter without changing large turns or movement.
- The HUD is drawn at output resolution in modes 0-4 and stays crisp. In
  full-frame stretched modes 5/6 the HUD is composed into the 1024x768 scene
  frame and stretched with the world, as required by
  `docs/specs/graphics/graphics-resolution.md`; that is deliberate and must not
  be used to hide a camera bug.
- Separate local presentation smoothing from multiplayer state. Interpolate
  replicated remote actor poses using the shared snapshot path, then render the
  camera/arms smoothly from those poses. Do not smooth or rewrite authoritative
  simulation state, server hitboxes, or the physical muzzle direction used to
  validate a shot. Avoid applying the same interpolation twice.
- Use a mode-owned 70-degree field of view as the initial Bloodtest value, with
  live JSON tuning for FOV and lens settings. Add
  restrained radial chromatic aberration at the image edges and adjustable
  lens shading/distortion. Keep central detail sharp and avoid broad black
  borders, hard circular masks, excessive blur, or distortion that hides a
  target. The final settings are tuned against in-game screenshots, not guessed
  from configuration numbers alone.
- Keep arms and the equipped weapon visible from the body-mounted view without
  allowing the head or camera to cover the sights. The weapon's visible muzzle
  must align with the physical projectile direction. An optional timestamp or
  camera label is a separate presentation choice; it is not required by the
  current reference set.
- `proximityFade` is a near-surface rendering fade. It is not camera/world
  collision and does not grant visibility through walls. Preserve ordinary
  world occlusion and verify wall edges, doorways, floors, and close cover from
  first person so the camera cannot peek around or through them.
- Put the bodycam camera, lens, aim-body, and smoothing overrides in the
  gamemode JSON schema and consume them through the shared camera/aim owners.
  Live JSON reload updates the running mode without resetting match state,
  weapon state, or authoritative actor transforms. The global
  `config/camconfig.json`, `config/aimbody.json`, and `config/postfx.json`
  remain defaults; mode data supplies a validated overlay rather than a second
  camera implementation.
- Let the mode choose its weapon collision policy independently from physical
  aim. The initial tactical profile keeps physical aim available; contact
  behavior must be deliberately selected and playtested.
- For Bloodtest, visible player-hit feedback comes from the shared blood and
  body-part effects. Do not use player outlines, health bars, damage numbers,
  or hit markers. World impacts use bullet-hole decals and a short debris
  burst, not rocket/grenade explosion visuals.
- Use shared accessibility settings for motion reduction, flashes, contrast,
  text size, audio levels, and visual injury intensity.
- Keep gameplay collision, weapon damage, and movement at the fixed tick.
  Camera shake and visual effects do not apply gameplay forces.

## Bloodtest live-tuning contract

Bloodtest's camera overrides live in `config/gamemodes/bloodtest.json` so the
player can start the mode, edit one mode-owned file, save, and see the next
validated camera/aim presentation revision without changing global defaults.
Keep the existing top-level `camera_fov` and `force_first_person` fields as the
single owners of FOV and forced perspective. Add the additional settings under
the existing `presentation.bodycam` block rather than duplicating those two
values or editing `config/camconfig.json`, `config/aimbody.json`, or
`config/postfx.json` for Bloodtest-only tuning.

The gamemode masterlist defines the complete nested schema, field order,
defaults/inherit markers, bounds, and units. The canonical Bloodtest JSON
contains the complete block. An initial shape is:

```json
{
  "camera_fov": 70.0,
  "force_first_person": true,
  "presentation": {
    "bodycam": {
      "enabled": true,
      "orientation_mode": "head_world",
      "head_eye_offset_m": [0.0, 0.0, 0.0],
      "position_response_seconds": 0.12,
      "rotation_response_seconds": 0.10,
      "head_response_seconds": 0.10,
      "torso_response_seconds": 0.22,
      "arm_response_seconds": 0.14,
      "weapon_stability_seconds": 0.10,
      "head_follows_body": true,
      "settle_deadband_degrees": 0.35,
      "camera_sway_scale": 1.0,
      "lens": {
        "chromatic_aberration": 0.0,
        "vignette": 0.0,
        "distortion": 0.0
      },
      "wall_collision": {
        "enabled": true,
        "probe_radius_m": 0.12
      }
    }
  }
}
```

This illustrates ownership and units, not final response or lens tuning. Use
named response-time constants rather than unexplained frame-dependent lerp
factors. The resolved values must be clamped to documented ranges and updated
without snapping the current camera/head/arm state. Smoothing is split by
camera position, camera rotation, head, torso, arms, and held weapon so each
can be adjusted independently. Physical aim continues to use the shared
aim-body owner; its weapon muzzle and damage contacts remain authoritative.

## Quick runtime check

Run `python devscripts/bloodtest-quick.py` from the repository root. It requests
launch mode 51 through the root dev loop, starts the second client, assigns the
two teams, and waits for `network.mode.start` with `stage=active`. It does not
start a second dev loop or bypass the build authority; if the published build is
stale it waits for the root loop to finish the requested build. Use
`--replace` only when the existing run is yours and can be stopped.

## Resolution and live edit workflow

1. Load shared defaults once from `camconfig.json`, `aimbody.json`, and
   `postfx.json`. Resolve the selected gamemode's explicit values over those
   defaults; absent or `null` fields inherit. The mode override is a temporary
   runtime overlay and must never write into a shared config singleton's base
   values or another mode's file.
2. Extend the existing masterlist validator and `GamemodeRegistry` parser for
   the nested block. Extend `GamemodeRuntimeResolver::applyClient` and
   `clearClient` to apply/clear it through the existing camera, aim-body, and
   post-processing owners. Do not create a second camera or smoothing system.
3. `GamemodeRegistry::pollReload` already watches mode JSON. On a valid file
   change, create one validated candidate revision, then reapply the active
   mode overlay at a safe presentation/aim boundary. Invalid or partially
   written JSON keeps the last known-good mode and tuning. Leaving Bloodtest
   clears its overlay and restores shared defaults.
4. A live edit may change presentation immediately on the next render and
   fixed-tick aim update. It must not reset match state, actor pose, ammunition,
   reload, or cooldown. FOV/first person keep the existing mode policy path.
5. Emit rate-limited structured records for override resolution, successful
   revision application, and rejection. Include mode ID, revision/hash,
   changed field paths, effective camera mode/FOV, and the exact rejection
   reason. Do not log every smoothing sample during ordinary play.

The initial tuning workflow is external JSON editing while Bloodtest runs. An
optional in-game tuning panel can be added after the data contract works; it
must edit the same mode-owned fields and must not introduce a second settings
store.

## Tuning passes and proof

Use one pinned Bloodtest run and one map/camera target per comparison. Capture
the initial 70-degree view and resolved JSON revision, then tune in this order:

1. **Attachment:** enable first person and living `head_world`; correct eye
   offset, camera placement, physical head pose, and world wall collision.
2. **Body response:** tune camera position/rotation and head/torso/arm/weapon
   response separately. Confirm mouse input remains readable and the weapon's
   physical muzzle matches the visible weapon.
3. **Optics:** tune edge chromatic aberration, vignette, distortion, exposure,
   and FOV against the supplied references. Keep central detail clear and
   avoid black borders.
4. **Network/replay:** confirm remote actor pose interpolation is not applied
   twice and replay reproduces the same camera/lens policy.

For each pass, save the JSON revision/hash, exact run/build identity, map,
camera pose, and screenshot. Change one setting group at a time. The game
journal must show the candidate was validated and applied; screenshot review
checks appearance, while wall tests and projectile muzzle/contact checks remain
separate behavior evidence. Revert a pass by restoring the previous JSON
revision and confirming the last-known-good values are active.

## Acceptance

Review the profile in live play and replay on at least one enclosed and one
open TDM-compatible map. Capture the same camera/weapon pose and comparable
lighting for each tuning pass. Verify that the view follows the living head,
arms and sights remain visible and aligned, remote actor movement is smooth,
and wall occlusion works. Compare central clarity, edge fringing, exposure,
peripheral shading, FOV, and unintended borders against the supplied visual
references. Verify no outline/healthbar/damage-number hit feedback appears and
that blood/body effects and world bullet impacts are visible instead. Check
reduced-motion settings, weapon aiming, weapon contact, sound, and replay export
separately. Keep the screenshots, sidecars, and exact run/build identity with
the visual review. Human gameplay/visual acceptance remains required.

The repository has a run-local `agent.screenshot` command, but it currently
copies the desktop rectangle at the visible window position as BMP; it is not a
framebuffer capture and is not valid proof when the window is hidden, covered,
minimized, or headless. Exact-tick `screenshot_at_tick` capture must read the
post-UI client framebuffer and record headless/window state; see
`docs/specs/dev-loop/visual-proof-screenshots.md` and
`issue-130ee745052a`. A screenshot or model-generated comparison cannot prove
server-authoritative hit behavior and does not replace human visual acceptance.
When event proof exists but visual acceptance is not yet observed, keep the
bodycam task or issue open with an explicit visual residual.

## Related specifications

- [Bloodtest implementation handoff](bodycam-implementation-handoff.md)
- [SWAT close-quarters mode](swat.md)
- [Replay](../replay/replay.md)
- [Effects](../effects/effects.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
