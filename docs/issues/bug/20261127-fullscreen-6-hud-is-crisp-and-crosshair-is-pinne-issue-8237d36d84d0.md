# issue-8237d36d84d0: Fullscreen 6 HUD is crisp and crosshair is pinned to centre instead of the mode weapon aim point

## Human report (verbatim, immutable)

> can u ensure thta  the hud in fullscreen mode 6 is stretched, like all of it, noneo  f it is crisp , adn ensure the corsshair follows the mode crosshair rules like right now the crosshair is crisp and in cetner of screen but its suposed to be fun counter strike mod which has the wepaon point where its suposed to go 
> 
> coordinate with toehragents to not interfere


## Agent discussion and suggestions

### 2026-11-27T05:45:22.769Z — hud-fs6-crosshair

Message: `msg-fe71efba3d56` (message)

Subject: File overlap: fullscreen 6 HUD stretch + crosshair

I am editing src/engine/engine-tick.cpp, src/gui/ui-system.cpp, and src/engine/engine-tick-ui-game-hud.cpp for the user's fullscreen 6 HUD-stretch/crosshair request. Your jitter-probe/live render alpha task touches the same HUD file (JitterProbe::sampleHud call site). Please avoid editing those three files until I report the lease released (about one build cycle); if you have an in-flight edit there, reply with the exact hunk and I will coordinate around it. No other overlap expected.

## Review (owner-fixed-runtime-proven)

### 2026-11-27T06:22:36.076Z — hud-fs6-crosshair

Reviewer: hud-fs6-crosshair

Owner evidence (hud-fs6-crosshair). Fix: engine-tick.cpp routes every whole-frame mode (fullscreen 5/6) into the 1024x768 scene target; ui-system.cpp resolves the HUD size from the scene target for that path; the gameplay crosshair follows the mode aim rule (aim_source=physical draws at the projected weapon point, otherwise centred) instead of being force-centred. Runtime proof: run 20261127_060325 (generation 779, Fun Counter Strike, dust2cyberiav4, client-01): ui.scene.draw_path whole_frame=true ui_in_scene=true postfx_hud=false; ui.scene.presentation framebuffer 1024x768 vs output 1920x1080 stretched=true; hud.crosshair.center fullscreen_mode=6 hud_space=scene physical_aim_used=true centered=false with live center offsets (e.g. x=506.1,y=395.6 against 512/384) and hud.crosshair.verdict matches=true. Earlier run 20261127_055253 shows the same at generation 778. Spec reconciled: graphics-resolution.md is authoritative; graphics-post-processing.md and bodycam.md now document modes 5/6 as in-frame stretched HUD. Human visual acceptance of the stretched look is still for the user; no human acceptance is claimed.

## Review (owner-fixed-runtime-proven)

### 2026-11-27T06:22:36.439Z — hud-fs6-crosshair

Reviewer: hud-fs6-crosshair

Owner evidence (hud-fs6-crosshair). Fix: engine-tick.cpp routes every whole-frame mode (fullscreen 5/6) into the 1024x768 scene target; ui-system.cpp resolves the HUD size from the scene target for that path; the gameplay crosshair follows the mode aim rule (aim_source=physical draws at the projected weapon point, otherwise centred) instead of being force-centred. Runtime proof: run 20261127_060325 (generation 779, Fun Counter Strike, dust2cyberiav4, client-01): ui.scene.draw_path whole_frame=true ui_in_scene=true postfx_hud=false; ui.scene.presentation framebuffer 1024x768 vs output 1920x1080 stretched=true; hud.crosshair.center fullscreen_mode=6 hud_space=scene physical_aim_used=true centered=false with live center offsets (e.g. x=506.1,y=395.6 against 512/384) and hud.crosshair.verdict matches=true. Earlier run 20261127_055253 shows the same at generation 778. Spec reconciled: graphics-resolution.md is authoritative; graphics-post-processing.md and bodycam.md now document modes 5/6 as in-frame stretched HUD. Human visual acceptance of the stretched look is still for the user; no human acceptance is claimed.

## Resolution

### 2026-11-27T06:24:54.938Z — hud-fs6-crosshair

Fixed: fullscreen 5/6 HUD is composed at 1024x768 and stretched; crosshair follows the mode aim rule (physical weapon point).

Evidence: evidence-91153ceba57c integration proven; run 20261127_060325 generation 779 (integration, proven)
