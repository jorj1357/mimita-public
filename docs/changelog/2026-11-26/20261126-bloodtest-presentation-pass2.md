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

## Pass 3 (2026-11-26): four live-switchable CA lens modes and per-mode proof

### Change

- **Four exclusive lens modes:** added `LensMode` resolution in `PostFX`
  (`mixed` 1, `world_only` 2, `normal` 3, `world_and_ui` 4). The live command is
  `lens_mode <1|2|3|4|config>`. `full_world` maps to `world_only` and
  `background_only` to `mixed`; Bloodtest now defaults to `world_only`.
- **Mode application:** `PostFX::render()` zeroes the CA uniform for mode 3,
  sets background-only masking for mode 1, and disables the bodycam distortion
  crop for modes 3/4 so the HUD stays visible. The HUD shader CA strength is set
  to `0.0` because the single PostFX pass owns CA.
- **UI routing:** `engineTickRenderFrame()` computes `uiInScene` from the lens
  mode. Mode 2 draws UI after PostFX (crisp); mode 4 draws UI into the scene so
  it receives CA; modes 1/3 follow the configured scene layer. Terminal/HUD is
  drawn once.
- **Proof tooling:** `presentation_pattern <frames>` draws a dense 7x9
  achromatic world grid and achromatic UI bars. `presentation_probe <tag>
  [mode]` applies the requested mode plus the pattern and captures two frames
  later in one atomic request, then emits eleven `presentation.*` events and a
  PNG. The pixel metric is channel-edge misalignment, not raw `abs(r-b)`.
- **Regression record:** `docs/regressions/presentation/` and the Bloodtest
  spec lens-mode section were updated.

### Evidence

- **Final run `20261126_175452`, generation 714, launch mode 61, fullscreen
  video mode 6, map slaughterhouse1, client PID 32636.** No
  `game.generation.activated` event occurred during the probe sequence.
- Each probe reported `mode_applied=true` and `matches=true`:
  - 2026-11-26T17:55:25.025Z mode 1 `mixed`: world dark `0.06281`, world
    bright `0.00591`, UI `0.00083`.
  - 2026-11-26T17:55:26.980Z mode 2 `world_only`: world dark `0.08570`, world
    bright `0.03243`, UI `0.00069`.
  - 2026-11-26T17:55:28.633Z mode 3 `normal`: world dark `0.0`, world bright
    `0.0`, UI `0.00088`.
  - 2026-11-26T17:55:30.378Z mode 4 `world_and_ui`: world dark `0.09809`,
    world bright `0.11181`, UI `0.03857`.
- Screenshots: `build/presentation-probes/final{1,2,3,4}-mode{1,2,3,4}-*.png`;
  UI crops `evidence-mode2-ui-clean.png` and `evidence-mode4-ui-ca.png`.
- Builds: generation 711 `game-dll-20261126T174647924868Z-2feedb` (atomic
  probe), 713 `game-dll-20261126T175102072132Z-c0898f` (dense pattern), 714
  `game-dll-20261126T175409817450Z-dd66fb` (verdict calibration).

### What failed and was corrected

- Raw `abs(r-b)` scored natural scene colour; replaced by channel-edge
  misalignment on an achromatic pattern.
- Seven bars did not fill the probe window; replaced by a dense grid, which
  dropped the mode-3 baseline from `0.00617` to `0.0`.
- A separate `lens_mode` then `presentation_probe` pair lost the in-DLL
  override when generation 710 activated mid-run at 2026-11-26T17:38:59Z; the
  probe is now atomic.
- The UI test bars used one guessed y origin; they are now drawn at both.
- Probe PNGs were bottom-up; rows are flipped before encoding.
- Verdict thresholds were overfit; they are calibrated with wide margins.

### Limits

- Mode 4 is proven only under whole-frame composition (fullscreen video mode
  6). A windowed launch has no UI CA; the verdict fails by design until an
  output-resolution UI CA pass exists.
- The CA strength is the existing bodycam value (`0.8`), so mode 4 produces a
  strong radial split on thin HUD text. Final art tuning and human visual
  acceptance are still required.

