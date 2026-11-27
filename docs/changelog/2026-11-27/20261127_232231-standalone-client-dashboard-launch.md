// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Standalone client dashboard launch

# Task

- Task ID: session-20261127-232228
- Session ID: session-20261127-232228
- Owner: codex-devloop-gui-client-standalone
- Focus: issue-9129efc0dda7 dev-loop dashboard client start
- Summary: Allow Start client to launch a managed standalone client when no local server is healthy; retain server connection behavior otherwise.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-27T23:22:30+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 1cfc7704d3fd5d378ad8e312881832689cb0d0d4
- Final commit: 1cfc7704d3fd5d378ad8e312881832689cb0d0d4

# Pre-existing changes

At changelog capture, the shared checkout also contained unrelated edits to
assets, configuration, editor code, docs, and a nested `devscripts/blender-ai`
repository. There were additional concurrent edits in `devscripts/dev-loop.py`
and `devscripts/dev_loop/gui.py`; attribution of those non-task hunks is
unknown. This session applied narrow patches only and preserved all other
content. The generated status snapshot below is a capture-time snapshot, not a
baseline from session start.

# Requested behavior

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/dashboard.md`,
  `docs/specs/dev-loop/multi-instance.md`,
  `docs/specs/dev-loop/process-lifecycle-and-release-events.md`.
- Exact requirements: the dashboard routes Client actions to the owning
  checkout daemon; active-run clients keep their own journal and lifecycle;
  a standalone executable keeps per-process event logging; client status is
  independent of server health.
- Why the change follows the specification: the no-server case now launches a
  managed client without a connection target, while a healthy primary server
  remains the default target. The manifest omits a server association for a
  standalone client.
- Conflicts or decisions: none. Runtime proof remains outstanding.

# Exact implementation changes

## Files: `devscripts/dev-loop.py`, `devscripts/dev_loop/gui.py`

- Lines/functions/headings: `DevLoop.start_client_instance`,
  `_instance_records`, and the dashboard process-panel and checkout-row
  Client actions.
- Old content or behavior: start-client required a healthy `server-01`, and
  could not choose an executable when the daemon had not launched a run.
- New content or behavior: GUI actions pass an automatic server target. The
  daemon connects to `server-01` when healthy; otherwise it selects the latest
  published executable, initializes run/build identity, creates a managed
  client journal, and omits `--room` and `--connect`. Standalone clients report
  no server association and are stale rather than frozen when no snapshots
  arrive.
- Reason: allow a client to open independently from the dashboard.
- Why unrelated behavior is preserved: explicit server targets still require
  that server and the automatic GUI target resolves to the same server-backed
  launch when it is healthy.

## Files: `docs/specs/dev-loop/dashboard.md`, `docs/specs/dev-loop/multi-instance.md`

- Lines/headings: checkout/process controls; topology and run manifest.
- Old content or behavior: no standalone managed-client case was documented.
- New content or behavior: documents optional server association, connection
  arguments, lifecycle, and dashboard fallback behavior.
- Reason: align desired behavior with implementation.
- Why unrelated behavior is preserved: the paired client contract remains.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `devscripts/dev-loop.py` | Reject without server; no idle-daemon artifact fallback. | Select healthy server or published client-only launch; retain per-client metadata/status. | Start client with no server. | Revert only the task hunks; preserve other edits in this shared file. |
| `devscripts/dev_loop/gui.py` | Client controls used default server requirement. | Both Client controls request automatic server selection; process button names standalone fallback. | Expose standalone behavior in dashboard. | Revert only this task's hunks; preserve other edits. |
| `docs/specs/dev-loop/dashboard.md` | No serverless Client fallback. | Defines standalone dashboard client lifecycle and label. | Document behavior. | Revert the added paragraph. |
| `docs/specs/dev-loop/multi-instance.md` | Every client implied a server association. | Server association is optional for standalone clients. | Document manifest/status shape. | Revert the added standalone-client text. |
| `docs/issues/improvement/20261127-allow-dashboard-client-launch-without-an-active--issue-9129efc0dda7.md` | Not present. | Append-only request, source finding, and proof expectation. | Track runtime proof. | Preserve issue history; do not delete. |
| `docs/changelog/2026-11-27/20261127_232231-standalone-client-dashboard-launch.md` | Not present. | This session record. | Required session history. | Preserve the record. |

# Contract / ABI changes

- Contract: dashboard `start-client` control accepts an empty automatic server
  target; run manifests allow a client with no server association.
- Version bump: none; existing fields are optional.
- Compatibility: CLI requests with an explicit/default `server-01` retain the
  existing server-required behavior.
- Reverse: revert the four task files to restore server-required launch.

# State / schema changes

- Domain: run manifest instance metadata.
- Old schema: client rows always named `server-01`.
- New schema: standalone client rows omit `server_instance_id` and
  `connected_address`.
- Migration: no persistent database; old run manifests remain readable.
- Reverse: restore the server association requirement in the producer/spec.

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

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` —
  source/spec comparison completed; `docs/workflows/runtime-scenario-validation.md`
  — runtime proof is required but not performed; `docs/operations/task-completion/task-completion.md`
  — followed for changelog and evidence separation.
- Tests and exact commands: none run, per task instruction against adding or
  running tests. `git diff --check` passed for the four task files.
