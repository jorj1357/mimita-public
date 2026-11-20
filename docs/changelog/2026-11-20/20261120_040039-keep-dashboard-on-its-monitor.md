// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Keep dashboard on its monitor

# Task

- Task ID: session-20261120-040037
- Session ID: session-20261120-040037
- Owner: dashboard-scroll-refresh
- Focus: Dashboard position stability across monitors
- Summary: Rechecked saved geometry and keepalive monitor detection. Found Windows custom maximize still uses the primary monitor work area; source edit is waiting on another agent's active gui.py lease.
- Status: BLOCKED
- Date, time, timezone: 2026-11-20T04:00:38+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf
- Final commit: d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf

# Pre-existing changes

- This checkout contains the pre-existing edits listed in the captured status
  below. No application source was changed in this recheck.
- Coordination brain update only: appended the user's new exact placement
  request to `issue-6772ff949164` and added task `dashboard-monitor-maximize`.
- This changelog is the only new file created by this session.

# Requested behavior

The user clarified: “moving the dashboard doenst reset its positon twhatevr
window, itjust stuck to wahtever window its on”. They want it to stay on the
monitor they move it to, including when maximizing/restoring.

# Specification alignment

- Current specification: `docs/specs/dev-loop/dashboard.md`.
- Its restoration rule says dashboard restoration must not steal focus or open
  a per-command window; it does not specify multi-monitor geometry behavior.
- The prior code saves normal size/position and the keepalive now recognizes a
  window intersecting any monitor. Reinspection found one remaining mismatch:
  the dashboard's custom Windows maximize path asks `SystemParametersInfoW`
  for the primary monitor's work area, even when the window is on a secondary
  monitor.
- No product decision is needed. A live edit is currently blocked by the active
  `gui.py` coordination lease held by `runtime-60hz` through
  `2026-11-20T04:43:10.449Z`.

# Exact implementation changes

- No source implementation change was made in this session.
- The remaining code fix is localized to `toggle_maximize()` in
  `devscripts/dev_loop/gui.py` (line 662): use `MonitorFromWindow` and
  `GetMonitorInfoW` to get the work area for the window's current monitor, then
  restore the captured geometry on the same monitor.
- Existing placement code: startup restores `.dev/dashboard-ui.json` geometry;
  root `<Configure>` debounces writes to that file; `close_view()` saves again;
  `dashboard-keepalive.py` uses `MonitorFromWindow` and only relocates a window
  that intersects no connected monitor.
