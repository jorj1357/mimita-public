# Rocket League Reference Baseline

- Timestamp: `2026-11-05T03:31:45Z` (2026-11-04 23:31:45 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD at start: `bb1df89d5a6743205def70507e5e3d911191943e`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope and pre-existing edits

Added the requested Rocket League reference baseline to
`docs/specs/gamemodes/vehicle-arena.md`, section `### 9.1 Rocket League
reference baseline` (starts at line 135). This is a documentation-only change;
no source, configuration, or executable was changed.

The repository already had unrelated edits at session start. They were
preserved: five remote avatar screenshots and `avatar.json`,
`config/analytics.json`, `config/audio/music-settings.json`, the
`devscripts/blender-ai` submodule, `docs/features/vehicles/vehicles.md`,
`docs/specs/vehicles/vehicles.md`,
`docs/architecture/collision/collision.md`,
`docs/specs/creator-tools/video-editor.md`,
`docs/specs/debug-logging/debug-logging.md`,
`docs/specs/entity-editor/entity-editor.md`,
`docs/specs/networking/networking.md`,
`docs/specs/procedural-infinite-world/procedural-infinite-world.md`,
`docs/specs/replay/replay.md`, and untracked
`devscripts/open-dev-loop-dashboard.py`. The Vehicle Arena spec was the only
file edited for this task before this final changelog.

## Exact previous content

The replaced section text was:

> The first arena tuning should match the current Rocket League gameplay feel for:
>
> - arena dimensions and goal size,
> - car acceleration, top speed, boost, jump, and air control,
> - ball radius, mass, restitution, drag, and maximum speed,
> - kickoff positions and reset timing,
> - match length, overtime, and score presentation.
>
> The exact reference values are recorded as a tuning table in this document
> before implementation. They are data, not physics branches. Values may be
> adjusted after playtests, but the reference table is the starting point.

## Exact new content

The replacement is the verbatim section at
`docs/specs/gamemodes/vehicle-arena.md:135-193`, headed `### 9.1 Rocket League
reference baseline`. It preserves table fields for behavior/parameter,
Rocket League value, units, source/build, confidence, and MiMITA comparison.
The table records official hitbox-class/shared-stat facts separately from
community data, including Octane dimensions and offset; car mass, speed,
gravity, arena scale, suspension, steering and tire curves; throttle/brake and
powerslide behavior; jump, dodge, boost, air control and recovery; visible and
collision ball radii; ball collision properties and car-to-ball impulse; and
repeatable kickoff/match conditions. It retains UU values alongside metric
conversions, identifies engine-specific force units, records source.gs build
`25914621` and page update dates, and marks RocketSim `main` as unpinned.
Unknown values and MiMITA measurements remain explicitly open.

## Reasoning and source review

- Epic support is used only for the official claim that there are six hitbox
  classes, cars share stats, and hitbox geometry differs. It is not cited for
  physics constants.
- Community sources are labeled as extracted or reverse-engineered. The Octane
  collision dimensions and base-body offset come from rl.source.gs; vehicle,
  ball, control, and arena constants use rl.source.gs and RocketSim.
- Conflicting meanings are retained rather than collapsed: ball visual radius
  95 UU is separate from Soccar collision radius 91.25 UU; ball and car-ball
  friction/restitution are identified by contact pair.
- MiMITA comparisons were read from `config/vehicles/car_sedan.json` and the
  vehicle specifications. The sedan's 2,600 kg mass, COM, suspension, tire
  inputs, and speed target are listed for context, not treated as a direct
  physics-unit match. Vehicle Arena ball and movement choices remain unset.
- Focused specification review found no spec-code disagreement requiring a
  code change. The work updates desired tuning documentation only; runtime and
  playtest acceptance remain necessary when the mode is implemented.
- Focused skills: `docs/skills/documentation-checker-v1.md` — PASS for route,
  citations, table consistency, and links; `docs/skills/spec-behavior-review-v1.md`
  — PASS_WITH_HUMAN_REVIEW, no behavior implementation changed.
- The documentation TODO scan found no actionable TODO marker. Matches at
  `docs/ROUTER.md:133` and `docs/skills/documentation-checker-v1.md:22` are
  instructional examples describing what to search for, not open TODOs.

## Validation and limits

- `git diff --check -- docs/specs/gamemodes/vehicle-arena.md`: PASS (Git noted
  only the repository's LF-to-CRLF working-copy normalization warning).
- Table structure check: PASS; every baseline row has the same six data
  columns.
- `python devscripts/weapon_json_sync.py --check`: PASS.
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `python devscripts/quality/lint.py --changed`: NOT APPLICABLE; this checkout's
  lint script does not implement `--changed` (it accepts `--all`, `--gate`, and
  `--update-baseline`).
- `python devscripts/quality/lint.py --gate`: FAIL due to
  `damage-authority.findings=82`, above the existing baseline of 81. This
  documentation-only edit does not touch code; unrelated source edits were
  already present at session start. The baseline was not changed.
- `python devscripts/quality/agent-gate.py`: NOT RUN. Inspection showed this
  gate unconditionally stops the dev-loop and may kill its associated build
  process. `python devscripts/dev-loop.py --status --json` showed an active
  server and two clients, so running the gate would disrupt the existing live
  multiplayer session. No build or runtime claim is made.
- Human review: verify that the selected source interpretations and confidence
  labels are suitable as the team's Octane starting baseline. Future arena
  tuning still needs measurements and playtest results recorded against the
  stated conditions.