## Pass 4 (2026-11-26): bodycam special mode 5, animation mode 2 tether, stretched 6

### Change

- **Lens mode 5 `bodycam_special`:** whole-frame 1024x768 presentation. World
  and HUD render into the 1024x768 scene target, PostFX applies full-world CA
  (when enabled), fisheye, vignette and peripheral blur, and the whole frame is
  stretched to the display. `lens_ca on|off` toggles CA. The chosen pipeline is
  Option 1; Options 2/3 are documented but not implemented.
- **Mode 2 clean-up:** mode 2 (`world_only`) now has no fisheye, vignette or
  blur. Modes 3/4 also have the lens off. Mode 1 and mode 5 keep the bodycam
  lens.
- **Fullscreen 6:** `fullscreen 6` is the borderless full-frame 1024x768
  stretched mode. It applies live from the console and is not the global
  default. The first pass is intentionally soft; a sharpening pass is future
  work.
- **No-limit lens config:** removed `min`/`max` from the
  `presentation.bodycam.lens` fields in the gamemode masterlist and removed the
  matching `setCameraLensDistortion` clamps. `config_limits_probe` emits the
  inventory and policy; `config.value.extreme` records out-of-old-band values.
  The full no-min/max migration is specified in
  `docs/specs/config-load/config-limits-retirement.md`; 109 ranges remain for
  later batches.
- **Animation mode 2:** redefined from "zero all part translations" to a smooth
  `budget*tanh(len/budget)` limb tether applied after every translation layer.
  Defaults: 0.005 m torso/head, 0.01 m arms/legs. `anim_root_mode 2` and
  `anim_tether <part> <m>` are live. Global mode 2 wins over per-clip overrides.
  `anim_tether_probe` emits per-part `anim.tether` and `anim.tether.verdict`.
- **Proof tooling:** dense achromatic UI checkerboard; mode-5 lens verdict;
  vignette/fisheye pixel events; `presentation_stretch_probe` for fullscreen 6.

### Evidence

- **Run `20261126_231556`, generation 738, launch mode 61, map
  slaughterhouse1.** `fullscreen 6` applied live at 23:16:26.290Z: scene
  1024x768, output 1920x1080, stretch `1.875 x 1.40625`, whole-frame true.
- **Mode 5 CA on** 23:16:32.453Z: `matches=true`, world dark `0.07090`, world
  bright `0.03469`, UI pattern `0.02406`, corner darkening `0.6651`, edge
  falloff `0.6154`.
- **Mode 5 CA off** 23:16:34.344Z: `matches=true`, world bright `0.00050`, UI
  pattern `0.00298`, vignette/fisheye unchanged.
- **Mode 2 clean** 23:16:37.730Z: `matches=true`, world bright `0.11188`, UI
  pattern `0.00551`, lens vignette `0` and distortion `0`.
- **Animation mode 2** 23:16:39.624Z onward: `matches=true`, max out `0.01 m`,
  max budget `0.01 m`, over-budget `0`. Per-part raw -> out: arm `1.155 ->
  0.01`, head `0.154 -> 0.005`, torso `0.063 -> 0.005`.
- **Stretch probe** 23:16:28.731Z: `matches=true`.
- **Config inventory** 23:16:27.382Z: 109 min / 33 max tokens; nine lens fields
  removed.
- **Screenshots:** `build/presentation-probes/proof5on-mode5-*.png`,
  `proof5off-mode5-*.png`, `proof2-mode2-*.png`.

### What failed and was corrected

- Mode 5's initial hard circular lens (`edge_mode=circle`, radius 0.05,
  darkness 0.95, zoom 2.2) crushed the HUD. Softened to a vignette edge,
  radius 0.72, darkness 0, zoom 1.0, vignette 0.35.
- The original UI bars were too flat and the sample region missed them. Replaced
  with a dense 12 px checkerboard and top/bottom quarter sampling.
