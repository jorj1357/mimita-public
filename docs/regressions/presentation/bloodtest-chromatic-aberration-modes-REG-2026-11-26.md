# Bloodtest chromatic-aberration lens modes — regression and proof record

- **Recorded:** 2026-11-26
- **Owner:** `opencode-deepseek-v4-1`
- **Latest runtime evidence:** run `20261126_231556`, game generation 738
- **First runtime evidence:** run `20261126_175452`, game generation 714
- **Domain:** presentation / render pipeline (`docs/regressions/presentation/`)

This record covers the five live-switchable chromatic-aberration (CA) lens
modes requested for Bloodtest, the regressions found while making them
independently switchable, and the pixel/journal evidence for each mode. It also
covers the 2026-11-26 animation mode 2 limb tether and the 1024x768 stretched
presentation. It is a regression and evidence record, not a behavior
specification. The behavior contract lives in
`docs/specs/gamemodes/bloodtest.md`.

## Mode contract

| Mode | Name | World CA | UI / world-UI / 3D popup CA | Bodycam lens |
|---|---|---|---|---|
| 1 | `mixed` | background/dark pixels only | same mask (bright UI stays crisp) | fisheye + vignette + blur |
| 2 | `world_only` | whole world | none; UI composited after PostFX | none |
| 3 | `normal` | none; black stays black | none | none |
| 4 | `world_and_ui` | whole world | yes; UI drawn into the scene before PostFX | none |
| 5 | `bodycam_special` | whole world | yes; whole 1024x768 frame incl. HUD | fisheye + vignette + blur, CA toggle |

Modes are exclusive. Exactly one compositing policy is active at a time; the
earlier `full_world`/`background_only` pair is mapped onto modes 2 and 1 so old
configuration keeps its meaning.

Lens mode 5 uses the chosen Option 1 pipeline: world and HUD both render at
1024x768, PostFX processes that frame (CA when enabled, fisheye, vignette,
peripheral blur), and the whole frame is stretched to the display. The HUD is
therefore stretched and, when CA is enabled, CA'd. Fullscreen mode 6 is entered
explicitly with `fullscreen 6`; it is not the default. `lens_ca on|off` toggles
CA without changing the lens.

Considered but not implemented:

- Option 2: world at 1024x768 stretched, HUD after PostFX at output resolution
  (crisp HUD, not stretched).
- Option 3: world at 1024x768 stretched, HUD at 1024x768 stretched without CA
  (needs a separate UI target).

## Shader pass map

1. **Scene/world pass** — `engine-tick-render.cpp` renders world, effects and
   (for modes 1/3/4) the HUD into the PostFX colour target.
2. **PostFX fullscreen pass** — `PostFX::render()` binds `postfx-fbo` and draws
   `shaders/post.frag`. Chromatic aberration runs before lens distortion:
   `uv` is sampled with a radial or horizontal offset, red is taken from
   `uv + dir*off`, blue from `uv - dir*off`, and the result is mixed by
   `caMask`. `uChromaticAberrationBackgroundOnly != 0` turns `caMask` into
   `1 - smoothstep(0.02, 0.35, luma)`, which is the mode-1 mask.
3. **UI composition** — `engineTickRenderFrame()` decides
   `uiInScene = wholeFrame && PostFX::lensModeMakesUiScene(mode, configured)`.
   Mode 2 forces UI after PostFX; mode 4 forces UI into the scene; modes 1/3
   follow the configured scene layer.
4. **Output/UI pass** — when UI is not in the scene, the HUD and console are
   drawn once after `PostFX::render()` at output resolution. No second CA pass
   is applied to the HUD; the single PostFX pass owns CA.

## Confirmed regressions and fixes

