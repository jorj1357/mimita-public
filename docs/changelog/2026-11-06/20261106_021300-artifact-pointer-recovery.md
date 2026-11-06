# Dev-loop latest artifact recovery

- Time: 2026-11-06T02:13:00Z (2026-11-05 21:13:00 EST)
- Branch: `20261011zt`; no commit made.
- Result: `PASS_WITH_HUMAN_REVIEW` — daemon recovery and dashboard launch
  verified. The stable-path retry is source-only until the live daemon can be
  safely restarted; visual/gameplay acceptance remains open.

## Cause and changes

The newest failed publication left an incomplete timestamped directory under
`.dev/builds/`. Dev-loop startup selected the newest directory without proving
that its executable and identity sidecar were valid, then crashed reading the
missing `build-identity.json`. The watchdog repeatedly restarted the daemon,
leaving the dashboard button with no control endpoint. The compiler's earlier
publication attempt had also rejected an invalid linker output (`Nothing
changed` followed by an invalid DOS header / file too small).

Changed `devscripts/dev-loop.py`:

- `publish_build()` (around line 953): remove a partially created artifact
  directory on publication exceptions and reject an invalid copied executable
  before writing the identity sidecar.
- `DevLoop.__init__()` (around line 1082): select the newest verified artifact
  using `newest_valid_published_build()` rather than the newest directory of
  any condition. Older valid builds remain launchable after failed publication.
- `activate_stable_launch()` (around line 930): retry the atomic move of the
  old `.dev/current` directory for up to 10 seconds after a transient Windows
  access-denied response, preserving the old path if the lock does not clear.

No unrelated source, configuration, or user data was changed by this task.
The repository already had extensive unrelated dirty and untracked work before
this task, including the `devscripts/dev-loop.py` edits; those edits were
preserved. The regression index and changelog directory also contained
pre-existing untracked material. The index was updated only because the user
requested a regression record.

## Specification and focused review

- Read `docs/ROUTER.md`, `docs/specs/dev-loop/dashboard.md`,
  `docs/specs/dev-loop/multi-instance.md`, `docs/specs/dev-loop/dev-loop.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/live-development/live-development.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/regressions/README.md`.
- Focused review: `docs/skills/spec-behavior-review-v1.md` — PASS. The
  implementation now follows the artifact contract: failed builds preserve
  the last valid artifact, and launch requests can use the newest verified
  publication. No spec change was needed.
- Regression record: `docs/regressions/dev-loop/latest-artifact-pointer-REG-2026-11-05.md`;
  indexed as Entry 20 in `docs/regressions/dev-loop/README.md`.
- A second confirmed failure, stable-path replacement racing Windows process
  exit, is recorded as Entry 21 in `docs/regressions/dev-loop/README.md` and
  `docs/regressions/dev-loop/stable-launch-swap-lock-REG-2026-11-05.md`.

## Validation and runtime evidence

- Build: the existing dev-loop worker completed a debug build (`Compiled: 1`,
  `Skipped: 613`) and published valid artifact
  `2026-11-06T01:57:33.707752Z` with SHA-256
  `127ddfac0f072a54d1533a84a6a1eda6ab45e97ff9b42f9d0eba20c5d19e5f00`.
- First launch: run `20261106_015703` launched the prior valid artifact while
  the build was in progress; server/client readiness and matching identity
  completed.
- Latest launch: run `20261106_015800` launched the newly published artifact
  through the dev-loop. Final status: `running`, server PID `26064`, client PID
  `16740`, both ready, matching build IDs, mode `current`, map `funworld3`.
  `.dev/current/mimita.exe` exists and the live processes use that path.
- Direct dashboard confirmation: button request `dashboard-15c6aada` with
  action `build-or-launch` completed. Current run `20261106_020329` has server
  PID `8812`, client PID `2476`, both ready and matching on artifact
  `2026-11-06T01:57:33.707752Z`; the executable exists at
  `.dev/current/mimita.exe` and is running visibly.
- Latest dashboard run: the button selected verified artifact
  `2026-11-06T02:05:45.057794Z` (SHA-256
  `f6cea7c92bad9729d8b123f088663674376dc4197bcd12d8c281289f3fc73ed4`). Run
  `20261106_020803` records `client_connection=ready`, `mode_start=ready`, and
  `complete=succeeded`; status reports server PID `11788`, client PID `17800`,
  matching builds, mode `fun counter strike`, and map `dust2cyberiav4`.
- A follow-up request to swap to that artifact initially failed with
  `WinError 5` while renaming `.dev/current`; the live process pair had been
  terminated before activation. A later dashboard request completed once the
  stable path was available. The bounded retry is in source, but the currently
  running daemon has not loaded that last edit: the watcher defers a source
  restart while a healthy run is active. The new retry remains unverified at
  runtime until a safe daemon restart.
- The broken artifact folder remained present during daemon recovery, proving
  startup skipped it in favor of the older valid artifact.
- No synthetic tests were added or run. The published identity reports
  `source_changed_during_build=true` for the dirty checkout; retain this as a
  source/build caveat. Runtime process and journal evidence is separate from
  human visual acceptance, which was not performed.

The daemon has continued publishing builds from newer source generations. At
the latest check, successful artifact
`2026-11-06T02:13:33.286531Z` existed while run `20261106_020803` remained on
artifact `2026-11-06T02:05:45.057794Z`; newer successful artifacts are not
activated automatically because `auto_restart=false`. Run `020803` remains
ready with matching server/client identities, and `.dev/current/mimita.exe`
hashes to the run's selected artifact. A follow-up build (source generation
22) was still active. The watcher recorded
`dev-loop.watcher.source-change-deferred`: the healthy game run prevented the
new daemon source from being loaded. Build and runtime evidence remain
separate.

## Human review

Confirm the visible MiMITA window appears and the current launch feels correct.
The exact process/run identity is verified in
`logs/2026-11-06/20261106_020803/`; visual/gameplay acceptance is still
pending. The process is left running as requested.
