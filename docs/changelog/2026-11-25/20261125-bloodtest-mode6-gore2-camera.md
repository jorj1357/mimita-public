# Bloodtest mode 6 presentation, gore mode 2, and bodycam spring

## Request

Finish Bloodtest as its intended whole-frame, first-person gore sandbox: mode 6
HUD/console, per-mode proximity fade, local first-person head/cosmetic hiding,
rifle weapon pose, max gore, and a damped camera rather than a per-frame snap.

## Change

- Fixed the Bloodtest gamemode file being rejected by the masterlist. The new
  `presentation.proximity_fade` and `presentation.first_person` keys are now in
  the schema. Before this fix the whole file failed validation, so the mode fell
  back to default blood scale and shared proximity fade.
- Added per-mode presentation evidence:
  `gamemode.presentation.policy`, `ui.scene.draw_path`,
  `first_person.head_hidden`, `camera.bodycam.path`, and `camera.look.trace`.
- Mode 6 now proves a single in-scene HUD/console draw: with whole-frame mode 6
  and `hud.layer=scene`, the client journals `hud_in_scene=true`,
  `postfx_hud=false`, and `console_draw_count=1`.
- Implemented gore mode 2. Actor HP remains the 100-point lethality pool; each
  body part accumulates separate gore damage with meat at 250, bone at 500, and
  destruction at 1000. Mode 1 keeps the original 100-point part model.
  Bloodtest selects `presentation.gore_mode: 2`. Server damage events now report
  `gore_mode`, `part_gore_damage`, `part_health_after`, and `layer_after`.
- Applied the bodycam critically damped spring on both the head-world aim path
  and the plain first-person path. Stiffness, damping, position response,
  maximum angular speed, and settle values remain JSON-owned.
- Added a single-client, no-NPC Bloodtest launch mode (`61`) so presentation
  checks do not require NPC navigation load.
- Regenerated the stale `config/hot-reload/packet-ledger.json` generated
  ledger after the packet codegen check blocked the build. This is a generated
  artifact, not a hand-edited packet schema.

## Evidence

- `20261125_211613` (generation 647): the repaired profile applied with
  `blood_scale=1.0`, `blood_lifetime_scale=1.0`, `proximity_fade=off`,
  bodycam `head_world`, and `weapon.pose.selected=rifle:idle` for
  `bloodtest_ak47`.
- `20261125_221051` (generation 651): mode 6 test produced
  `whole_frame=true`, `scene_layer=true`, `hud_in_scene=true`,
  `postfx_hud=false`, and `console_draw_count=1`. The policy event reported
  `gore_mode=2`, `proximity_fade=off`, and
  `first_person_hide_cosmetics=true`. `first_person.head_hidden` fired for the
  local `NPC Dev` with `head_hidden=true` and `cosmetics_hidden=true`.
- `20261125_222447` (generation 654): the camera journal (category enabled only
  for the test) showed `camera.bodycam.path` with
  `third_person=false`, `force_first_person=true`, `ragdoll_aim_active=true`,
  `orientation_mode=head_world`, `bodycam_override=true`, and the same
  `camconfig_ptr` as the mode resolver. `camera.look.trace` reported
  `stiffness=200`, `damping=28`, and sub-degree rotation error while settled.
- `20261125_222546` (generation 654): server damage events from NPC contact
  reported `gore_mode=2`, independent actor health and per-part gore damage,
  for example actor health `96->93` while head gore reached `3`, and torso gore
  `5->8`. The local first-person presentation and camera path remained active
  during the same run.
- Generations 651-654 compiled the changed game sources; generation 654 included
  the camera path and resolver diagnostics. The test client used the published
  EXE build `2026-11-25T22:06:18.775350Z`.

## Limits

- Gore thresholds above 250 were not reached by the low-damage NPC contact
  used here: the maximum observed part gore was 48, and the 100 HP actor died
  before meat was exposed. The 250/500/1000 transitions are implemented and
  reviewed in `ActorBodyState`/server damage, but a deterministic high-damage
  probe (grenade or direct server damage) is still needed to observe meat,
  bone, and explosion in the journal.
- The mode 6 run temporarily selected fullscreen mode 6 in the test client and
  restored `config/video-settings.json` to its prior value afterward. A player
  must select whole-frame mode 6 to see the stretched HUD/console path.
- The camera category is normally `off` in `config/debuglogger.json`; the
  camera proof above used a temporary category enablement that was restored.