| ID | Observation | First recorded (UTC) | Cause | Fix | Proof |
|---|---|---|---|---|---|
| R1 | "Normal" mode still showed CA fringing | 2026-11-26T17:00Z probe run `20261126_170006` | `effectiveChromaticAberration()` did not zero the uniform for mode 3, and the bodycam override was applied unconditionally | mode 3 returns `0.0f`; `PostFX::render()` writes `0.0` for mode 3 | final `atomic`/`final3` verdict: `world_pattern_dark_mean = 0.0`, `world_pattern_bright_mean = 0.0` |
| R2 | Mode 4 did not CA the UI | 2026-11-26T17:00Z probe run `20261126_170006` | UI was always drawn after PostFX, so it never entered the scene target; test client was also not in whole-frame mode | `lensModeMakesUiScene()` returns true for mode 4 and `uiInScene` routes the HUD into the scene; whole-frame mode 6 required | final `final4`: `ui_pattern_mean_channel_misalignment = 0.03857` vs `0.00069` in mode 2 |
| R3 | HUD missing in modes 3 and 4 | 2026-11-26T17:37Z run `20261126_172857` | bodycam radial lens crop pushed the HUD outside the frame | distortion forced to `0.0` for modes 3/4 | screenshots `final3`/`final4` show HP, AK ammo and status text |
| R4 | HUD double-fringed in mode 4 | 2026-11-26T17:02Z change note | HUD CA strength was still applied by the UI shader after the PostFX pass | resolver passes `0.0f` for `hud.caStrength`; single PostFX pass owns CA | mode-4 UI is fringed once by the scene pass; no second fringe on the output pass |
| R5 | Probe could not distinguish CA from natural R/B scene colour | 2026-11-26T17:08Z run `20261126_171712` | metric was `abs(r-b)` over arbitrary scene pixels | metric is channel-edge misalignment on a dense achromatic pattern; sample window is the pattern rectangle | mode 3 baseline drops to exactly `0.0`; mode 2 bright score `0.03243` |
| R6 | Mode evidence was split by a mid-run generation swap | 2026-11-26T17:38:59Z, run `20261126_173815` | `game.generation.activated` reloaded the DLL between `lens_mode` and `presentation_probe`, discarding the in-DLL override | atomic `presentation_probe <tag> <mode>` applies mode + pattern and captures two frames later in one request | final run has no `game.generation.activated`; each verdict reports `mode_applied=true` |

## Hypotheses and evidence

### H1 — A single PostFX pass can own all four CA policies

- **Prediction:** one fullscreen pass with `caMask` and a UI-layer routing
  decision reproduces background-only, full-world, off, and full-world+UI
  without layering two CA passes.
- **Evidence for:** final run mode 1 dark `0.06281` vs bright `0.00591`
  (10.6x); mode 2 dark `0.08570` / bright `0.03243`; mode 3 both `0.0`; mode 4
  dark `0.09809` / bright `0.11181`. Mode 4 is the only mode with a non-zero UI
  pattern score (`0.03857`).
- **Evidence against:** none in the final run.

### H2 — Achromatic test tiles make the score CA-specific

- **Prediction:** equal R/G/B tiles produce zero channel misalignment without
  CA, so any score above zero is CA (or resampling), not the pattern's colour.
- **Evidence for:** with mode 3 active, both dark and bright pattern scores are
  exactly `0.0`.
- **Evidence against:** earlier chromatic bars produced non-zero baseline and
  false negatives; those bars were removed.

### H3 — Dense pattern coverage removes scene baseline

- **Prediction:** filling the probe's centre window with the pattern removes
  level geometry from the dark/bright averages.
- **Evidence for:** 7x9 tile grid at 2.0 m, probe window `240x180 px`:
  mode-3 score is `0.0`; before the dense grid the same region reported
  `0.00617` dark baseline.
- **Evidence against:** none observed.

### H4 — Mode 4 requires whole-frame composition

- **Prediction:** if UI is not in the scene, CA cannot reach it.
- **Evidence for:** mode-4 verdict requires `whole_frame && uiInScene`; with
  fullscreen mode 6 it is true and the UI score is `0.03857`.
- **Evidence against:** windowed/non-whole-frame launches cannot prove mode 4;
  the verdict deliberately fails rather than claiming success. A future
  output-resolution UI CA pass would remove this dependency.

### H5 — Deferring the probe removes the generation-swap race

- **Prediction:** applying the mode and reading pixels inside one request
  survives a concurrent published generation because no command boundary sits
  between them.
- **Evidence for:** run `20261126_175452` has no
  `game.generation.activated`; all four verdicts have
  `requested_mode == mode` and `mode_applied == true`.
- **Evidence against:** a swap landing inside the two-frame capture window
  would still invalidate that probe; the verdict would report the new mode and
  fail the requested-mode assertion.

