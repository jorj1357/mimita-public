# One coordinator dashboard for MiMITA lanes

**Timestamp:** 2026-11-09 15:41:07 EST (2026-11-09T20:41:07Z)  
**Branch:** `checkpoint/pre-live-kernel-20261108`  
**HEAD:** `8dfb1a4c86a06de8905ddbb68f5bcb0ed8ea4b1e`

## Summary

Lane startup no longer opens a dashboard. The main checkout dashboard reads
the main status and registered lane snapshots, routes lane actions to the
matching daemon, and shows one freshness timer with a larger, scrollable status
and process view.

## Files changed

- `devscripts/agent-lane.py:257-295` — lane ensure and run requests pass
  `--no-dashboard`.
- `devscripts/dev_loop/cli.py:59` — add the no-dashboard option.
- `devscripts/dev-loop.py:5232-5265,5618-5622,5730-5782,5875-5885` — only the
  coordinator checkout opens the shared dashboard; the dashboard reads lane
  metadata/status within the repository and routes lane actions to the lane's
  own daemon endpoint.
- `devscripts/dev_loop/gui.py:20-23,421-484,607-660,935-1047,1260-1274` — enlarge
  default dimensions and text, add scrollable checkout/lane status rows and
  per-checkout Build/Launch/Server/Client actions, display lane identity and
  available connection target in process rows, label unknown process roles as
  unverified, and replace repeated timestamps with one `Updated Ns ago` line.
- `docs/specs/dev-loop/dashboard.md:1-170` — define coordinator ownership,
  lane overview, single freshness indicator, and acceptance criteria.
- `docs/specs/dev-loop/dev-loop.md:231-240,1546-1555,2278-2288,2392-2405,2887-2898`
  — align dashboard entry points and lane behavior with the coordinator model.
- `docs/specs/dev-loop/multi-instance.md:195-198` — align process freshness
  wording with the shared dashboard indicator.
- `docs/specs/open-gameplay-issues.md:129` — add OGI-033 for duplicate windows,
  readability, lane/process visibility, and remaining acceptance.

## Previous and updated behavior

- Previous: each checkout's `ensure_running` path could start a dashboard, so
  creating/ensuring lanes could produce more windows.
- Updated: `ensure_running` opens a dashboard only when the worktree is the
  coordinator root; lane manager requests explicitly suppress dashboard
  startup. Even a direct dashboard request from a lane returns status without
  opening a window.
- Previous: the GUI showed a single checkout and separate process scan
  timestamps in a small default window.
- Updated: the main view includes registered lanes, their build/run summaries,
  process rows with lane/role/PID/state/target, larger type and dimensions, and
  one shared refresh-age indicator. Lane controls use the matching daemon's
  authenticated control port/token from its status snapshot, and completion
  feedback is read from that same lane's status.

## Evidence and review

- Pre-existing workspace edits were present at entry across build scripts,
  configuration, hot-reload, networking, NPC code, specs, and changelogs.
  Preserved those edits. `devscripts/dev-loop.py` and
  `docs/specs/open-gameplay-issues.md` already contained unrelated changes;
  this work did not replace them.
- Read `docs/ROUTER.md`, the dashboard, multi-instance, dev-loop, and lane build
  authority specifications, `docs/regressions/README.md`,
  `docs/architecture/live-development/self-contained-workspace.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/skills/documentation-checker-v1.md`, and
  `docs/skills/spec-behavior-review-v1.md`.
- Validation: `python -m py_compile` passed for the four changed Python files;
  the dev-loop CLI help showed `--no-dashboard`; changed code and dashboard
  contract passed `git diff --check`. The open-gameplay-issues diff still has
  pre-existing trailing whitespace at lines 11 and 43; left those human-authored
  lines unchanged.
- No EXE build, dev-loop launch, visual GUI check, or gameplay/runtime check was
  performed. The agent build-authority rule prohibits agents from building,
  and several existing lanes reported active daemons/runs. The window count,
  multi-lane controls, and multi-client/server presentation still need live
  human verification through the coordinator dev-loop.
- No regression record was added: the reported duplicate-window behavior was
  not reproduced in this session, and code/static validation is not runtime
  proof.
- Documentation TODO scan found the existing informal dashboard note at
  `docs/specs/open-gameplay-issues.md:43` (“todo make it so dev loop launchgin
  that just 1 single gui…”). OGI-033 now tracks the precise desired behavior;
  the shorthand remains unchanged as historical input.
- Focused review: the dashboard contract and implementation route controls by
  checkout identity and leave daemon state ownership unchanged. Runtime,
  visual, and human acceptance remain open.
