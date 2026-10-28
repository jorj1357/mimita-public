# Outline alignment, animation priority diagnostics, performance evidence, and contact lights

Time created: 2026-10-28T01:55:11Z
Time last updated: 2026-10-28T01:55:11Z

Branch: `20261011zt`
Starting commit: `6cb5d37e`
Completion commit: none (changes left uncommitted for review)

## Request

Implement the approved four-part plan:

1. Fix TDM teammate/enemy outlines.
2. Add performance/logging evidence that proves the current baseline and the
   original failure mode.
3. Improve animation sequencing/priorities and prove dash behavior with a
   chainsaw.
4. Make melee contacts bigger and add dynamic contact lights.

## Documents and skills read

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/animations/animations.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/gamemodes/teamdeathmatch.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/skills/logging-checker-v1.md`

## Findings

### 1. Giant TDM red/green sheets were stale interpolated poses

The TDM configuration already requested outlines with wireframes disabled, but
the renderer interpolated `previousWorldTransform` across a respawn/teleport
without validating the step. Runtime evidence from
`logs/2026-10-28/20261028_014618/events.jsonl`:

- actor `200005` (`monsterstabber`)
- `bounds_max_offset=1.153`
- `bounds_max_previous_offset=255.648`
- `camera_distance=75.242`

The outline shell stretched triangles from the stale previous pose to the
current pose. The fix snaps to the current transform when the previous-to-current
step exceeds a plausible fixed-tick movement. The outline/wireframe paths now
also log and reject non-finite or oversized geometry.

### 2. TDM team identity was already correct; the layer was not the issue

`player_visuals.outline` reported `teammate=false`, known team identity, and
`drawn=true`. The problem was geometry interpolation, not unknown team identity.

### 3. Animation priority was observable but dash clips are missing on several avatars

`anim.dash.clip_missing` appeared for avatars whose animation set has no dash
clip. The local chainsaw dash test selected `dash` with
`mimita_sizing_6_dash` and `dash_timer=1.0`, so the shared state selection can
play a dash while holding a chainsaw. The debug path also proved candidate
ordering and priority values.

### 4. Performance was being measured through excessive debug output

The diagnostic-heavy run `20261028_014759` reached the 100 MB journal ceiling
and showed p99 about 36 ms. The clean run `20261028_015330` used about 15 MB in
45 seconds and showed recent p95 about 1.95 ms and p99 about 6.29 ms. The
difference is the debug-only `anim.priority` stream (18,278 state changes in the
diagnostic run), not normal gameplay.

## Changes

### Outline and player-visual diagnostics

- `src/entities/player-render.cpp`
  - `renderPartTransform()` now rejects a previous-to-current step larger than
    `8.0f` metres and uses the current transform instead.
  - `drawCombinedBodyParts` accepts `outlinePass` and applies front-face culling
    on that path, matching the per-part outline path.
- `src/render/render-player.cpp`
  - added `ActorVisualBounds` and `computeActorVisualBounds()`: part origins,
    previous origins, world extents from batch bounds, and maximum part scale.
  - added `player_visuals.actor` bounded diagnostics (debug or one-shot dump).
  - added `outline.rejected` and expanded `wireframe.bounds_rejected` fields.
  - added layer filters and one-shot dump state.
- `src/render/render-player.h`
  - exposed `setPlayerVisualDebugEnabled`, `playerVisualDebugEnabled`,
    `setPlayerVisualLayerFilter`, `playerVisualLayerFilter`, and
    `requestPlayerVisualDump`.
- `src/terminal/terminal-debug-toggles.cpp`
  - `outline_debug [0|1]`
  - `outline_dump`
  - `outline_only [0|1]`
  - `wireframe_only [0|1]`
  - `body_only [0|1]`
  - `player_layers`
- `config/gamemodes/tdm.json`
  - confirms `player_outlines=true`, `friendly_outlines=true`,
    `player_wireframes=false`, `friendly_wireframes=false`.
- `config/playervisuals.json`
  - teammate point light disabled and its height offset corrected from `-91.0`
    to `1.0`.
- `docs/specs/gamemodes/teamdeathmatch.md`
  - TDM now specifies outlines, with wireframes reserved for debug.

### Animation priority and dash hardening

- `src/entities/player-animation.cpp`
  - added `anim_priority_debug` state and `anim.priority` records with all
    candidates, eligibility, clip presence, timers, locks, weapon, ground
    state, and velocity.
  - `anim.state.changed` now includes `selected_state`, `selected_priority`,
    and the candidate list.
  - dash/downDash use `"dash"`/`"downDash"` fallback names and a short
    `0.15s` window when the avatar set has no clip; missing clips log
    `anim.dash.clip_missing`.
- `src/entities/player.h`
  - declarations for `setAnimationPriorityDebug` / `animationPriorityDebug`.
- `src/terminal/terminal-debug-toggles.cpp`
  - `anim_priority_debug [0|1]`.

### Performance evidence

- `src/perf/perf.cpp`
  - one `performance.baseline` per process with build ID, PID, run ID, journal
    path, frame cap, VSync state, and starting counts.
  - `PERFORMANCE_FRAME` now includes logger totals/deltas and flush time.
  - `performance.combat-growth` every five seconds with NPC/effect/blood/corpse
    counts, p99, allocation counters, and logger growth.
- `devscripts/perf-evidence.py`
  - reads a journal and prints process mix, process audit, baseline, recent
    p95/p99, logger growth, combat growth, and spike contributors.

### Melee contact size and light

- `config/hitfx.json`
  - `meleeContact.baseDiameter` 0.5 -> 0.75
  - `meleeContact.maxDiameter` 1.0 -> 1.5
  - `meleeContact.radiusScale=1.25`
  - new `meleeContact.light` block: color, max color, intensity, radius,
    lifetime 6 ticks, retrigger 6 ticks.
- `src/effects/hit-effects.h`
  - `MeleeContactLightConfig`, `radiusScale`, and `weaponRadiusScale`.
- `src/effects/hit-effects-config.cpp`
  - parse `radiusScale` and `light`.
- `src/effects/hit-effects-spawn.cpp`
  - scaled sphere/spark presentation, one retriggered `DynamicLightManager`
    light per contact, and light fields in `melee.contact.fx`.
- `src/combat/weapon-contact-local.cpp`,
  `src/network/confirmed-damage-presentation.cpp`
  - pass per-weapon `contactRadiusScale`.
- weapon configs:
  - `chainsaw=1.35`, `mace=1.6`, `swordsword=1.2`.

## Exact evidence

### Build

- Build `3545` succeeded after the first implementation slice.
- Build `3549` succeeded after the stale-pose guard.
- Build `3550` succeeded after the final header/comment cleanup.
- Dev-loop reported `source_current=true` for build `3550`.

### Checks

- `python devscripts/animation_json_check.py`
  - `clips=116 avatar_profiles_checked=34 errors=0`
- `python devscripts/weapon_json_sync.py --check`
  - `masterlist=1 definitions=34 errors=0`
- `python -m py_compile devscripts/perf-evidence.py devscripts/dev-loop.py`
  - passed.
- JSON parse check for hitfx, player visuals, TDM, and three melee weapon
  configs passed.

### Runtime

Clean run `logs/2026-10-28/20261028_015330/events.jsonl`, build `3550`:

- journal size about 15.2 MB after 45 seconds
- latest 10 windows: p95 average 1.954 ms, p99 average 6.290 ms
- maximum simulation 9.295 ms
- `performance.baseline` recorded `vsync=false`, `target_fps_max=999`
- `performance.combat-growth` emitted every five seconds

Diagnostic run `logs/2026-10-28/20261028_014759/events.jsonl`, build `3549`:

- `anim.priority` selected `dash` with `mimita_sizing_6_dash` while the
  equipped weapon was `chainsaw`, `dash_timer=1.0`.
- `outline.rejected` recorded `max_previous_offset=255.648` in the earlier
  run, proving the stale pose.
- `melee.contact.fx` recorded `diameter=1.505`, `radius_scale=1.25`,
  `light_spawned=true`, `light_intensity=3.014`.
- `weapon.contact.result` recorded shared blade contact and a kill.
- This run also produced 18,278 `anim.state.changed` events because
  `anim_priority_debug` was enabled; it reached the 100 MB journal ceiling.
  That is debug-only overhead, not the normal baseline.

### Versioninfo

`.dev/current/mimita.exe --versioninfo` reported:

`EVENTS_JSONL_PATH=logs/10-27-2026/20261027_215441/events.jsonl`

`RUN_ID=20261027_215441`

## Pre-existing edits

- `config/analytics.json` was changed by the running game
  (`launch_count` 2660 -> 2662). This is runtime-generated state, not part of
  the implementation.
- No commit was created.

## Regression

New record:
`docs/regressions/2026-10-28/stretched-teleport-outline-REG.md`

Status: `ATTEMPTED FIX (1)`. The stale previous pose is confirmed by journal
evidence. Human visual acceptance of the corrected outline remains pending.

## Human review still required

- Confirm TDM outlines now hug the actor and read clearly as green teammate /
  red enemy at normal play distance.
- Confirm dash with chainsaw visibly plays torso/legs and does not fall back to
  idle.
- Confirm the larger contact shape and dynamic light feel right in combat.
- Confirm the clean performance baseline is acceptable in normal play.
- Confirm no debug command remains enabled in a normal session.
