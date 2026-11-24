# Bloodtest Bodycam Implementation Handoff

**Purpose:** give the OpenCode DeepSeek V4.1 implementation agent a bounded,
code-aware plan for implementing the existing bodycam contract.
**Audience:** the OpenCode implementer, MiMITA code reviewers, and the human
tuning the running mode.
**Does not:** replace [Bloodtest](bloodtest.md),
[Bodycam Presentation](bodycam.md), or shared system specifications; redefine
the requested visuals; or authorize a second implementation of camera, aim,
network interpolation, or reload ownership.

## Assignment and source of truth

Implementation owner: `opencode-deepseek-v4-1` (OpenCode DeepSeek V4.1).
Review role: `bodycam-spec-implementation-reviewer`, read-only after the
implementer reports completion. Keep the work in the active shared checkout
and use its existing dev-loop. Do not open a second source tree or build loop.

Implement the complete Bloodtest bodycam requirements in
[bodycam.md](bodycam.md), together with its relevant Bloodtest mode requirements
in [bloodtest.md](bloodtest.md). Those two specifications are authoritative.
This file records the code map, sequence, and invariants to reduce repository
exploration and avoid accidental duplicate owners.

## Current owner map

These are the current likely integration points; inspect current code before
editing and extend the shared owner rather than assuming this map never drifts.

| Concern | Existing owner to inspect/extend |
|---|---|
| Mode data and JSON parsing | `src/gamemode/gamemode.h`, `src/gamemode/gamemode.cpp` (`GamemodeBodycam`, presentation parser) |
| Masterlist schema and canonical mode data | `config/gamemodes/masterlist/gamemode-masterlist.json`, `config/gamemodes/bloodtest.json` |
| Mode overlay lifecycle | `src/gamemode/gamemode-runtime-resolver.cpp` (`applyClient` / `clearClient`) |
| Camera defaults and bodycam overlay | `src/config/camera-config.h` and its implementation; `src/camera.cpp` (`applyBodycamPose`); `src/engine/engine-tick-camera.cpp` (living `head_world` camera path) |
| Body and physical aiming | `src/entities/aimbody-config.h` and implementation; `src/entities/player-animation.cpp`; shared pose/weapon owner found by tracing the held-weapon pose and authoritative muzzle path |
| Lens effects | existing PostFX owner and `PostFX::ModeBodycamLensOverride` |
| Mode hit-feedback policy | `GamemodePresentation` parsing/resolution and existing shared HUD/impact-feedback owners |
| Live reload and structured diagnostics | `GamemodeRegistry::pollReload`, mode resolver, categorized structured logging owner |
| Replay and replicated pose | existing replay-aware camera/effects path and shared actor snapshot interpolation; do not create a second interpolator |

At present the mode parser and camera override contain camera position/rotation
response, sway, wall-collision, and lens settings. The bodycam specification
also requires independent head, torso, arm, and held-weapon response controls.
The current bodycam override does not yet show those distinct controls, so the
implementer must trace where local desired pose becomes rendered body pose and
where remote snapshot interpolation ends before choosing insertion points.

## Implementation sequence

1. **Trace and record the current data path.** Follow Bloodtest JSON through
   masterlist validation, `GamemodeRegistry`, active-mode resolution, camera,
   aim-body/pose, lens, and live reload. Trace local and remote actor poses and
   the physical muzzle used by server-authoritative projectile validation.
   Identify the existing camera clipping/world occlusion path. Do not treat
   `proximityFade` as camera collision.
2. **Complete the mode schema.** Keep FOV (`camera_fov`) and forced first
   person (`force_first_person`) at their current single-owner mode fields.
   Under `presentation.bodycam`, expose the response and lens/collision fields
   already specified in `bodycam.md`. Add every field to the masterlist,
   parser, validation, and canonical Bloodtest JSON. Preserve documented
   defaults, bounds, units, optional/null inheritance, and unknown/invalid
   value behavior.