### H6 — The background-only mask leaves bright UI crisp

- **Prediction:** high-luma UI pixels get `caMask ~ 0` in mode 1, so mode-1 UI
  reads near the mode-3 baseline while dark world pixels fringe.
- **Evidence for:** mode 1 UI `0.00083`, close to mode 3 `0.00088`; mode 1 dark
  pattern `0.06281`.
- **Evidence against:** mode-1 UI is not separately asserted because the
  bodycam crop can push it off-screen; mode 1 is proven on the world region.

## Final per-mode evidence

Run `20261126_175452` (client PID 32636, server PID 18188), build
`2026-11-26T17:45:43.947108Z:23e7e0ec4b19736a65b23da87a85fde98652032cd3472b77d071326565db13f2`,
game generation 714, launch mode 61, fullscreen video mode 6, map
`slaughterhouse1`, profile `npc-navigation`.

Command sequence (atomic, one call per mode):

```
presentation_probe final1 1
presentation_probe final2 2
presentation_probe final3 3
presentation_probe final4 4
```

Each call emits: `presentation.lens.mode.changed`, `presentation.lens.mode`,
`presentation.lens.policy`, `presentation.postfx.shader`,
`presentation.postfx.pass`, `presentation.ui.layer`,
`presentation.framebuffer.readback`, `presentation.pixel.dark_ca`,
`presentation.pixel.bright_ca`, `presentation.pixel.ui_ca`,
`presentation.mode.verdict`, plus `terminal.command.completed`.

| Mode | Verdict UTC | matches | world dark | world bright | UI pattern | Screenshot |
|---|---|---|---|---|---|---|
| 1 mixed | 2026-11-26T17:55:25.025Z | true | 0.06281 | 0.00591 | 0.00083 | `build/presentation-probes/final1-mode1-1795715724668.png` |
| 2 world_only | 2026-11-26T17:55:26.980Z | true | 0.08570 | 0.03243 | 0.00069 | `build/presentation-probes/final2-mode2-1795715726570.png` |
| 3 normal | 2026-11-26T17:55:28.633Z | true | 0.0 | 0.0 | 0.00088 | `build/presentation-probes/final3-mode3-1795715728344.png` |
| 4 world_and_ui | 2026-11-26T17:55:30.378Z | true | 0.09809 | 0.11181 | 0.03857 | `build/presentation-probes/final4-mode4-1795715730097.png` |

Supporting crops:

- `build/presentation-probes/evidence-mode2-ui-clean.png` — mode-2 UI bars
  stay achromatic (no fringe) while the world fringe is visible behind them.
- `build/presentation-probes/evidence-mode4-ui-ca.png` — the same UI bars in
  mode 4 show red/blue borders and the HUD text splits into red/blue ghosts.

Build generations used for this record:

| Generation | Build attempt | Published UTC | Changed paths | Result |
|---|---|---|---|---|
| 711 | `game-dll-20261126T174647924868Z-2feedb` | 2026-11-26T17:47Z | `src/engine/engine-tick.cpp`, `src/render/postfx-commands.cpp` | atomic deferred probe |
| 713 | `game-dll-20261126T175102072132Z-c0898f` | 2026-11-26T17:52Z | `src/engine/engine-tick-render.cpp`, `src/render/postfx-commands.cpp` | dense achromatic world pattern |
| 714 | `game-dll-20261126T175409817450Z-dd66fb` | 2026-11-26T17:54Z | `src/render/postfx-commands.cpp` | calibrated verdict thresholds |

## Exact code changes

