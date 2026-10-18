# Captain Falcon up-punch export and landing hold

Time: 2026-10-18T21:00:45Z (2026-10-18 17:00:45 EDT)
Branch: `20261011zt`
HEAD at start: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
Status: PASS_WITH_HUMAN_REVIEW

## Pre-existing workspace state

The checkout was already dirty before this task, including animation profiles and assets, weapon data, animation specs, exporter code, player animation code, and multiple changelogs. Those existing edits were preserved. This record covers the additional `uppunch` export/mappings and landing-priority change made during this session.

## Changes with exact old and new text

- `tools/mimita_animation_exporter.py:261-269`
  - Before: `if root_transform_curves: raise ValueError("Animated plrOrigin/root transform channels are not supported in this export version")`
  - After: evaluate each root transform curve over the clip and reject it only when `max(values) - min(values) > 1e-5`; fixed keyed root placement is ignored. This fixed the exporter error for the Blender scene's constant root channels while retaining the unsupported-root-motion guard.
- `config/animations/library/melee/captainfalcon/uppunch_a.json:1-...` (new exporter output; no prior text)
  - Exported Blender action set from `mimita sizing 6.blend`, root `plrOrigin.033`, six current body-part Actions, authored keyframes, and Melee markers. Result: clip ID `uppunch_a`, duration 60 ticks, four poses, markers windup 0 / active 20 / recovery 40 / exclusive end 60.
- `assets/avatars/captainfalcon/animations.json:363`
  - Before: `"config/animations/library/melee/captainfalcon/upkick_a.json"`
  - After: `"config/animations/library/melee/captainfalcon/uppunch_a.json"`
- `config/weapons.json:1734-1840`
  - `b_falcon_punch`: before legacy `cf_falconpunch_*` phases; after `"animation_clip_id": "punch_a"`.
  - `b_air_forward_dash`: before legacy `cf_fwdash_*` phases and right-arm fist hitbox; after `kick_a`, with its hitbox attached to `rightLeg` and kick limb offset `[1.0, -3.0, 0.5]`. Existing damage, impulse, and active-window values remain.
  - `a_uptilt`: before legacy `cf_uptilt_*` phases; after `uppunch_a`.
  - `a_air_up_kick` and `b_air_up_special`: before `upkick_a` and right-leg foot hitboxes; after `uppunch_a` and right-arm fist hitboxes. Their separate damage/impulse values remain, so B stays stronger.
- `src/entities/player-animation.cpp:461-466`
  - Before: automatic jump and fall selectors ran before `landAnimTimer`.
  - After: landing is selected first among automatic states and holds above jump, fall, crouch, locomotion, and idle until the authored landing duration expires. Locked actions and hurt/respawn/death still take precedence.
- `docs/specs/animations/animations.md:592-598`
  - Before: `idle < walk < run < crouch < land < fall < jump < hurt < respawn < death` and landing only promised to interrupt walk/run.
  - After: `idle < crouch < walk < run < jump < fall < land < hurt < respawn < death`; landing holds over automatic states through the full one-shot.
- `docs/specs/gamemodes/supersmashbrosmelee.md:368-373`
  - Before: B-ground used Falcon Punch animation, B-air used forward-dash pose, and upward moves used old clips.
  - After: B-ground references `punch_a`; unmodified air attacks reference `kick_a`; all Space+primary upward attacks and upward special reference `uppunch_a`.
- `docs/specs/animations/blender-animation-authoring.md:64-81`
  - Before: self-export instructions named `plrOrigin.032` / `upkick_a` and said keyed root transforms fail.
  - After: instructions name `plrOrigin.033` / `uppunch_a`; fixed root placement is allowed and varying root motion is rejected.

## Documentation and crouch investigation

Read the animation, Melee, avatar, weapon, JSON configuration, asset authoring, build, runtime validation, and focused behavior-review routes. The Melee spec's historical crouch report (lines 801-807) attributes an old crouch/fall flash to reading raw `onGround`; its current rule (lines 396-399) says Melee pose selection uses `stableOnGround`. Current source does use `ground.stableOnGround` for airborne selection and reads the Ctrl crouch input each tick. The available old journal has 3,849 player state changes and 64 with `crouching=true`, but none show a crouch or landing transition with the crouch input active; it is not a controlled reproduction. The inspected landing asset is only 7 ticks, and the existing journal shows land-to-idle after 8 ticks. Therefore the brief land-to-crouch transition after the one-shot is expected; repeated crouch toggling remains unconfirmed and needs a controlled Ctrl-held run.

No regression record was added because this session did not confirm a current crouch regression or its cause. The focused Documentation Checker search found only the router/skill's instructional mentions of TODO phrases; no new TODO was introduced.

## Validation and evidence

- Export: PASS. `py tools/mimita_animation_exporter.py` launched Blender 5.0 against `C:\important\quiet\blender-projects\mimita sizing 6.blend`; output written to `config/animations/library/melee/captainfalcon/uppunch_a.json`.
- Data validation: PASS. JSON parsing and assertions confirmed the profile include, the six requested action-to-clip IDs, and the 60-tick markers.
- Build: PASS. Canonical dev-loop build request published build 2611. The exact artifact `.dev\builds\2611\mimita.exe --versioninfo` printed its executable identity and `EVENTS_JSONL_PATH`.
- Live gameplay: NOT RUN. Dev-loop status after build reported the existing client still on build 2607, `run_alive=true`, and `server_alive=false`; no existing client/server was restarted. The versioninfo invocation is not gameplay evidence.
- Tests: NOT RUN (not requested).
- Human visual/gameplay acceptance: pending, especially up-punch appearance and Ctrl-held crouch behavior.

Focused skill: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; `docs/skills/asset-checker-v1.md` — PASS for exported JSON structure, in-game visual acceptance pending; `docs/skills/documentation-checker-v1.md` — PASS for changed routes/claims.
