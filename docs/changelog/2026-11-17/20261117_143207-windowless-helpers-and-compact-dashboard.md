// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Windowless helpers and compact dashboard

# Task

- Task ID: 20261117-dev-loop-window-dashboard
- Session ID: 20261117-dev-loop-window-dashboard
- Owner: Codex
- Focus: dev-loop console launch and dashboard layout
- Summary: Hide build-helper consoles and keep dashboard controls visible at 500x500.
- Status: complete
- Date, time, timezone: 2026-11-17T14:32:06+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d41f00ef4c7e83cf240a716373aa6efa6b51c14c
- Final commit: d41f00ef4c7e83cf240a716373aa6efa6b51c14c

# Pre-existing changes

- Exact status output: Full `git status` was blocked by access denied while Git
  traversed `external/libjuice`; path-specific diffs were available.
- Files not created or modified by this session: Before this work,
  `devscripts/dev-loop.py` already contained unrelated build-relaunch changes,
  and `docs/specs/dev-loop/dev-loop.md` began with a user-authored dated
  performance note. Both were preserved. The initial diff check reported
  trailing whitespace on that pre-existing first line.

# Requested behavior

Find the source of the recurring ccache/Python console windows and keep the
coordinator dashboard usable at 500x500 with its action buttons visible.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/dev-loop.md`;
  `docs/specs/dev-loop/dashboard.md`;
  `docs/operations/build-and-exe/build-and-exe.md`;
  `docs/specs/dev-loop/build-time-observability.md`.
- Exact requirements: Short-lived build helpers must not open console windows;
  dashboard opens at 500x500 and keeps launch/build and instance controls
  visible while status panels scroll.
- Why the change follows the specification: Uses the existing hidden build
  runner for ccache statistics and the post-link gate; the dashboard puts
  scrolling panels above a fixed action footer.
- Conflicts or decisions: The user requested 500x500, replacing the previous
  400x500 specification size.

# Exact implementation changes

## `build.py`

- Lines/functions/headings: `ccache_stats()` and post-link `cold_gate`.
- Old content or behavior: Direct `subprocess.run()` launched `ccache.exe` and
  `build_target_gate.py` without Windows no-console flags.
- New content or behavior: Both use `build_toolchain.run_tool()`, which sets
  `CREATE_NO_WINDOW` on Windows.
- Reason: These are the best match for the observed sequence: ccache statistics
  are queried during the build, then the Python gate runs after linking.
- Why unrelated behavior is preserved: Only the helper launch wrapper changed;
  arguments, outputs, and build decisions are unchanged.

## `devscripts/dev-loop.py`

- Lines/functions/headings: `run_hidden()` call sites for the cold-link report,
  cutover report, render-capability report, and hot-client gate.
- Old content or behavior: Those helper calls bypassed the existing hidden
  subprocess policy.
- New content or behavior: They now use `run_hidden()`.
- Reason: Keep other detached dev-loop helper processes from allocating
  visible consoles.
- Why unrelated behavior is preserved: Existing user changes in nearby build
  relaunch logic were left intact.

## `devscripts/dev_loop/game_build.py`

- Lines/functions/headings: `_run_checked()`.
- Old content or behavior: The dev-loop's codegen-check subprocesses used
  `Popen()` without hidden-window flags.
- New content or behavior: On Windows they now set `CREATE_NO_WINDOW`,
  `STARTF_USESHOWWINDOW`/`SW_HIDE`, and `close_fds=True`.
- Reason: Prevent Python helper consoles during hot game-DLL builds.
- Why unrelated behavior is preserved: The command, output pipe, and return
  code behavior are unchanged.

## `devscripts/dev_loop/gui.py` and `docs/specs/dev-loop/dev-loop.md`

- Lines/functions/headings: Window constants and initial geometry; body canvas;
  fixed process and launch control rows; dashboard sizing requirement.
- Old content or behavior: 1120x900 initial size, 760x680 minimum, and controls
  could fall below the visible area.
- New content or behavior: 500x500 initial/minimum size; status panels scroll;
  process and launch/build buttons stay in the footer.
- Reason: Make the dashboard fit the requested size while retaining access to
  its actions.
- Why unrelated behavior is preserved: The dashboard remains resizable and
  still uses its existing status/control clients.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `build.py` | Plain helper launches | Hidden `run_tool()` launches | Prevent ccache/Python console windows | Revert only the two wrapper substitutions |
| `devscripts/dev-loop.py` | Plain helper launches | Hidden `run_hidden()` launches | Keep detached helpers windowless | Revert only the listed helper substitutions; preserve adjacent edits |
| `devscripts/dev_loop/game_build.py` | Codegen child without hide flags | Hidden child process on Windows | Prevent Python console windows | Revert only `_run_checked()` launch flags |
| `devscripts/dev_loop/gui.py` | Large minimum; controls can be off-screen | 500x500 with scrollable panels and fixed footer | Keep panels navigable and actions visible | Restore only sizing/layout hunks |
| `docs/specs/dev-loop/dev-loop.md` | 400x500 requirement | 500x500 with fixed footer | Match requested layout | Restore only sizing text; preserve dated note |

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

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md`;
  reviewed code against the routed behavior.