- Build status: no C++ compiler inputs changed; no build requested.
- Runtime or hot-reload evidence: not exercised; no client process or GUI
  restart performed.
- Output files: issue `issue-9129efc0dda7` records the request and outstanding
  runtime proof.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review: dashboard label not observed in a restarted GUI.
- Gameplay review: not applicable to Python launch-control edit.
- Multiplayer review: paired path not run after this edit.
- Still unverified: press Start client with no server and inspect its journal;
  repeat with a healthy server and confirm the connection target.

# Agent handoff context

- Owner: codex-devloop-gui-client-standalone
- Focus: issue-9129efc0dda7 dev-loop dashboard client start
- Files to check: the four task files listed above.
- What another agent should not redo: source/spec tracing and implementation.
- Next exact step: exercise both standalone and server-backed GUI controls,
  inspect the client journal and manifest, and update issue proof.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 1cfc7704d3fd5d378ad8e312881832689cb0d0d4 / 1cfc7704d3fd5d378ad8e312881832689cb0d0d4
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": true, "latest_successful_build": "2026-11-27T17:45:19.700118Z", "latest_successful_build_id": "2026-11-27T17:45:19.700118Z:758a748d853e7b8b5bed98c614f8b243c0c9e5b7e8b9d9455fd04d6d9b096fdc", "latest_successful_source_generation": 42}, "game_current": {"build_mode": "debug", "code_hash": "e5abe8a081328fa1d394e60a0739a97516c1761de12dae04e92b7f605fdb0308", "dll": "build\\game\\generation-805\\game-g805.dll", "generation": 805, "revision": "fc7d555aa70be7ec4994eb754ae00f4d96b0a68c55cab3cfda4c1968bff51c1c", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-805\\game-g805.dll", "generation": 805, "published_utc": "2026-11-27T18:48:09.805Z", "revision": "fc7d555aa70be7ec4994eb754ae00f4d96b0a68c55cab3cfda4c1968bff51c1c", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 1.911, "phase": "compiling", "skipped": 87, "updated_utc": "2026-11-27T17:46:06.626Z"}}
- Generation: 805
- Artifact: build\game\generation-805\game-g805.dll
- Journal path: C:\mimita-v9\logs\2026-11-27\20261127_230255\client-01\events.jsonl
- Session ID: session-20261127-232228

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
M assets/avatars/remote_81bd6aa8fbb5fda8aa34686a/avatar.json
 M config/accounts/default.json
 M config/analytics.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/gui.py
 M docs/issues/README.md
 M docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-07977478c8fc.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/multi-instance.md
 M src/host/host-runtime.cpp
 M src/mapeditor/map-editor.cpp
?? config/maps/mimita-pipes.json
?? docs/changelog/2026-11-27/20261127_1906-issue-0cdac76fb006-independent-review.md
?? docs/issues/improvement/20261127-allow-dashboard-client-launch-without-an-active--issue-9129efc0dda7.md
?? docs/issues/launch-friction/20261121-launch-failure-mode-start-timeout-at-mode-start-issue-150c895a1d99.md
```

## Captured diff stat

```text
.../remote_81bd6aa8fbb5fda8aa34686a/avatar.json    |   6 -
 config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             |  54 +++++---
 devscripts/dev_loop/gui.py                         |  10 +-
 docs/issues/README.md                              |   7 +-
 ...-actions-cause-frame-free-issue-07977478c8fc.md |   8 ++
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  61 +++++++++
 docs/specs/dev-loop/dashboard.md                   |   7 ++
 docs/specs/dev-loop/multi-instance.md              |  15 ++-
 src/host/host-runtime.cpp                          |  46 ++++++-
 src/mapeditor/map-editor.cpp                       | 140 +++++++++++++++++++--
 14 files changed, 312 insertions(+), 48 deletions(-)
```

## Captured changed paths

```text
M	assets/avatars/remote_81bd6aa8fbb5fda8aa34686a/avatar.json
M	config/accounts/default.json
M	config/analytics.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/gui.py
M	docs/issues/README.md
M	docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-07977478c8fc.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/multi-instance.md
M	src/host/host-runtime.cpp
M	src/mapeditor/map-editor.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-27T17:45:19.700118Z",
    "latest_successful_build_id": "2026-11-27T17:45:19.700118Z:758a748d853e7b8b5bed98c614f8b243c0c9e5b7e8b9d9455fd04d6d9b096fdc",
    "latest_successful_source_generation": 42
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "e5abe8a081328fa1d394e60a0739a97516c1761de12dae04e92b7f605fdb0308",
    "dll": "build\\game\\generation-805\\game-g805.dll",
    "generation": 805,
    "revision": "fc7d555aa70be7ec4994eb754ae00f4d96b0a68c55cab3cfda4c1968bff51c1c",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-805\\game-g805.dll",
    "generation": 805,
    "published_utc": "2026-11-27T18:48:09.805Z",
    "revision": "fc7d555aa70be7ec4994eb754ae00f4d96b0a68c55cab3cfda4c1968bff51c1c",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 1.911,
    "phase": "compiling",
    "skipped": 87,
    "updated_utc": "2026-11-27T17:46:06.626Z"
  }
}
```