| Path | Change |
|---|---|
| `src/render/post-fx.h` | `LensMode` enum; `lensMode` in `ModeBodycamLensOverride`; `effectiveLensMode()`, `effectiveChromaticAberration()`, `setLensModeOverride()`, `clearLensModeOverride()`, `hasLensModeOverride()`, `lensModeMakesUiScene()`, `lensModeName()`; presentation-pattern frame counter; deferred probe declarations. |
| `src/render/post-fx.cpp` | Mode resolution (runtime override → bodycam lens → config); mode 3 zeroes CA; mode 1 sets background-only; modes 3/4 disable the bodycam distortion crop; mode application happens in `PostFX::render()`. |
| `src/render/postfx-commands.cpp` | `lens_mode <1|2|3|4|config>` live command; `presentation_pattern <frames>`; atomic `presentation_probe <tag> [mode]` with two-frame deferred capture; pixel metric (channel-edge misalignment); per-mode verdict; PNG row flip. |
| `src/engine/engine-tick.cpp` | `uiInScene` derived from whole-frame and lens mode; terminal/HUD draw exactly once; presentation-pattern frame consume; deferred probe tick. |
| `src/engine/engine-tick-render.cpp` | Dense 7x9 achromatic world test pattern in the scene pass, active only while pattern frames remain. |
| `src/engine/engine-tick-ui.cpp` | Achromatic UI test bars at both possible UI y origins, active only while pattern frames remain. |
| `src/gamemode/gamemode-runtime-resolver.cpp` | Maps config `chromatic_aberration_mode` to `lensMode`; passes `0.0f` for `hud.caStrength` so CA has one owner. |
| `src/gamemode/gamemode.h`, `gamemode.cpp` | Bloodtest bodycam lens mode field and config parsing. |
| `config/gamemodes/bloodtest.json` | Bloodtest default lens mode `world_only` (mode 2). |
| `config/gamemodes/masterlist/gamemode-masterlist.json` | Allowed lens-mode values. |
| `shaders/post.frag` | CA is applied before distortion; full-world vs background-only `caMask`. |
| `config/video-settings.json` | Temporarily `fullscreen: 6` for the whole-frame proof run; restored to the checked-in `0` after the run. |

## What did not work and was corrected

1. Measuring `abs(r - b)` over arbitrary scene pixels scored natural colour and
   could not separate modes. Replaced by channel-edge misalignment on an
   achromatic pattern.
2. Seven chromatic/achromatic bars did not fill the probe window, so dark
   samples were mostly scene. Replaced by a dense 7x9 achromatic grid.
3. The UI pattern was drawn at one guessed y origin and sometimes landed
   outside the sampled top strip. It is now drawn at both origins.
4. The probe PNG was written bottom-up. Rows are now flipped before encoding.
5. Mode switching and pixel capture in separate commands let a published
   generation swap drop the in-DLL override. They are now one atomic request.
6. Verdict thresholds were absolute and overfit to one run. They are now
   calibrated with wide margins against the dense pattern, and mode 3 is
   asserted at exactly zero.
7. The bodycam distortion crop hid the HUD in modes 3/4. It is disabled for
   those modes.
8. A second HUD CA pass double-fringed mode 4. HUD CA strength is zero because
   the single PostFX pass owns CA.

## Reproduction

```
python devscripts/dev-loop.py --request batch-build
python devscripts/dev-loop.py --start-test-run --test-launch-mode 61 \
  --test-game-generation <current> --test-map slaughterhouse1 \
  --test-profile npc-navigation
python devscripts/dev-loop.py --command "presentation_probe p1 1" --run-id <run> --client client-01
python devscripts/dev-loop.py --command "presentation_probe p2 2" --run-id <run> --client client-01
python devscripts/dev-loop.py --command "presentation_probe p3 3" --run-id <run> --client client-01
python devscripts/dev-loop.py --command "presentation_probe p4 4" --run-id <run> --client client-01
```

Inspect `logs/<date>/<run>/client-01/events.jsonl` for
`presentation.mode.verdict`; every row must have `mode_applied=true` and
`matches=true`, and the journal must not contain
`game.generation.activated` between the probes.

## Limits and open work

- **Mode 4 currently requires whole-frame composition** (fullscreen video mode
  6). A windowed launch has no UI CA and the verdict fails by design. Adding an
  output-resolution UI CA pass would make mode 4 independent of whole-frame;
  that is future work, not claimed here.
- The probe scores a synthetic pattern, not the shipped scene. The UI test
  bars prove the scene-drawn UI layer, which also carries world-anchored 3D
  popups and PNG-backed elements when they are submitted to that layer; those
  individual element types were not separately probed. Human visual acceptance
  of the real Bloodtest look is still required.
- The lens strength is the existing bodycam value (`0.8`). Mode 4 therefore
  produces a strong radial split on thin HUD text; whether that is the desired
  final look is a human tuning decision.
- This record proves the lossless pathway and mode switching. It does not claim
  the CA amount, vignette, or bodycam crop are final art choices.

