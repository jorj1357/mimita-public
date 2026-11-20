// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Preserve dashboard scroll position

# Task

- Task ID: session-20261120-034129
- Session ID: session-20261120-034129
- Owner: dashboard-scroll-refresh
- Focus: Dev-loop dashboard scrolling and refresh responsiveness
- Summary: Dashboard refreshes now avoid unchanged redraws, retain text and page scroll positions, use smoother wheel movement, and display larger type.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-20T03:41:33+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf
- Final commit: d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf

# Pre-existing changes

- The shared checkout already had numerous unrelated edits; the exact status,
  source identity, and broad captured diff appear in the evidence sections below.
- `devscripts/dev_loop/gui.py` was already modified by the dashboard
  coordination-panel work. I preserved those additions and changed only its
  refresh, text rendering, wheel scrolling, and font sizing paths.
- This session's only source edit is `devscripts/dev_loop/gui.py`; the issue
  intake and task records are in the shared coordination brain. This changelog
  is the only new repository file created by this session.

# Requested behavior

The user asked that dev-loop dashboard scrollbars keep their positions during
refresh, text be larger, mouse-wheel scrolling feel smoother, and refreshes stop
redrawing the whole view slowly. Their original wording is preserved in
`issue-6772ff949164`.

# Specification alignment

- Current specification: `docs/specs/dev-loop/dashboard.md`.
- Related requirement: dashboard rendering and animation work must remain
  bounded and must not delay daemon controls or build activity. The spec does
  not directly prescribe scrollbar persistence or font size.
- Related requirements: the shared freshness indicator refreshes in place,
  and process selection remains attached to stable process identity.
- The change reduces repeated widget work and keeps the current view position
  across refreshes, consistent with in-place bounded rendering. It does not
  change daemon or game behavior.
- No product decision was needed. Live visual acceptance remains outstanding.

# Exact implementation changes

## File: `devscripts/dev_loop/gui.py`

- Lines/functions: `label()` (424), `scroll_body()` (707),
  `set_text_content()` (745), `render_coordination()` (1058),
  `update_game_queue_panel()` (1860), and `refresh()` (1909, 2168).
- Old behavior: refresh replaced coordination text and rows repeatedly,
  rewrote the queue/output text every tick, forced output to the bottom, and
  used coarse whole-unit mouse-wheel scrolling. Dashboard labels were smaller.
- New behavior: skip unchanged coordination snapshots; replace text only when
  content differs and retain its view, sticking to the bottom only when it was
  already at the bottom; preserve the main canvas position; apply wheel input
  to the pane under the cursor with fractional pixel/line accumulation; raise
  explicit UI font sizes by one point.
- Reason: reduce visible redraw work and keep the user's reading position
  stable while preserving automatic following for a tailing output pane.
- The existing coordination panel, control callbacks, process identity
  selection, refresh cadence, and dev-loop/game processes were left intact.

## File: `devscripts/dashboard-keepalive.py`

- Function: `restore_window()` (lines 151-154).
- Old behavior: compare the window against primary-display width and height,
  then move it to `(20, 20)` when those bounds do not contain it.
- New behavior: call `MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL)` and move
  only when the window intersects no connected monitor.
- Reason: a secondary monitor outside primary-screen coordinates is still a
  valid desktop location and should not be “restored” onto the primary display.
- Keepalive still moves a genuinely off-screen window back to the primary
  display, which is the recovery path when a monitor was disconnected.

## Follow-up desktop placement change

- `devscripts/dev_loop/gui.py`: load the last valid `widthxheight+x+y` geometry
  from `.dev/dashboard-ui.json`; debounce geometry writes while the root window
  is resized or moved; persist it alongside collapsed-section state; save once
  more when the user closes the view.
- `devscripts/dashboard-keepalive.py`: use Windows `MonitorFromWindow` to
  determine whether the window intersects any connected monitor. The previous
  primary-screen-only bounds check could mistake a secondary-monitor window
  for an off-screen one and move it.
- The requested behavior was: “ensure the gui doesnt move from where it is put
  on the deesktop bc it keeps going back to the other screen”. The quote is
  appended to `issue-6772ff949164`; task `dashboard-window-placement` is open
  and unverified until relaunch and multi-monitor behavior are observed.

## Additional issues recorded in the coordination brain

- `issue-453d0596e294`: diagnose waiting for server snapshots and the reported
  reconnecting state. No cause has been asserted; reproduction and trace are
  required.
