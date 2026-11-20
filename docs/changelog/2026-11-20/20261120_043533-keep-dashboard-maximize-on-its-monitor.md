// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Keep dashboard maximize on its monitor

# Task

- Task ID: session-3a8a23af99c9
- Session ID: session-3a8a23af99c9
- Owner: dashboard-no-snap
- Focus: devscripts/dev_loop/gui.py
- Summary: Confirmed toggle_maximize used SPI_GETWORKAREA and always moved the borderless dashboard to the primary display. It now queries the monitor nearest the dashboard window via MonitorFromWindow/GetMonitorInfoW, preserves signed offsets for left/above monitors, and leaves the window untouched on lookup failure. Updated dashboard placement contract and appended an attempted-fix regression record. Python syntax, negative Tk geometry formatting, current HWND monitor query, and git diff whitespace checks passed; live secondary-monitor maximize/restore remains unverified because the open dashboard process predates the source change.
- Status: attempted
- Date, time, timezone: 2026-11-20T04:35:32+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: ce0009d5b2983f462d1d17dd640c4e66769c2a23
- Final commit: ce0009d5b2983f462d1d17dd640c4e66769c2a23

# Pre-existing changes

- Exact status output: Captured at the end of this changelog; it includes concurrent work from other sessions.
- Files not created or modified by this session: `AGENTS.md`, `config/accounts/default.json`, `config/analytics.json`, `devscripts/blender-ai`, `devscripts/dev-loop.py`, `devscripts/dev_loop/cli.py`, `docs/ROUTER.md`, `docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md`, `docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md`, `docs/issues/README.md`, `docs/specs/dev-loop/agent-self-improvement.md`, `docs/specs/networking/contact-authority.md`, `devscripts/dev_loop/launch_report.py`, `devscripts/quality/test_launch_report.py`, `devscripts/tech-debt-audit.py`, `docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md`, `docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md`, `docs/issues/architecture/`, `docs/issues/tech-debt/`, and `docs/specs/architecture/`.

# Requested behavior

> can u investigate and make ti so the gui doesnt have th snappgin issue

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/dashboard.md`
- Exact requirements: Maximize on the monitor nearest the dashboard, preserve signed offsets, restore prior geometry, and avoid moving if monitor lookup fails.
- Why the change follows the specification: The primary-display work-area lookup was replaced with a lookup against the dashboard's own window handle.
- Conflicts or decisions: None.

# Exact implementation changes

## File: `devscripts/dev_loop/gui.py`

- Lines/functions/headings: `_monitor_work_area`, `_tk_geometry`, `toggle_maximize`
- Old content or behavior: `SPI_GETWORKAREA` returned primary-display bounds and maximize moved the dashboard there.
- New content or behavior: Query `MonitorFromWindow`/`GetMonitorInfoW` for the nearest monitor, maximize to its work area, format signed Tk offsets, and leave the window untouched on lookup failure.
- Reason: Prevent maximizing from snapping a dashboard on a secondary display to the primary display.
- Why unrelated behavior is preserved: Normal saved geometry, restore geometry, and daemon/game lifecycle are unchanged.

## File: `docs/specs/dev-loop/dashboard.md`

- Lines/functions/headings: `Window placement and maximize`
- Old content or behavior: No explicit multi-monitor maximize contract.
- New content or behavior: Specifies monitor-local maximize/restore, signed offsets, and no movement when lookup fails.
- Reason: Make placement expectations explicit.
- Why unrelated behavior is preserved: Adds only a focused placement contract.

## File: `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md`

- Lines/functions/headings: New regression record.
- Old content or behavior: No record for primary-display maximize snapping.
- New content or behavior: Records report, cause, attempted fix, and outstanding runtime proof.
- Reason: Retain the user report and evidence state.
- Why unrelated behavior is preserved: No existing regression history was edited.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `devscripts/dev_loop/gui.py` | Primary monitor bounds | Nearest monitor bounds | Fix maximize snap | Revert only the helper and maximize hunk |
| `docs/specs/dev-loop/dashboard.md` | No explicit placement contract | Monitor-local maximize behavior | Specify desired behavior | Revert only the placement section |
| `docs/regressions/dev-loop/README.md` | 29 entries | Entry 30 appended | Index regression | Revert Entry 30 append |
| `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md` | Absent | Attempted-fix record | Preserve diagnosis/evidence | Remove this file |
| `docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md` | Absent | User report and triage | Preserve issue intake | Remove this file |
| `docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md` | Absent | This session record | Required changelog | Remove this file |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category: Dashboard window placement.
- Input: Dashboard HWND.
- Decision: Select the nearest monitor and query its work area.
- Output: Monitor-local Tk geometry.
- Failure or rejection reason: Leave the window unchanged if Windows cannot identify the monitor/work area.
- Rate limiting: One lookup per maximize action.

# Validation

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` identified the first incorrect step; `docs/skills/documentation-checker-v1.md` informed the placement contract and regression link.
- Tests and exact commands: `python -m py_compile devscripts/dev_loop/gui.py`; `_monitor_work_area(2230668)` returned `(0, 0, 1536, 816)` for the current dashboard HWND; signed Tk geometry evaluated to `1920x1040-1920-20`; `git diff --check` passed.
- Build status: No game build required for Python dashboard code.
- Runtime or hot-reload evidence: The visible dashboard predates the edit and still has old code loaded. Secondary-monitor maximize/restore was not exercised.
- Output files: Regression record and changelog.