## 2026-11-26 23:16Z — mode 5, animation tether, stretched 1024x768, no-limit lens config

### New regressions and fixes

| ID | Observation | First recorded (UTC) | Cause | Fix | Proof |
|---|---|---|---|---|---|
| R7 | Mode 5 rendered almost black with no HUD | 2026-11-26T23:01:34Z, run `20261126_230040` | hard circular lens edge (`edge_mode=circle`, `edge_radius=0.05`, `edge_darkness=0.95`, `zoom=2.2`) pushed the HUD outside the frame | `edge_mode=vignette`, `edge_radius=0.72`, `edge_softness=0.3`, `edge_darkness=0.0`, `zoom=1.0`, `vignette=0.35` | `proof5on`: HUD visible, corner darkening `0.6651`, no hard crop |
| R8 | Mode 5 UI CA could not be measured | 2026-11-26T23:06:42Z, run `20261126_230528` | large flat UI bars had too few edges and the sample window missed them | dense 12 px achromatic checkerboard at top and bottom; probe samples top/bottom quarters | CA-on UI `0.02406`, CA-off UI `0.00298` |
| R9 | Mode 5 CA-off verdict failed by 0.00016 | 2026-11-26T23:12:07Z, run `20261126_231311` | the stretched checkerboard has a natural resampling floor (`0.00416`) even with CA off | CA-off UI threshold `<0.012`, CA-on `>0.02` | `proof5off`: matches true |
| R10 | Animation mode 2 hard-zeroed every part translation | pre-existing `rootTranslationMode=2` code | `target.translation = 0` removed steps, crouch and arm reach | smooth `budget*tanh(len/budget)` tether after all pose layers; global mode 2 wins over clip overrides | arm `1.155 m -> 0.01 m`, head `0.154 -> 0.005`, `over_budget_count=0` |

### Final evidence

Run `20261126_231556`, game generation 738, launch mode 61, map
`slaughterhouse1`, profile `npc-navigation`. The client was switched live with
`fullscreen 6`; no config default changed.

| Check | Verdict UTC | matches | Key values |
|---|---|---|---|
| fullscreen 6 applied | 2026-11-26T23:16:26.290Z | — | scene `1024x768`, output `1920x1080`, stretch `1.875 x 1.40625`, whole-frame true |
| config limits inventory | 2026-11-26T23:16:27.382Z | — | `min_token_count=109`, `max_token_count=33`, nine lens fields removed |
| stretch probe | 2026-11-26T23:16:28.731Z | true | corner non-black `1.0`, centre `0.99899` |
| mode 5 CA on | 2026-11-26T23:16:32.453Z | true | world dark `0.07090`, bright `0.03469`, UI `0.02406`, corner dark `0.6651`, edge falloff `0.6154` |
| mode 5 CA off | 2026-11-26T23:16:34.344Z | true | world dark `0.00178`, bright `0.00050`, UI `0.00298`, corner dark `0.6672`, edge falloff `0.6172` |
| mode 2 clean | 2026-11-26T23:16:37.730Z | true | world dark `0.09824`, bright `0.11188`, UI `0.00551`, lens vignette `0`, distortion `0` |
| animation mode 2 | 2026-11-26T23:16:39.624Z | true | `max_out_len=0.01`, `max_budget=0.01`, `over_budget_count=0` |

Per-part tether samples from the same run:

| Part | Raw translation | Tethered output | Budget |
|---|---:|---:|---:|
| torso | 0.0633 m | 0.005 m | 0.005 m |
| head | 0.1541 m | 0.005 m | 0.005 m |
| leftArm | 1.1550 m | 0.010 m | 0.010 m |
| rightArm | 0.7467 m | 0.010 m | 0.010 m |
| leftLeg | 0.0578 m | 0.010 m | 0.010 m |
| rightLeg | 0.0917 m | 0.010 m | 0.010 m |

Screenshots:

- `build/presentation-probes/proof5on-mode5-1795734992151.png` — stretched
  1024x768 frame, world and HUD checkerboard both CA'd, HUD visible.
- `build/presentation-probes/proof5off-mode5-1795734994047.png` — same frame
  with CA off; checkerboard is crisp, vignette/fisheye remain.
