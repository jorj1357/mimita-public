# Dev-loop dashboard specification

Time: 2026-11-05 14:33:24 EST (2026-11-05T19:33:24Z)

Branch: `20261011zt`  
HEAD: `a992f9a4`

## Work completed

- Added `docs/specs/dev-loop/dashboard.md` as the owner for Python dashboard
  boundaries, external MiMITA process discovery, verified stop controls, and
  visible control outcomes. It explicitly separates this developer tool from
  the in-game JSON/C++ GUI and specifies reconnecting to the daemon after a
  dashboard restart.
- Updated `docs/specs/dev-loop/multi-instance.md` around “Dashboard
  presentation” and “Start/stop contract” to link the new owner, show external
  processes separately from run-owned instances, and allow stopping only
  verified external MiMITA processes without adopting them into the run.
- Updated `docs/ROUTER.md` lines 75 and 86 to route in-game UI/HUD work to the
  existing `guiv2.md` specification. Added the dashboard route at line 96 and
  included the dashboard spec in the multi-instance route at line 97.

## Exact contract changes

- Router: replaced the missing `docs/specs/gui/gui.md` target with the existing
  `docs/specs/gui/guiv2.md` for in-game UI and objective HUD work.
- Multi-instance: replaced “A process from another run is reported, never
  adopted” with an explicit external-process view. External processes remain
  outside the active manifest and its IDs, but a verified process can receive a
  targeted Stop action; unverified candidates cannot be stopped.
- New dashboard spec: defines identity checks using executable identity and
  role evidence; PID/start-time revalidation before stopping; stable row
  selection across refresh; accepted-versus-completed feedback; journaled
  request/result evidence; and no termination of the launching terminal.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, the dev-loop and multi-instance specs,
  the GUI specs, `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`, the time-format standard, and task
  completion instructions.
- Documentation checker: the new spec has a purpose, audience, and explicit
  boundary; routed GUI and dev-loop targets exist. `git diff --check` passed.
- No build or runtime test was run because this change only edits
  documentation. The reported Stop behavior still needs live dashboard review
  before implementation can be claimed to work.
- Preserved the pre-existing modified submodule `devscripts/blender-ai`.
- No confirmed regression was established; no regression record was added.

## Relevant existing TODO notes

These notes were left untouched per `docs/ROUTER.md`:

- `docs/specs/dev-loop/dev-loop.md:17` — “todo make htis doc better”. Suggested
  follow-up: replace the informal note with an organized section map and clear
  status/ownership labels. This note is why the dashboard-specific contract
  was added as a separate spec instead of editing `dev-loop.md`.
- `docs/specs/dev-loop/dev-loop.md:61` — “2026 10 13 1915 jorj todo -”. It
  does not identify a concrete question; clarify the intended problem before
  expanding it.
- `docs/specs/dev-loop/dev-loop.md:2271` — “2026 10 18 1146 jorj todo
  problmes”. Suggested follow-up: turn the surrounding concerns into distinct
  questions with observable acceptance criteria.
- `docs/specs/gui/gui-general.md:1` — “2026 10 19 1813 jorj todo
  explainnnnnnn”. Suggested follow-up: state whether this file is a scratch
  note or an active overview, then link its purpose to `guiv2.md`.
- `docs/specs/gui/guiv2.md:23` and `:25` — notes request explaining the
  data/command-driven GUI and terminal-first action flow. Suggested follow-up:
  add a concise shared-action section defining how GUI controls invoke the
  same named actions used by terminal commands. These concern the in-game GUI,
  not the Python developer dashboard.

## Pre-existing changes

At task start, `devscripts/blender-ai` was already modified. It remains
untouched. This task changed only the router, dev-loop documentation, and this
changelog.