# Measured evidence

- Before values: Custom maximize used primary-display bounds.
- After values: Current HWND monitor lookup succeeded and returned `(0, 0, 1536, 816)` for the current primary-monitor window.
- Timestamps: 2026-11-20 UTC.
- Tick/frame/network measurements: Not applicable.

# Regression review

- Regression entry appended: Yes.
- Why this is or is not a confirmed regression: Source used primary-display bounds unconditionally and the user reported cross-monitor snapping. The code fix is attempted; runtime behavior is not yet proven.
- Related regression paths: `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md`.

# Human acceptance

- Visual review: The live dashboard was identified, but maximize/restore was not observed on a secondary monitor.
- Gameplay review: Not applicable.
- Multiplayer review: Not applicable.
- Still unverified: Restarting the view and checking maximize/restore on a secondary monitor.

# Agent handoff context

- Owner: dashboard-no-snap
- Focus: devscripts/dev_loop/gui.py
- Files to check: `devscripts/dev_loop/gui.py`, `docs/specs/dev-loop/dashboard.md`, and the linked regression record.
- What another agent should not redo: Do not restore `SPI_GETWORKAREA`; it targets the primary display.
- Next exact step: Restart the single dashboard view through its normal lifecycle, then maximize and restore from a secondary monitor and record proof.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / ce0009d5b2983f462d1d17dd640c4e66769c2a23 / ce0009d5b2983f462d1d17dd640c4e66769c2a23
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-20T02:29:08.406738Z", "latest_successful_build_id": "2026-11-20T02:29:08.406738Z:1026e13afa4215122cb82cf069a8d1b4fee1e887b6b4d224d97163dec8085755", "latest_successful_source_generation": 57}, "game_current": {"build_mode": "debug", "code_hash": "b1e8824cc320422d440880ae80f1e1bf7c91b76b8cf20983ddd3a7f7126062be", "dll": "build\\game\\generation-394\\game-g394.dll", "generation": 394, "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-394\\game-g394.dll", "generation": 394, "published_utc": "2026-11-20T04:05:35.363Z", "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0", "status": "ready"}, "progress": {"compiled": 5, "elapsed_s": 577.909, "phase": "build_complete", "result": "success", "skipped": 82, "updated_utc": "2026-11-20T02:11:17.625Z"}}
- Generation: 394
- Artifact: build\game\generation-394\game-g394.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_043123\server-01\events.jsonl
- Session ID: session-3a8a23af99c9

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M AGENTS.md
 M config/accounts/default.json
 M config/analytics.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/gui.py
 M docs/ROUTER.md
 M docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
 M docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
 M docs/issues/README.md
 M docs/regressions/dev-loop/README.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/networking/contact-authority.md
?? devscripts/dev_loop/launch_report.py
?? devscripts/quality/test_launch_report.py
?? devscripts/tech-debt-audit.py
?? docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md
?? docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md
?? docs/issues/architecture/
?? docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md
?? docs/issues/tech-debt/
?? docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md
?? docs/specs/architecture/
?? docs/specs/dev-loop/launch-feedback.md
```

## Captured diff stat

```text
AGENTS.md                                          | 10 +++
 config/accounts/default.json                       |  2 +-
 config/analytics.json                              |  2 +-
 devscripts/blender-ai                              |  0
 devscripts/dev-loop.py                             | 92 +++++++++++++++++-----
 devscripts/dev_loop/cli.py                         |  6 ++
 devscripts/dev_loop/gui.py                         | 66 +++++++++++++---
 docs/ROUTER.md                                     |  2 +
 ...dination-brain-and-build-fan-out-attribution.md | 17 +++-
 .../20261120_040000-live-dashboard-build-health.md | 19 ++++-
 docs/issues/README.md                              |  4 +
 docs/regressions/dev-loop/README.md                |  9 ++-
 docs/specs/dev-loop/agent-self-improvement.md      | 42 ++++++++++
 docs/specs/dev-loop/dashboard.md                   | 13 +++
 docs/specs/networking/contact-authority.md         | 86 ++++++++++++++++++++
 15 files changed, 326 insertions(+), 44 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/gui.py
M	docs/ROUTER.md
M	docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
M	docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
M	docs/issues/README.md
M	docs/regressions/dev-loop/README.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/networking/contact-authority.md
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-20T02:29:08.406738Z",
    "latest_successful_build_id": "2026-11-20T02:29:08.406738Z:1026e13afa4215122cb82cf069a8d1b4fee1e887b6b4d224d97163dec8085755",
    "latest_successful_source_generation": 57
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "b1e8824cc320422d440880ae80f1e1bf7c91b76b8cf20983ddd3a7f7126062be",
    "dll": "build\\game\\generation-394\\game-g394.dll",
    "generation": 394,
    "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-394\\game-g394.dll",
    "generation": 394,
    "published_utc": "2026-11-20T04:05:35.363Z",
    "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0",
    "status": "ready"
  },
  "progress": {
    "compiled": 5,
    "elapsed_s": 577.909,
    "phase": "build_complete",
    "result": "success",
    "skipped": 82,
    "updated_utc": "2026-11-20T02:11:17.625Z"
  }
}
```