- The mode-5 CA-off UI threshold was below the stretched checkerboard's natural
  resampling floor (0.00416). Raised the CA-off bound to 0.012 and the CA-on
  bound to 0.02.

### Limits

- Mode 5 is proven only under `fullscreen 6`; Option 1 means the HUD is
  stretched and CA'd. Crisp upscale and non-CA'd stretched HUD are future work.
- The animation tether is a translation tether, not full IK; a tight arm budget
  can pull the hand off a weapon.
- No-limit migration currently covers only the lens block.
- **No-limit acceptance:** run `20261126_232129`, generation 741, with a
  temporary `vignette: 0.9` (old max 0.8) reported
  `gamemode.bodycam.applied.lens.vignette=0.9` and
  `config.value.extreme {path: presentation.bodycam.lens, values:{vignette:0.9}}`;
  the mode-5 probe still passed. Vignette restored to 0.35 afterward.

## Pass 5 (2026-11-27 UTC): fullscreen-6 crosshair centre and outline pose alignment

### Change

- **Crosshair:** in fullscreen 6 (`stretchesWholeFrame()`), the crosshair is
  forced to the active HUD centre (`uiScreenW()*0.5f`, `uiScreenH()*0.5f`) and
  the physical-aim offset is skipped. Normal windowed modes still use physical
  aim. `hud.crosshair.center` now relogs on HUD-space/fullscreen change and
  `hud.crosshair.verdict` reports the centre tolerance.
- **Outline pose:** the combined body path and the outline/per-part paths now
  share `playerRenderPartTransform()` (previous→current interpolation). The
  combined path previously used raw `part.worldTransform`, so the outline
  trailed the body by one interpolation fraction. `player_combined_pose <0|1>`
  is a dev A/B switch; default 1 is the fix.
- **Diagnostics:** `player_visuals.outline_pose` and
  `player_visuals.outline_pose_verdict` report raw, interpolated and body
  positions, `body_mismatch_m`, `raw_vs_interp_m`, and a 1 Hz alignment verdict.

### Evidence

- **Crosshair:** run `20261127_011550`, generation 748. Windowed mode 2:
  `x=960,y=540`. Fullscreen 6 mode 2: `x=960,y=540` with
  `hud_space=output`, `forced_center=true`. Fullscreen 6 mode 5:
  `x=512,y=384` with `hud_space=scene`. All verdicts `matches=true`, `dx=dy=0`.
- **Outline:** run `20261127_011757`, generation 748, TDM NPC.
  `player_combined_pose 0`: `max_mismatch_m=7.9176` and `5.6871`,
  `matches=false`. `player_combined_pose 1`: `max_mismatch_m=0.0`,
  `matches=true` (for example 2026-11-27T01:20:47.818Z).

### Build note

The first full build after these edits linked with undefined
`PlayerAiController`/`ClientCollisionWorldView` symbols because their cached
objects were missing even though the source files existed. The affected TUs
were forced back through the cache with a documented one-line comment; no
functional code changed. Generation 748 then published successfully.

## Pass 6 (2026-11-27 UTC): bounded jitter probe

### Change

- Added `src/debug/jitter-probe.{h,cpp}` and `jitter_probe <seconds>` /
  `jitter_probe_stop`. The probe samples four categories and emits
  `jitter.probe.started`, one `jitter.window` per second, and a final
  `jitter.verdict`:
  - `interp`: remote interpolation alpha delta, final render delta, hard snaps,
    underruns, holds, buffer depth;
  - `actor`: per-part rendered second difference and raw-vs-interpolated lag;
  - `camera`: camera position and Euler-angle second difference;
  - `hud`: crosshair centre error when a forced centre is active.
- Integrated the probe into `multiplayer-interpolation.cpp`, `render-player.cpp`,
  `engine-tick-camera.cpp`, `engine-tick-ui-game-hud.cpp`, and
  `engine-tick.cpp` (`endFrame`). Off by default; no simulation/render state is
  changed.

### Live alpha investigation