- Tests and exact commands: No tests run. `git diff --check` reported only the
  pre-existing trailing whitespace at line 1 of `docs/specs/dev-loop/dev-loop.md`.
- Build status: No build run; agents do not run builds.
- Runtime or hot-reload evidence: Dashboard visually observed at 500x500 with
  process actions and launch/build/More controls visible; scrolling/resizing
  gestures were not exercised. Read-only `python devscripts/dev-loop.py
  --status` reported an active run with a game-DLL generation 300 build in
  progress and its server/client alive. No build was started or interrupted;
  no fresh ccache console reproduction was attempted.
- Output files: None.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression: Static code confirms both
  helper subprocesses lacked no-console flags. The reported window sequence is
  consistent with those call sites, but no build was triggered for a fresh
  process-parent trace.
- Related regression paths:

# Human acceptance

- Visual review: 500x500 dashboard screenshot showed fixed footer controls.
- Gameplay review:
- Multiplayer review:
- Still unverified: User acceptance of scrolling/resizing; fresh build run
  confirming ccache and Python gate consoles no longer appear.

# Agent handoff context

- Owner: Codex
- Focus: dev-loop console launch and dashboard layout
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d41f00ef4c7e83cf240a716373aa6efa6b51c14c / d41f00ef4c7e83cf240a716373aa6efa6b51c14c
- Git status: path-specific review; full status unavailable because the
  `external/libjuice` submodule denied access during Git's recursive status scan.
- Diff stat: unavailable from Git's recursive status path; exact session hunks
  are described above.
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T01:01:29.903571Z", "latest_successful_build_id": "2026-11-17T01:01:29.903571Z:4cbb23a276282f8ea397bfce466a5223dc8a81892a7811fd5d060749eaf040d9", "latest_successful_source_generation": 32}, "game_current": {"build_mode": "debug", "code_hash": "e6c494faaf7f63ce52e155da72c3b962e3bf4213922ee07e66490b3cc604ff06", "dll": "build\\game\\generation-298\\game-g298.dll", "generation": 298, "revision": "755517f0c2b38055fe9f050228abe71ba294f38da96a4baf4efb808f2a215a9e", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-298\\game-g298.dll", "generation": 298, "published_utc": "2026-11-17T14:31:18.407Z", "revision": "755517f0c2b38055fe9f050228abe71ba294f38da96a4baf4efb808f2a215a9e", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 14.135, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-17T13:55:20.877Z"}}
- Generation: 298
- Artifact: build\game\generation-298\game-g298.dll
- Journal path: C:\mimita-v9\logs\2026-11-17\20261117_135530\client-01\events.jsonl
- Session ID: 20261117-dev-loop-window-dashboard

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
Full Git status failed while traversing external/libjuice (access denied).
Known session paths: build.py; devscripts/dev-loop.py;
devscripts/dev_loop/game_build.py; devscripts/dev_loop/gui.py;
docs/specs/dev-loop/dev-loop.md;
docs/changelog/2026-11-17/20261117_143207-windowless-helpers-and-compact-dashboard.md
```

## Captured diff stat

```text
Unavailable: recursive Git status failed on external/libjuice.
```

## Captured changed paths

```text
Known touched paths are listed in the captured git state; other pre-existing
working-tree paths could not be enumerated because of the submodule access error.
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-17T01:01:29.903571Z",
    "latest_successful_build_id": "2026-11-17T01:01:29.903571Z:4cbb23a276282f8ea397bfce466a5223dc8a81892a7811fd5d060749eaf040d9",
    "latest_successful_source_generation": 32
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "e6c494faaf7f63ce52e155da72c3b962e3bf4213922ee07e66490b3cc604ff06",
    "dll": "build\\game\\generation-298\\game-g298.dll",
    "generation": 298,
    "revision": "755517f0c2b38055fe9f050228abe71ba294f38da96a4baf4efb808f2a215a9e",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-298\\game-g298.dll",
    "generation": 298,
    "published_utc": "2026-11-17T14:31:18.407Z",
    "revision": "755517f0c2b38055fe9f050228abe71ba294f38da96a4baf4efb808f2a215a9e",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 14.135,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-17T13:55:20.877Z"
  }
}
```