- `issue-bbc291c180f8`: investigate the reported Launch latest stable live
  executable replacement regression and its slowness. The original user quote
  was appended verbatim after correcting an entry typo. The existing
  `launch_latest()` path has stage events, but this report has not been
  reproduced or diagnosed.
- `issue-7d3dcfd7433b`: measure build, launch/readiness, and all wait stages;
  track avoidable or over-budget waits as issues and drive them toward zero.
  Follow-up task `latency-wait-occurrences` records every occurrence, including
  normally expected waits, with start/end, duration, and reason.
- `issue-dd1c5da31ecb`: profile the dashboard's remaining full-refresh/render
  costs and optimize from measured timings. This is separate from the focused
  text/coordination redraw reductions in this change.
- Original reports and their summaries are preserved in the append-only issue
  records; no unrelated wait was marked fixed.
- Generated issue-detail paths:
  `docs/issues/bug/20261120-keep-dev-loop-dashboard-scrollbars-stable-increa-issue-6772ff949164.md`,
  `docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md`,
  `docs/issues/bug/20261120-fix-launch-latest-stable-mimita-exe-live-executa-issue-bbc291c180f8.md`,
  `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md`,
  and `docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md`.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/agent_coord.py` |  |  |  | git checkout -- devscripts/agent_coord.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop-watch.py` |  |  |  | git checkout -- devscripts/dev-loop-watch.py |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/cli.py` |  |  |  | git checkout -- devscripts/dev_loop/cli.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` | pre-existing dashboard coordination panel edits | in-place refresh, scroll retention, fractional wheel handling, larger type | Keep the dashboard readable during refresh without replacing existing panel work | Revert only this session's hunks; do not restore the whole file because it contains pre-existing edits |
