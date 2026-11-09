# Single dev-loop dashboard instance

- Local timestamp: 2026-11-09 10:44:36 EST.
- Canonical UTC timestamp: 2026-11-09T15:44:36.230Z.
- Branch: `checkpoint/pre-live-kernel-20261108`.
- HEAD at start: `95c55bd00232e09253f31224e7459c9069eeb702`.
- Scope: make dev-loop command entry points reuse one dashboard and prevent
  simultaneous dashboard processes/windows for this checkout.

## Finding and change

The command path called `ensure_running()` with its default
`open_dashboard=True`, so command entry points intentionally tried to open a
dashboard. `dashboard_pid()` used a PID marker/process scan without serializing
startup, allowing concurrent command processes to decide that no dashboard
existed before a newly launched one registered.

In `devscripts/dev-loop.py`:

- `dashboard_pid()` now verifies the process command line against this checkout
  before trusting a PID marker, and process discovery includes `python.exe` and
  `pythonw.exe` dashboard entry points.
- `start_dashboard_detached()` serializes startup using a repository-local OS
  file lock, rechecks for an existing view, and waits for the new dashboard to
  register. A dashboard started by a command uses `--no-terminal-relaunch`,
  keeping the native dashboard in one process instead of creating a terminal
  host process too.
- `run_dashboard()` holds a separate repository-local OS lock for its lifetime.
  If another entry point reaches the dashboard while it is already open, that
  entry point exits without creating another view.
- The existing dashboard refreshes daemon status and recent journal activity
  in place. Its recent activity already formats each event with the journal's
  UTC ISO 8601 `wall_time`, including command request text and result/reason.

The prior dashboard contract said AI command calls never opened a dashboard.
The user clarified the intended behavior: reuse the existing dashboard, or
open one if the checkout has none. `docs/specs/dev-loop/dev-loop.md` now states
that contract and gives command transcript timestamps in UTC ISO 8601 form.

## Exact behavior wording

Old: `AI command calls never open a dashboard.`

New: `AI command calls reuse the existing dashboard or start one if this
checkout has no dashboard process.`

Old command transcript format: `YYYY-MM-DD HH:MM:SS UTC | AI | target
instance | command | result`.

New command transcript format:
`YYYY-MM-DDTHH:MM:SS.sssZ | AI | target instance | command | result`.

## Documents and focused review

- Read `docs/specs/dev-loop/agent-lanes-build-authority.md` before editing;
  its repository-local lane path is not implemented, so no agent lane or build
  was started.
- Read `docs/ROUTER.md`, `docs/specs/dev-loop/dashboard.md`,
  `docs/specs/dev-loop/multi-instance.md`, `docs/specs/dev-loop/dev-loop.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The prior
  spec/code disagreement was corrected to the user's stated behavior; process
  count and on-screen refresh still need runtime observation.
- `docs/skills/documentation-checker-v1.md`: PASS_WITH_HUMAN_REVIEW. Relevant existing
  TODOs left unchanged per router instructions:
  - `docs/specs/open-gameplay-issues.md:35`: “todo make it so dev loop launchgin
    that just 1 single gui, make th gui like its own thing not part of dev-loop”
    — replace with an explicit ownership statement that the dashboard is a
    separate singleton view over daemon status and journal data.
  - `docs/specs/gui/guiv2.md:25,27`: asks for a terminal-first, shared-action
    explanation — document commands as action owners and GUI controls as
    callers of those same actions.

## Validation and remaining evidence

- Python AST syntax parse for `devscripts/dev-loop.py`: passed.
- `git diff --check` scoped to `devscripts/dev-loop.py` and
  `docs/specs/dev-loop/dev-loop.md`: passed (Git reported only a line-ending
  conversion notice for the pre-existing modified spec file).
- A repository-wide `git diff --check` reports trailing whitespace in the
  pre-existing modified `docs/specs/open-gameplay-issues.md`; those lines were
  not changed in this session.
- No build or live dev-loop launch was run. The mandated agent lane cannot yet
  be used, and starting a daemon could trigger its automatic build. The
  one-window/process behavior still needs observation in Task Manager and the
  dashboard after concurrent command invocations.
- No human visual review was performed.

## Pre-existing workspace edits preserved

The checkout already contained unrelated modifications before this session,
including changes in `devscripts/dev-loop.py` and
`docs/specs/dev-loop/dev-loop.md`. In particular, the existing repository-local
room-code temporary-file change and lane-policy documentation edits were
preserved. Other pre-existing changed paths were `AGENTS.md`, `build.py`,
`config/analytics.json`, `devscripts/agent-lane.py`, `devscripts/blender-ai`,
`devscripts/build_scheduler.py`, `docs/architecture/live-development/live-development.md`,
`docs/operations/build-and-exe/agent-lane-integration-and-pending-validation.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/specs/debug-logging/debug-logging.md`,
`docs/specs/dev-loop/agent-lanes-build-authority.md`,
`docs/specs/dev-loop/multi-instance.md`, and
`docs/specs/open-gameplay-issues.md`. The existing untracked changelog
`20261109_151417-agent-repository-local-workspace-policy.md` was preserved.