- I did not edit `gui.py` because `agent-coord edit begin` rejected the write
  while another agent's lease was active. The hard lease rule is the blocker.

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/agent-runtime.py` |  |  |  | git checkout -- devscripts/agent-runtime.py |
| `devscripts/agent_coord.py` |  |  |  | git checkout -- devscripts/agent_coord.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop-watch.py` |  |  |  | git checkout -- devscripts/dev-loop-watch.py |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/cli.py` |  |  |  | git checkout -- devscripts/dev_loop/cli.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/dev_loop/parallel_test_runs.py` |  |  |  | git checkout -- devscripts/dev_loop/parallel_test_runs.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
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
| `src/network/server-packet-handlers.cpp` |  |  |  | git checkout -- src/network/server-packet-handlers.cpp |
| `src/network/server-packets.cpp` |  |  |  | git checkout -- src/network/server-packets.cpp |
| `devscripts/agent-wake.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/agent-wake.py' |
| `devscripts/dashboard-keepalive.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/dashboard-keepalive.py' |
| `docs/changelog/2026-11-20/20261120_021700-dev-loop-coordination-panel.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_021700-dev-loop-coordination-panel.md' |
| `docs/changelog/2026-11-20/20261120_023700-agent-proof-ledger-spec-and-gate.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_023700-agent-proof-ledger-spec-and-gate.md' |
| `docs/changelog/2026-11-20/20261120_025000-visible-melee-contact-fallback.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_025000-visible-melee-contact-fallback.md' |
| `docs/changelog/2026-11-20/20261120_033500-60hz-baseline-dashboard-presence-remote-culling.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_033500-60hz-baseline-dashboard-presence-remote-culling.md' |
| `docs/changelog/2026-11-20/20261120_034133-preserve-dashboard-scroll-position.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_034133-preserve-dashboard-scroll-position.md' |
| `docs/changelog/2026-11-20/20261120_040000-dashboard-keepalive-wake-runner-melee-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_040000-dashboard-keepalive-wake-runner-melee-evidence.md' |
| `docs/issues/bug/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/' |
| `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md' |
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
| `docs/specs/dev-loop/agent-wake.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/agent-wake.md' |

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

- Focused skill: dashboard route in `docs/ROUTER.md`; specification reviewed.
- Commands: `agent-coord sync --peek`, `board`, `issue show`, and source reads.
  No source check or build was needed because this turn made no source edit.
- Lease acquisition for `devscripts/dev_loop/gui.py` was rejected with the
  current holder and expiry; no lease was overridden.
- Runtime or visual evidence: none. Previous saved-geometry and secondary
  monitor keepalive changes are present in source, but the remaining maximize
  path has not been changed or visually verified.
- Output: issue quote and `dashboard-monitor-maximize` task are in the
  coordination brain; task remains open and unproven.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: no.
- The primary-monitor maximize path is confirmed by source inspection; a user
  visible failure was not reproduced in the running dashboard.
- Related issue: `issue-6772ff949164`.

# Human acceptance

- Visual review: not performed; the active dashboard was not restarted.
- Gameplay review: not applicable.
- Multiplayer review: not applicable.
- Still unverified: maximize/restore on secondary monitor and position
  persistence after dragging between monitors.

# Agent handoff context

- Owner: dashboard-scroll-refresh
- Focus: Dashboard position stability across monitors
- Files to check: `devscripts/dev_loop/gui.py` `toggle_maximize()` and
  `devscripts/dashboard-keepalive.py` `restore_window()`.
- What another agent should not redo: saved geometry and all-monitor
  keepalive changes are already in source from the prior session.
- Next exact step: after the `gui.py` lease releases, use the current monitor's
  work area for maximize and verify move/maximize/restore across monitors.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf / d4898e0b41b0fc93f9f3dd1dd1c596c4fe923ecf
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-20T02:29:08.406738Z", "latest_successful_build_id": "2026-11-20T02:29:08.406738Z:1026e13afa4215122cb82cf069a8d1b4fee1e887b6b4d224d97163dec8085755", "latest_successful_source_generation": 57}, "game_current": {"build_mode": "debug", "code_hash": "b6d1cbf2d5fc97708361731b510da03fff337946637031ffceeb93a5ae0ee086", "dll": "build\\game\\generation-393\\game-g393.dll", "generation": 393, "revision": "27794bbe21a024a8e8db5cd164d1a59ac357b45551c97e83765dec4f56727ef1", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-393\\game-g393.dll", "generation": 393, "published_utc": "2026-11-20T03:36:21.219Z", "revision": "27794bbe21a024a8e8db5cd164d1a59ac357b45551c97e83765dec4f56727ef1", "status": "ready"}, "progress": {"compiled": 5, "elapsed_s": 577.909, "phase": "build_complete", "result": "success", "skipped": 82, "updated_utc": "2026-11-20T02:11:17.625Z"}}
- Generation: 393
- Artifact: build\game\generation-393\game-g393.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_000300\client-01\events.jsonl
- Session ID: session-20261120-040037

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
 M devscripts/agent-runtime.py
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/dev-loop-watch.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/game_build_worker.py
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
 M src/network/server-packet-handlers.cpp
 M src/network/server-packets.cpp
?? devscripts/agent-wake.py
?? devscripts/dashboard-keepalive.py
?? docs/changelog/2026-11-20/20261120_021700-dev-loop-coordination-panel.md
?? docs/changelog/2026-11-20/20261120_023700-agent-proof-ledger-spec-and-gate.md
?? docs/changelog/2026-11-20/20261120_025000-visible-melee-contact-fallback.md
?? docs/changelog/2026-11-20/20261120_033500-60hz-baseline-dashboard-presence-remote-culling.md
?? docs/changelog/2026-11-20/20261120_034133-preserve-dashboard-scroll-position.md
?? docs/changelog/2026-11-20/20261120_040000-dashboard-keepalive-wake-runner-melee-evidence.md
?? docs/issues/bug/
?? docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
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
?? docs/specs/dev-loop/agent-wake.md
```

## Captured diff stat

```text
AGENTS.md                                          |  15 +-
 config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 devscripts/agent-runtime.py                        |  77 ++
 devscripts/agent_coord.py                          | 780 ++++++++++++++++++++-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop-watch.py                       |  33 +
 devscripts/dev-loop.py                             | 205 +++++-
 devscripts/dev_loop/cli.py                         |  12 +-
 devscripts/dev_loop/game_build.py                  |   9 +-
 devscripts/dev_loop/gui.py                         | 524 ++++++++++++--
 devscripts/dev_loop/parallel_test_runs.py          |  22 +-
 devscripts/game_build_worker.py                    |  28 +-
 devscripts/quality/test_agent_coord.py             | 142 ++++
 docs/ROUTER.md                                     |   7 +
 ...dination-brain-and-build-fan-out-attribution.md |  68 +-
 docs/issues/README.md                              |  21 +-
 ...-build-request-is-waiting-issue-346c68de182d.md |  14 +
 ...o-intake-and-warm-candida-issue-67f2be3d7012.md |   6 +
 docs/specs/dev-loop/agent-coordination.md          |  68 +-
 docs/specs/dev-loop/agent-self-improvement.md      |  73 ++
 docs/specs/dev-loop/dashboard.md                   |  74 +-
 docs/specs/dev-loop/dev-loop.md                    |   7 +
 docs/workflows/issue-intake-and-triage.md          |  41 +-
 src/combat/melee-client.cpp                        |  47 +-
 src/engine/engine-tick-render.cpp                  |  24 +-
 src/network/server-packet-handlers.cpp             |  52 +-
 src/network/server-packets.cpp                     |  15 +-
 28 files changed, 2219 insertions(+), 149 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	devscripts/agent-runtime.py
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/dev-loop-watch.py
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/game_build_worker.py
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
M	src/network/server-packet-handlers.cpp
M	src/network/server-packets.cpp
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