`animationRenderAlpha()` is already set in the live fixed-tick path at
`engine-tick-replay.cpp` (`setAnimationRenderAlpha((float)simClock.alpha())`), so
the earlier hypothesis that live alpha stays at 1.0 was not the cause. The probe
must identify the real jitter source from the four measured categories.

### Baseline evidence

- Run `20261127_054945`, generation 777, mode 61, fullscreen 6.
- `jitter_probe 10` produced ten `jitter.window` events and a
  `jitter.verdict` at 2026-11-27T05:51:22.692Z.
- Idle/local baseline: actor accel max `0.00248 m`, raw lag max `0.00061 m`,
  camera angular accel `0.0`, HUD samples `0` (crosshair path owned by another
  active agent at the time).
- Interp samples `0` because the run had no remote actors; a team-mode run with
  NPCs is still needed for the network/interp baseline.

### Coordination

`hud-fs6-crosshair` was editing `engine-tick-ui-game-hud.cpp` concurrently. Their
new crosshair event format passes `forcedCenter=false` to
`JitterProbe::sampleHud`, so HUD samples are zero. A bounded question was sent;
this file is not edited further by this agent. The probe's HUD sample can be
re-enabled by passing the actual forced-centre value from the owner.

## Pass 7 (2026-11-27 UTC): agent process autonomy and context handoff

- **AGENTS.md:** added `AGENT PROCESS AUTONOMY` (agents may start as many
  checkout-owned MiMITA EXEs / isolated test runs as a task needs, with root
  dev-loop authority, recorded runs, and no interference with other agents),
  `QUEUED WORK IS AUTONOMOUS` (continue through queued items without per-item
  confirmation), and `CONTEXT HANDOFF` (write a handoff, wake a fresh agent,
  leave a buildable tree).
- **New spec:** `docs/specs/dev-loop/agent-context-handoff.md` defines the
  trigger (~70% context or near-full model report), required handoff fields,
  wake mechanism, tree-state rules, and acceptance.
- **New handoff:** `docs/handoffs/20261127_jitter-body-ads-damage-handoff.md`
  covers jitter, per-avatar body sockets, ADS, damage modes, bat viewmodel, NPC
  shooting, and slope dot, with file ownership and the active
  `hud-fs6-crosshair` / Rocket League constraints.
- **Issues:** `issue-44bdee5f2121` process autonomy and
  `issue-f6c167ef94b9` context handoff (created 2026-11-27T06:04Z).
- **Continuation:** a fresh `general` agent (`ses_de4649f6effes4k26beVQUwzwX`)
  was woken with the handoff and owns the continuation.

## Pass 8 (2026-11-27 UTC): build why/postmortem and top-level handoff chat

- **Build notes:** `build-note.py` now carries `--why`, `--header-fanout`,
  `--expected-seconds`, `--risk`, and `--proof`; missing `--why` is recorded as
  `why="missing"` and flagged by the postmortem.
- **Automatic postmortem:** the dev-loop adds `postmortem` to the build summary
  and `dev-loop.game-generation.ready`: why, phase seconds, queue wait,
  slowest TUs, cache, CPU/memory pressure, suggestions, and metrics to watch.
  Spec: `docs/specs/dev-loop/build-why-and-postmortem.md`.
- **Example (gen 782):** 603s total, 588.7s compile, 568s max queue wait, CPU
  99.8%, 49 MB free memory, `player.h` changed without a fanout record. The
  postmortem suggests recording fanout, inspecting the top slow TUs, and
  reducing resource contention.
- **Context handoff:** the continuation is now a **new top-level OpenCode chat**
  via `agent-wake.py --adapter opencode` (`wake-9ad211184399`, agent
  `opencode-deepseek-v4-2`, child PID 27560), not a subagent. The wake template
  no longer says "do not edit without human approval"; it points at AGENTS.md
  and the build-approval protocol.
- **Handoff:** `docs/handoffs/20261127_jitter-body-ads-damage-handoff.md` now
  records the partial body-socket/rig-editor work at gen 782, the build
  approval protocol, the postmortem contract, and the new chat identity.