| `devscripts/dashboard-keepalive.py` | pre-existing keepalive script | monitor visibility check uses all connected displays | Stop a secondary-monitor window from being relocated | Revert only the `MonitorFromWindow` edit |
| `devscripts/dev_loop/parallel_test_runs.py` |  |  |  | git checkout -- devscripts/dev_loop/parallel_test_runs.py |
| `devscripts/quality/test_agent_coord.py` |  |  |  | git checkout -- devscripts/quality/test_agent_coord.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md` |  |  |  | git checkout -- docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/build-friction/20261120-build-friction-build-request-is-waiting-issue-346c68de182d.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-build-friction-build-request-is-waiting-issue-346c68de182d.md |
| `docs/issues/improvement/20261120-wire-build-friction-auto-intake-and-warm-candida-issue-67f2be3d7012.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-wire-build-friction-auto-intake-and-warm-candida-issue-67f2be3d7012.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/dev-loop/agent-self-improvement.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-self-improvement.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/dev-loop/dev-loop.md` |  |  |  | git checkout -- docs/specs/dev-loop/dev-loop.md |
| `docs/workflows/issue-intake-and-triage.md` |  |  |  | git checkout -- docs/workflows/issue-intake-and-triage.md |
| `src/combat/melee-client.cpp` |  |  |  | git checkout -- src/combat/melee-client.cpp |
| `src/engine/engine-tick-render.cpp` |  |  |  | git checkout -- src/engine/engine-tick-render.cpp |
| `devscripts/agent-wake.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/agent-wake.py' |
| `devscripts/dashboard-keepalive.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/dashboard-keepalive.py' |
| `docs/changelog/2026-11-20/20261120_021700-dev-loop-coordination-panel.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_021700-dev-loop-coordination-panel.md' |
| `docs/changelog/2026-11-20/20261120_023700-agent-proof-ledger-spec-and-gate.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_023700-agent-proof-ledger-spec-and-gate.md' |
| `docs/changelog/2026-11-20/20261120_025000-visible-melee-contact-fallback.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_025000-visible-melee-contact-fallback.md' |
| `docs/changelog/2026-11-20/20261120_033500-60hz-baseline-dashboard-presence-remote-culling.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_033500-60hz-baseline-dashboard-presence-remote-culling.md' |
| `docs/issues/bug/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/' |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md' |
| `docs/issues/docs/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/docs/' |
| `docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md' |
| `docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md' |
| `docs/issues/improvement/20261120-dev-loop-coordination-panel-show-agents-messages-issue-48e061680b80.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-dev-loop-coordination-panel-show-agents-messages-issue-48e061680b80.md' |
| `docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md' |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md' |
| `docs/issues/improvement/20261120-subagents-and-cohort-build-pointers-issue-0b04fc442d2e.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-subagents-and-cohort-build-pointers-issue-0b04fc442d2e.md' |
| `docs/issues/risk/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/risk/' |
| `docs/specs/dev-loop/agent-build-queue.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/agent-build-queue.md' |
| `docs/specs/dev-loop/agent-proof-and-verification.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/agent-proof-and-verification.md' |
| `docs/specs/dev-loop/agent-runtime-registry.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/agent-runtime-registry.md' |
| `docs/specs/dev-loop/agent-subagents.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/agent-subagents.md' |

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

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

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

- Visual review: not performed; dashboard process is still running its old
  in-memory code.
- Gameplay review: not applicable.
- Multiplayer review: not applicable.
- Still unverified: larger font fit, scroll persistence while values change,
  wheel feel, and refresh responsiveness in the restarted dashboard.

# Agent handoff context

- Owner: dashboard-scroll-refresh
- Focus: Dev-loop dashboard scrolling and refresh responsiveness
- Files to check: `devscripts/dev_loop/gui.py` around `scroll_body()`,
  `set_text_content()`, `render_coordination()`, and `refresh()`.
- What another agent should not redo: the code-level scroll retention, cached
  coordination snapshot, and text change detection are implemented.
- Next exact step: restart only the dashboard view, then visually verify scroll
  persistence, wheel movement, font fit, and refresh responsiveness. Task
  `dashboard-scroll-persistence` and issue `issue-6772ff949164` remain open.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf / d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-20T02:29:08.406738Z", "latest_successful_build_id": "2026-11-20T02:29:08.406738Z:1026e13afa4215122cb82cf069a8d1b4fee1e887b6b4d224d97163dec8085755", "latest_successful_source_generation": 57}, "game_current": {"build_mode": "debug", "code_hash": "b6d1cbf2d5fc97708361731b510da03fff337946637031ffceeb93a5ae0ee086", "dll": "build\\game\\generation-393\\game-g393.dll", "generation": 393, "revision": "27794bbe21a024a8e8db5cd164d1a59ac357b45551c97e83765dec4f56727ef1", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-393\\game-g393.dll", "generation": 393, "published_utc": "2026-11-20T03:36:21.219Z", "revision": "27794bbe21a024a8e8db5cd164d1a59ac357b45551c97e83765dec4f56727ef1", "status": "ready"}, "progress": {"compiled": 5, "elapsed_s": 577.909, "phase": "build_complete", "result": "success", "skipped": 82, "updated_utc": "2026-11-20T02:11:17.625Z"}}
- Generation: 393
- Artifact: build\game\generation-393\game-g393.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_033359\server-01\events.jsonl
- Session ID: session-20261120-034129

## Focused review and validation

- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The
  dashboard spec supports bounded in-place rendering; it does not explicitly
  promise sticky scroll positions or larger fonts. Runtime visual acceptance is
  still required.
- `docs/skills/efficiency-checker-v1.md`: PASS. The one-second refresh loop
  previously repeated text replacement and coordination row reconstruction.
  The new snapshot cache and content-aware text updates remove unchanged work;
  no separate performance benchmark was run.
- Validation: Python `compile()` syntax checks passed for both
  `devscripts/dev_loop/gui.py` and `devscripts/dashboard-keepalive.py` after the
  final edits; `git diff --check` passed. No build was needed for these
  Python-only dashboard edits.
- Final `python devscripts/dev-loop.py --status`: `lifecycle=stopped`,
  `run=NOT RUNNING`, no server/client PIDs, and whole-game DLL generation 393
  was `building`. An earlier status snapshot showed a running game, but the
  final snapshot changed while this task was in progress; this work did not
  issue a stop or build command.
- Tkinter assessment: it is a mature Python interface to Tcl/Tk, with Tk widget
  code implemented in C; its dated look does not by itself establish a speed
  problem. Its event loop is single-threaded, so long handlers block redraws.
  The measured code issue addressed here was repeated synchronous widget
  replacement; the new dashboard issue tracks timing the remaining refresh
  stages before considering a toolkit change.
- Reference: <https://docs.python.org/3/library/tkinter.html> (architecture and
  threading model).
- The existing dashboard process still has its old code loaded. The CUA window
  lookup returned a game screenshot for the selected dashboard handle, so I
  did not send a close action or restart the view. The saved dashboard code
  needs a view restart and visual scrolling check before the task can be marked
  proven or closed.

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
 M config/audio/music-settings.json
 M config/video-settings.json
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/dev-loop-watch.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/quality/test_agent_coord.py
 M docs/ROUTER.md
 M docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
 M docs/issues/README.md
 M docs/issues/build-friction/20261120-build-friction-build-request-is-waiting-issue-346c68de182d.md
 M docs/issues/improvement/20261120-wire-build-friction-auto-intake-and-warm-candida-issue-67f2be3d7012.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/workflows/issue-intake-and-triage.md
 M src/combat/melee-client.cpp
 M src/engine/engine-tick-render.cpp
?? devscripts/agent-wake.py
?? devscripts/dashboard-keepalive.py
?? docs/changelog/2026-11-20/20261120_021700-dev-loop-coordination-panel.md
?? docs/changelog/2026-11-20/20261120_023700-agent-proof-ledger-spec-and-gate.md
?? docs/changelog/2026-11-20/20261120_025000-visible-melee-contact-fallback.md
?? docs/changelog/2026-11-20/20261120_033500-60hz-baseline-dashboard-presence-remote-culling.md
?? docs/issues/bug/
?? docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
?? docs/issues/docs/
?? docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
?? docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
?? docs/issues/improvement/20261120-dev-loop-coordination-panel-show-agents-messages-issue-48e061680b80.md
?? docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md
?? docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
?? docs/issues/improvement/20261120-subagents-and-cohort-build-pointers-issue-0b04fc442d2e.md
?? docs/issues/risk/
?? docs/specs/dev-loop/agent-build-queue.md
?? docs/specs/dev-loop/agent-proof-and-verification.md
?? docs/specs/dev-loop/agent-runtime-registry.md
?? docs/specs/dev-loop/agent-subagents.md
```

