# Bloodtest bodycam live lens overlay and coordination findings

Date: 2026-11-23
Agent: `bloodtest-bodycam-runtime`
Issue: `issue-2e7b0cd6f868`

## Scope

Completed a focused extension of the existing Bloodtest bodycam overlay. The
mode parser accepts bodycam lens values, `GamemodeRuntimeResolver` applies and
clears them through the shared `PostFX` owner, and `config/gamemodes/bloodtest.json`
now exposes chromatic aberration, vignette, and distortion for live tuning.
The masterlist documents their bounds. Distortion starts at zero to avoid
introducing edge stretching while the visual profile is still unreviewed.

The earlier bodycam implementation in this work also added mode-owned camera
orientation, eye offset, response times, camera sway and wall-collision settings,
plus hot-reload reapplication. Bloodtest retains the specified 70-degree FOV
and forced first-person values in its existing top-level mode fields.

## Source and validation

- `config/gamemodes/bloodtest.json` and
  `config/gamemodes/masterlist/gamemode-masterlist.json` pass
  `python devscripts/gamemode_json_sync.py --check`.
- Focused `git diff --check` passed for the touched camera, gamemode, post-FX,
  and Bloodtest configuration files.
- Root dev-loop game-DLL attempt
  `game-dll-20261123T191346937240Z-35f07c` built generation 570 successfully;
  all 67 submitted translation units completed and the DLL was published.
- Root dev-loop EXE artifact
  `2026-11-23T19:22:15.804284Z:2e24d312603e4135cdd9be0d870a11ce4be98042e253296de6ca16ca189d512b`
  built and launched in mode 51 (`[2 CLIENTS] Bloodtest - Free for all`) on
  `mimita-duels-map-v3`, run `20261123_192224`.
- The client journal records module-host startup, Bloodtest connection and
  repeated authoritative snapshot application. It does not identify the
  bodycam overlay values or prove the lens/camera appearance was applied.

## Limits and open work

No visible game window or captured screenshot was available for visual
acceptance. The documented `screenshot_at_tick` terminal command returned
`terminal.command.unknown`; the observation was appended to screenshot issue
`issue-89efbbc86535`. Do not treat the build or network snapshots as proof of
first-person framing, camera wall collision, post-FX appearance, or successful
live tuning.

The separate head, torso, arm and held-weapon smoothing controls required by
`docs/specs/gamemodes/bodycam.md` are not implemented by this slice. Camera and
arm alignment under mouse look, weapon muzzle alignment, close-wall behavior,
and live JSON reload still need matching runtime diagnostics and screenshot
review. TDM rules, projectile weapons, NPC angle holding, gore and the broader
Bloodtest feature set were not implemented here.

The optional presentation-DLL refresh during launch failed, but the dev-loop
continued to a successful EXE build and the runtime started. The run’s
`module.local.loaded` reports local module generation 1; no journal event ties
that module number to game-DLL generation 570, so that exact DLL-to-runtime
identity remains unproven.

## Coordination and efficiency

Created response-size issue `issue-a8772368be34` for routine coordination calls
returning the full shared brain. Evidence showed 169 registered agents, 303
open issues, numerous context/proof/conflict records, and routine responses
exceeding 200,000 tokens. This establishes excessive returned payload size,
not CPU or memory pressure. The issue proposes compact routine views with an
explicit full export path.

The existing issue-review scheduler runs on activation/work-session events,
after five completed sessions or a 30-minute eligible interval. It dispatches
bounded evidence reviews; it is not a continuously running AI process and it
does not itself seek consensus or close issues. The convergence gap and a
measurable proposal are recorded in issue `issue-91e3384b219b`; a scheduled
review wake was observed as launched, but not completed in this session.

The build’s compiler queue wait reached 188,537 ms during header fan-out. This
observation was appended to existing wait-latency issue
`issue-7d3dcfd7433b`, rather than creating a duplicate per-build issue.

Specification review used `docs/skills/spec-behavior-review-v1.md`. Runtime
proof and human visual acceptance remain open.
