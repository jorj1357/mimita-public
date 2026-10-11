# Heavy shotgun plan

- EST session date: 2026-10-10; canonical UTC timestamp: 2026-10-11T02:09:51Z
- Branch: detached/unspecified in this checkout (`git branch --show-current` returned empty)
- Result: PASS_WITH_HUMAN_REVIEW; planning/documentation only

## Change made

Created `docs/jorj-docs/20261010plan.md` with a staged plan for a distinct
`heavy_shotgun` weapon, Juggernaut4 map/performance validation, asset/audio
registration, authoritative pellet damage and knockback, bright muzzle/tracer
presentation, and a recoil ownership redesign.

The prior file was an untracked user draft containing the same request. It was
replaced with a structured plan; no gameplay source, active configuration, or
user-provided asset was changed by this session.

## Evidence and reasoning

- Read `AGENTS.md` and `docs/ROUTER.md`.
- Read the routed weapons, networking, performance, effects, JSON configuration,
  live-development, hot-audio, runtime-validation, asset, build, completion,
  spec-review, asset-checker, and efficiency-checker documents listed in the
  plan.
- Inspected the active `config/weapons.json`, `config/weaponsets.json`,
  `config/gamemodes/juggernaut.json`, `config/weapon-tracers.json`, recoil
  owners under `src/combat`, `src/camera.cpp`, and the available heavy-shotgun
  model/WAV files.
- Confirmed `juggernaut4.glb` exists, but the active Juggernaut map list does
  not currently include `juggernaut4`.
- Confirmed the heavy-shotgun model and three WAVs exist as untracked files;
  they remain untouched and are identified as pre-existing user inputs.
- Confirmed the repository hard rule that gameplay remains fixed at 60 Hz and
  VSync stays off; 240 FPS is therefore specified as a measured render target.
- Confirmed current recoil is split across weapon recoil accumulation/decay,
  shooter impulse, camera punch, and legacy paths, so the plan requires an
  ownership decision before tuning.

## Validation

- Plan file exists and contains 256 lines.
- `git status --short` shows the plan plus the pre-existing modified files and
  untracked heavy-shotgun assets.
- No build, executable launch, runtime scenario, network test, or human visual
  acceptance was performed because the user asked whether this could be done
  and requested a plan.

## Human review still needed

Before implementation, decide whether 35 damage means per confirmed pellet,
whether the gun belongs to the actual Juggernaut or Fighters, which exact
dynamic-light fields “2x bigger” applies to, and the named 240 FPS benchmark
conditions/percentile target.

`docs/regressions/regressions-v1.md` is missing from the current authority
location and exists only at `docs/regressions/archive-ignore/regressions-v1.md`;
the archived copy was not treated as current authority.