## Captured diff stat

```text
AGENTS.md                                          |  15 +-
 config/accounts/default.json                       |   4 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/video-settings.json                         |   2 +-
 devscripts/agent_coord.py                          | 751 ++++++++++++++++++++-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop-watch.py                       |  33 +
 devscripts/dev-loop.py                             |  75 +-
 devscripts/dev_loop/cli.py                         |   7 +-
 devscripts/dev_loop/game_build.py                  |   9 +-
 devscripts/dev_loop/gui.py                         | 426 ++++++++++--
 devscripts/dev_loop/parallel_test_runs.py          |  22 +-
 devscripts/quality/test_agent_coord.py             | 131 +++-
 docs/ROUTER.md                                     |   5 +
 ...dination-brain-and-build-fan-out-attribution.md |  55 +-
 docs/issues/README.md                              |  15 +-
 ...-build-request-is-waiting-issue-346c68de182d.md |  14 +
 ...o-intake-and-warm-candida-issue-67f2be3d7012.md |   6 +
 docs/specs/dev-loop/agent-coordination.md          |  67 +-
 docs/specs/dev-loop/agent-self-improvement.md      |  73 ++
 docs/specs/dev-loop/dashboard.md                   |  74 +-
 docs/specs/dev-loop/dev-loop.md                    |   7 +
 docs/workflows/issue-intake-and-triage.md          |  41 +-
 src/combat/melee-client.cpp                        |  47 +-
 src/engine/engine-tick-render.cpp                  |  24 +-
 26 files changed, 1770 insertions(+), 137 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/video-settings.json
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/dev-loop-watch.py
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/quality/test_agent_coord.py
M	docs/ROUTER.md
M	docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
M	docs/issues/README.md
M	docs/issues/build-friction/20261120-build-friction-build-request-is-waiting-issue-346c68de182d.md
M	docs/issues/improvement/20261120-wire-build-friction-auto-intake-and-warm-candida-issue-67f2be3d7012.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/workflows/issue-intake-and-triage.md
M	src/combat/melee-client.cpp
M	src/engine/engine-tick-render.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-20T02:29:08.406738Z",
    "latest_successful_build_id": "2026-11-20T02:29:08.406738Z:1026e13afa4215122cb82cf069a8d1b4fee1e887b6b4d224d97163dec8085755",
    "latest_successful_source_generation": 57
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "b6d1cbf2d5fc97708361731b510da03fff337946637031ffceeb93a5ae0ee086",
    "dll": "build\\game\\generation-393\\game-g393.dll",
    "generation": 393,
    "revision": "27794bbe21a024a8e8db5cd164d1a59ac357b45551c97e83765dec4f56727ef1",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-393\\game-g393.dll",
    "generation": 393,
    "published_utc": "2026-11-20T03:36:21.219Z",
    "revision": "27794bbe21a024a8e8db5cd164d1a59ac357b45551c97e83765dec4f56727ef1",
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