3. **Apply a mode overlay through shared owners.** Extend the existing resolver
   and clear path. Global `camconfig.json`, `aimbody.json`, and `postfx.json`
   remain defaults. Bloodtest data must not mutate or save into those globals.
   Leaving Bloodtest clears all of its camera, aim-body, lens, and feedback
   overrides.
4. **Implement time-based presentation response.** Use frame/tick delta-aware
   response (for example, exponential response `1 - exp(-dt / tau)` where that
   fits the existing math) for camera position/rotation and separate head,
   torso, arm, and weapon controls. Integrate with the pose pipeline so each
   quantity is filtered once. When tuning changes live, preserve the current
   filtered state and smoothly converge to the new target; do not reset actor
   pose or blend twice with existing snapshot interpolation.
5. **Keep physical aiming honest.** Physical/hybrid aim and the weapon's
   authoritative muzzle/contact remain owned by existing gameplay systems.
   Bodycam-only visual smoothing must not rewrite fixed-tick hitboxes, movement,
   projectile trajectory, or authoritative muzzle direction. Verify the
   visible sights/muzzle align with the projectile's actual launch direction.
6. **Finish camera fit and occlusion.** Run the living first-person camera from
   `head_world`, keep arms/sights in frame, apply bounded wall collision using
   the existing world collision path, and retain ordinary world occlusion.
   Tune FOV and lens against the supplied screenshots; avoid broad borders,
   hard circular masks, excessive blur, and hidden central detail.
7. **Apply hit-feedback policy and diagnostics.** Bloodtest hides outlines,
   health bars, damage numbers, and hit markers; blood/body feedback and
   bullet-hole/debris world impact remain the intended feedback. Emit
   rate-limited structured records for resolved/applied/rejected mode revision
   with mode ID, revision/hash, changed fields, effective FOV/camera mode, and
   rejection reason. No per-frame smoothing logs.
8. **Build and prove the real feature.** Queue one shared dev-loop build after
   all compiler-input owners report ready. Run Bloodtest through the exact
   checkout dev-loop artifact, inspect its `events.jsonl`, and save screenshot
   evidence with build/run/map/config revision. Verify valid hot edit, invalid
   JSON retaining last-known-good values, leaving the mode clearing overlays,
   enclosed/open map wall occlusion, camera attachment, muzzle alignment,
   replay, and no double interpolation. Screenshot tooling/model comparison
   is supplementary; visual tuning still needs human review.

## Non-negotiable invariants

- Gameplay movement, collision, hit validation, and damage remain fixed 60 Hz.
- Never smooth or rewrite authoritative simulation, server hitboxes, or
  projectile damage direction for visual comfort.
- Do not apply snapshot interpolation twice to remote actors.
- One shared owner for camera, body pose, physical aim, post effects, replay,
  mode parsing, and hot reload. No parallel camera/smoothing system.
- A valid live JSON revision changes only the requested tunables. It does not
  reset match, actor life/pose, ammunition, reload, cooldown, or the current
  smoothed state. Invalid/partial JSON leaves the last known-good revision in
  force.
- Bodycam clipping/wall collision is separate from proximity fade. No wall
  peeking or visibility through geometry.
- Bloodtest's weapon roster, projectile combat, gore implementation, round
  lifecycle, NPC sightline tactics, and broad TDM changes are not expanded by
  this camera handoff unless required by an explicit bodycam acceptance item.

## Work-package completion record

Before asking for review, the implementer should report:

- changed paths and the traced owner/function for each behavior;
- mode schema and a sample resolved Bloodtest JSON revision/hash;
- build request/build ID and exact run ID/artifact;
- journal evidence for apply, reject/last-known-good, and clear behavior;
- enclosed/open wall-occlusion observations and screenshots;
- muzzle alignment, replay, and interpolation observations;
- known misses or human visual-review questions.

The reviewer checks source/spec alignment and evidence without editing source.
Runtime or visual acceptance must be reported separately from compilation.