- `build/presentation-probes/proof2-mode2-1795734997424.png` — clean world-only
  CA; UI checkerboard crisp.

### No-limit proof (out-of-band value)

Run `20261126_232129`, generation 741, with `presentation.bodycam.lens.vignette`
temporarily set to `0.9` (old schema max `0.8`):

- `gamemode.bodycam.applied` at 2026-11-26T23:22:0xZ carried
  `vignette: 0.9`, so the parser and resolver accepted the out-of-band value.
- `config.value.extreme` at 2026-11-26T23:22:0xZ reported
  `path=presentation.bodycam.lens`, `values={vignette: 0.9}`,
  `baseline=2026-11-26`.
- `presentation.mode.verdict` for mode 5 at 2026-11-26T23:22:08.252Z still
  reported `matches=true` (world bright `0.0339`, UI `0.0226`).
- The vignette was restored to `0.35` after the proof.

This is the acceptance shape from
`docs/specs/config-load/config-limits-retirement.md`: no range gate, the applied
value is visible in the journal, and the feature still runs.

### Event inventory per phase

| Phase | Events emitted | Reused / new |
|---|---|---|
| no-limit lens | `config.limits.inventory`, `config.limits.policy`, `gamemode.bodycam.applied` (lens block), `presentation.lens.policy`, `presentation.pixel.vignette`, `presentation.pixel.dark_ca`, `presentation.mode.verdict`, `presentation.lens.verdict` | 4 reused, 2 new |
| mode 5 | `presentation.lens.mode.changed`, `presentation.lens.mode`, `presentation.lens.policy`, `presentation.postfx.shader`, `presentation.postfx.pass`, `presentation.ui.layer`, `presentation.framebuffer.readback`, `presentation.pixel.dark_ca`, `presentation.pixel.bright_ca`, `presentation.pixel.ui_ca`, `presentation.pixel.vignette`, `presentation.pixel.fisheye`, `presentation.mode.verdict`, `presentation.lens.verdict`, `presentation.ca.toggle` | 10 reused, 3 new |
| animation mode 2 | `anim.settings.loaded`, `anim.root_mode.changed`, `anim.tether.settings`, six per-part `anim.tether`, `anim.tether.verdict`, `anim.state.changed`, `avatar.pose.live`, `AIMBODY_LIVE_SAMPLE` | 4 reused, 2 new families |
| fullscreen 6 | `video.fullscreen.mode_applied`, `video.stretch.policy`, `video.stretch.probe`, `video.stretch.verdict`, `ui.scene.presentation`, `ui.framebuffer-size`, `postfx.fbo-probe`, `postfx.pass`, `presentation.framebuffer.readback`, `presentation.mode.verdict` | 7 reused, 3 new |

### Exact code changes

| Path | Change |
|---|---|
| `src/render/post-fx.h` / `.cpp` | `LensMode::BodycamSpecial=5`; clamp 1..5; `lensModeMakesUiScene(5)=true`; lens on only for modes 1/5; mode 2/3/4 lens off; CA override (`lens_ca`); bodycam lens getter; mode name. |
| `src/render/postfx-commands.cpp` | `lens_mode 5`; `lens_ca on/off/value`; mode-5 verdict; lens verdict; vignette/fisheye pixel probes; extended `presentation.lens.policy`; `config_limits_probe`; `presentation_stretch_probe`. |
| `src/engine/engine-tick-ui.cpp` | dense achromatic UI checkerboard for the probe. |
| `src/gamemode/gamemode-runtime-resolver.cpp` | map `bodycam_special`/`world_ca_lens` to 5; `config.value.extreme` advisory event (rate-limited to one per distinct value set). |
| `src/gamemode/gamemode.cpp` | remove the lens parser clamps; finite guards only; accept `bodycam_special`/`world_ca_lens`. |
| `src/entities/player-avatar-animations.h` / `.cpp` | per-part tether budgets, live mode/budget overrides, diagnostic actor, settings event. |
| `src/entities/player-animation.cpp` | mode 2 soft tether after every translation layer; extraParts tether; per-part `anim.tether` and `anim.tether.verdict`. |
| `src/entities/aim-commands.cpp` | `anim_root_mode`, `anim_tether`, `anim_tether_probe`. |
| `src/renderer/renderer.h` | `appliedVideoMode()` accessor for the stretch probe. |
| `config/animations/settings.json` | `rootTranslationLimits` (0.005 torso/head, 0.01 arms/legs); mode 1 remains the default. |
| `config/gamemodes/masterlist/gamemode-masterlist.json` | lens block min/max removed; `bodycam_special`/`world_ca_lens` enum values. |
| `config/gamemodes/bloodtest.json` | bodycam lens softened for the HUD-safe bodycam special: vignette 0.35, zoom 1.0, edge vignette, edge darkness 0. |

