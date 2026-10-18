# Captain Falcon down-attack clips

Time: 2026-10-18T21:09:47Z (2026-10-18 17:09:47 EDT)
Branch: `20261011zt`
HEAD at start: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
Status: PASS_WITH_HUMAN_REVIEW

## Pre-existing workspace state

The checkout was already dirty with prior Captain Falcon animation work, including `assets/avatars/captainfalcon/animations.json`, `config/weapons.json`, `tools/mimita_animation_exporter.py`, and animation documentation. Those pre-existing changes were preserved. This record covers this turn's two additional exports, action links, and authoring notes.

## Exact changes

- `config/animations/library/melee/captainfalcon/downpunch_a.json:1-...` — new exporter output; no prior file. Blender Action `downpunch20261018`, root `plrOrigin.036`, six current part Actions, authored-keyframe mode. Result: 60 ticks, four poses, markers `phase.windup=0`, `phase.active=20`, `phase.recovery=40`, `action.end=60`.
- `config/animations/library/melee/captainfalcon/downkick_a.json:1-...` — new exporter output; no prior file. Blender Action `downkick20261018`, root `plrOrigin.037`, six current part Actions, authored-keyframe mode. Result: 60 ticks, four poses, same four marker boundaries.
- `config/weapons.json:1848-1910`
  - `a_ground_sweep`: before legacy `cf_sweep_*` phases and `sweep_foot` attached to `rightLeg`; after `"animation_clip_id": "downpunch_a"`, hitbox ID `down_fist` attached to `rightArm`.
  - `b_falcon_punch_crouch`: before legacy `cf_falconpunch_*` phases; after `"animation_clip_id": "downpunch_a"`; its hitbox remains on `rightArm` and is named `down_fist_b`.
  - `a_air_down_kick`: before legacy `cf_airdown_kick_*` phases; after `"animation_clip_id": "downkick_a"`; existing `rightLeg` hitbox remains.
  - `b_air_down_strike`: before legacy `cf_airdown_strike_*` phases and `strike_fist` on `rightArm`; after `"animation_clip_id": "downkick_a"`, hitbox ID `strike_foot` on `rightLeg`.
  Existing size, offset, active window, damage, knockback, impulse, and audio values were preserved.
- `assets/avatars/captainfalcon/animations.json:364-365`
  - Before: profile ended after `"config/animations/library/melee/captainfalcon/uppunch_a.json"`.
  - After: appended `downpunch_a.json` and `downkick_a.json` include paths.
- `tools/mimita_animation_exporter.py:409`
  - Before: example asked for `plrOrigin.032` for the upkick screenshot.
  - After: example names `plrOrigin.036` for downpunch and `.037` for downkick.
- `docs/specs/gamemodes/supersmashbrosmelee.md:374-377`
  - Before: grounded Ctrl+left was a ground sweep, grounded Ctrl+right was Falcon Punch, and airborne Ctrl attacks referenced separate legacy down moves.
  - After: both grounded Ctrl attacks document `downpunch_a`; both airborne Ctrl attacks document `downkick_a`, with B combat values stronger.
- `docs/specs/animations/blender-animation-authoring.md:3,62-78`
  - Before: status mentioned only the upkick export and the self-export steps covered only `uppunch_a`.
  - After: status includes the up/down exports and the table documents the action/root/clip mappings for up-punch, down-punch, and down-kick.

## Export and configuration validation

- Ran `py tools/mimita_animation_exporter.py` twice against `C:\important\quiet\blender-projects\mimita sizing 6.blend`, choosing the roots and clip IDs above. Both Blender exports completed successfully.
- Parsed `weapons.json`, the Captain Falcon animation profile, and both clip files. Assertions confirmed all four action IDs point to the requested clip, both clip files are included, and each contains the required four Melee markers. Result: PASS.
- The exporter profile prompt was left blank during these runs; the two include paths were then added directly to the active profile and verified.
- No test suite was run.

## Build and runtime evidence

- Canonical dev-loop build request succeeded and published build 2612.
- Ran `.dev\\builds\\2612\\mimita.exe --versioninfo`; it reported `EVENTS_JSONL_PATH=logs/10-18-2026/20261018_170923/events.jsonl`.
- Dev-loop status after the build still reported the existing client on build 2607 (`run_alive=true`) and no server (`server_alive=false`). No game process was restarted. The current user-visible session therefore did not exercise these new mappings; visual and gameplay acceptance remain pending.

## Documentation review

Focused skills: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; `docs/skills/asset-checker-v1.md` — PASS for generated paths and includes; `docs/skills/documentation-checker-v1.md` — PASS for the updated controls/export instructions.

The router-listed `docs/regressions/regressions-v1.md` and `docs/doc-review-09-03-2026.md` do not exist; read the current `docs/regressions/README.md` and relevant routed specs instead. The documentation TODO scan also found the unrelated note at `docs/architecture/code-ownership/code-ownership.md:3`: `9 3 2026 todo maybe expand this? explain what exact kinds of files should go where , without putting exact file paths so that code can change to match the spec`. Suggested future edit: replace it with concise ownership categories and decision rules for assigning file responsibilities without naming implementation paths. It was left unchanged because it is outside this request.