### Limits

- Mode 5 requires whole-frame composition (`fullscreen 6`). Without it the HUD
  is drawn after PostFX and mode 5 is not claimed as proven.
- Option 1 is the chosen pipeline: the HUD is stretched and CA'd with the world.
  A crisp upscale and a non-CA'd stretched HUD (Option 3) are future work.
- The tether is a translation tether, not full IK. A tight arm budget can pull
  the hand off a weapon; raise only the arm budget if that matters.
- The no-limit migration currently covers only the bodycam lens block. 109
  numeric ranges remain in the gamemode masterlist and are recorded in
  `docs/specs/config-load/config-limits-retirement.md`.

## 2026-11-27 — fullscreen-6 crosshair centre and outline pose alignment

### R11 — crosshair off-centre in fullscreen 6

| | |
|---|---|
| Observation | In fullscreen 6 the physical-aim crosshair was drawn off-centre (for example `x=952.1, y=554.0` against a 960/540 centre). |
| First recorded | 2026-11-27T01:03:09.911Z, run `20261127_010236` |
| Cause | `DebugVis::projectToScreen()` maps NDC into **scene** dimensions when `stretchesWholeFrame()`, but modes with UI after PostFX draw the HUD at **output** dimensions. The projected point is therefore scaled into the wrong space. |
| Fix | `engine-tick-ui-game-hud.cpp` forces `cx=uiScreenW()*0.5f`, `cy=uiScreenH()*0.5f` whenever `stretchesWholeFrame()`, and leaves physical aim in normal windowed modes. |
| Proof | Run `20261127_011550`, generation 748: windowed mode 2 `x=960,y=540`; fullscreen 6 mode 2 `x=960,y=540` (`hud_space=output`, `forced_center=true`); fullscreen 6 mode 5 `x=512,y=384` (`hud_space=scene`). All `hud.crosshair.verdict.matches=true` with `dx=dy=0`. |

### R12 — outline/body pose split (jitter between old and current position)

| | |
|---|---|
| Observation | Friendly/enemy green/red outlines jittered between the actor's previous and current position every tick. |
| Cause | The healthy all-skin **body** used `drawCombinedBodyParts()`, which built matrices from raw `part.worldTransform`. The **outline** uses `outlinePass=true`, so the module disables the combined path and the shell uses `playerRenderPartTransform()` (previous→current interpolation). The two paths therefore drew different poses. |
| Fix | `playerRenderPartTransform()` is now shared; `drawCombinedBodyParts()` calls it when `combinedBodyInterpolate()` is true (default). `player_combined_pose 0` restores the raw path for A/B proof only. |
| Proof | Run `20261127_011757`, generation 748. With `combined_interpolate=false`: `player_visuals.outline_pose_verdict` `max_mismatch_m=7.9176` and `5.6871`, `matches=false`. With `combined_interpolate=true`: `max_mismatch_m=0.0`, `matches=true` (for example 2026-11-27T01:20:47.818Z). |

### Events added/reused

| Event | Purpose |
|---|---|
| `hud.crosshair.center` (extended, on change + 1 Hz) | HUD space, fullscreen mode, lens mode, physical aim, forced centre, dx/dy. |
| `hud.crosshair.verdict` (new) | centre tolerance verdict. |
| `video.fullscreen.mode_applied` (reuse) | proves mode 6 live. |
| `player_visuals.combined_pose.changed` (new) | A/B toggle state. |
| `player_visuals.outline_pose` (new) | raw/interpolated/body positions, `body_mismatch_m`, `raw_vs_interp_m`. |
| `player_visuals.outline_pose_verdict` (new) | 1 Hz max mismatch and `matches`. |
| `player_visuals.outline` (reuse) | draw/skip state and team identity. |
