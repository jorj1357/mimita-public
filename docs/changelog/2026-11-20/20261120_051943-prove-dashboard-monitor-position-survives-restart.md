// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Prove dashboard monitor position survives restart

# Task

- Task ID: session-376cc2b99544
- Session ID: session-376cc2b99544
- Owner: dashboard-position-proof
- Focus: devscripts/dev_loop/gui.py
- Summary: Persisted Win32 window bounds for negative-coordinate displays, fixed dashboard keepalive process discovery, and repeatedly closed/reopened the live dashboard. Three cycles and maximize/restore stayed on DISPLAY5; issue proof is verified and the regression record updated. The separate reusable small-desktop proof-standard issue remains open. Concurrent working-tree changes are not part of this session.
- Status: complete
- Date, time, timezone: 2026-11-20T05:19:43+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: ce0009d5b2983f462d1d17dd640c4e66769c2a23
- Final commit: ce0009d5b2983f462d1d17dd640c4e66769c2a23

# Pre-existing changes

- The shared checkout contained extensive ongoing work by other agents and the user before this task. Those changes were preserved and are listed in the captured git state below.
- This session's source changes are limited to `devscripts/dev_loop/gui.py` and `devscripts/dashboard-keepalive.py`; its regression record and index are also updated. The two dashboard issue records were maintained by `agent-coord.py`.
- No unrelated files were reverted or claimed.

# Requested behavior

- Track the dashboard position snap as an issue; investigate its cause; close and relaunch the GUI repeatedly; count it fixed only after the window stays on the chosen monitor. Keep a separate open issue for making small desktop behaviors visible to agents and requiring observable proof before calling them fixed.

# Specification alignment

- Followed `docs/specs/dev-loop/dashboard.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-proof-and-verification.md`, and `docs/workflows/issue-intake-and-triage.md`.
- Saved and restored actual Win32 window bounds in virtual-screen coordinates, including negative monitor positions. Keepalive uses the real dashboard role markers and excludes its own PowerShell query from process discovery.
- The monitor snap issue has a verified integration evidence record and is closed. The generalized small-desktop evidence-standard issue remains open, with its acceptance criteria recorded for follow-up.

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `devscripts/dev_loop/gui.py`

- Functions: `_monitor_work_area`, `_window_bounds`, `_tk_geometry`, `persist_window_geometry`, and `toggle_maximize`.
- Old behavior: maximization targeted the primary monitor; negative offsets were rejected by the geometry persistence regex and a restart returned to the primary display.
- New behavior: maximization uses the containing monitor work area; actual top-level Win32 x/y/width/height are persisted and restored, including negative x; monitor lookup failure leaves position unchanged.
- Reason: preserve the user's chosen dashboard monitor across movement, maximize/restore, and restart.
- Unrelated behavior: collapse state and other dashboard controls retain their existing ownership and behavior.

## File: `devscripts/dashboard-keepalive.py`

- Function: `dashboard_processes`.
- Old behavior: only `--dashboard` was matched, so `--dev-loop-role=dashboard` was missed; the PowerShell query could match its own command line.
- New behavior: match both dashboard role forms, require the Python process name, and exclude the current PowerShell PID.
- Reason: avoid false missing-window reports and unnecessary restart attempts.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `devscripts/agent_coord.py` |  |  |  | git checkout -- devscripts/agent_coord.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dashboard-keepalive.py` |  |  |  | git checkout -- devscripts/dashboard-keepalive.py |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/cli.py` |  |  |  | git checkout -- devscripts/dev_loop/cli.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md` |  |  |  | git checkout -- docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md |
| `docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md` |  |  |  | git checkout -- docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/specs/dev-loop/agent-self-improvement.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-self-improvement.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/networking/contact-authority.md` |  |  |  | git checkout -- docs/specs/networking/contact-authority.md |
| `src/config/collision-config.h` |  |  |  | git checkout -- src/config/collision-config.h |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `devscripts/dev_loop/launch_report.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/dev_loop/launch_report.py' |
| `devscripts/quality/test_launch_report.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_launch_report.py' |
| `devscripts/tech-debt-audit.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/tech-debt-audit.py' |
| `docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md' |
| `docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md' |
| `docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md' |
| `docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md' |
| `docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md' |
| `docs/issues/architecture/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/architecture/' |
| `docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md' |
| `docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md' |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md' |
| `docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md' |
| `docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md' |
| `docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md' |
| `docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md' |
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md' |
| `docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md' |
| `docs/issues/tech-debt/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/tech-debt/' |
| `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md' |
| `docs/specs/architecture/` |  |  |  | Remove-Item -LiteralPath 'docs/specs/architecture/' |
| `docs/specs/dev-loop/launch-feedback.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/launch-feedback.md' |
| `docs/specs/dev-loop/visual-proof-screenshots.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/visual-proof-screenshots.md' |
| `docs/specs/performance/culling-and-work-avoidance.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/performance/culling-and-work-avoidance.md' |

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

- `python -m py_compile devscripts/dev_loop/gui.py devscripts/dashboard-keepalive.py` — passed.
- `git diff --check -- devscripts/dev_loop/gui.py devscripts/dashboard-keepalive.py docs/regressions/dev-loop/README.md docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md` — passed (Git emitted only its LF-to-CRLF working-copy notice for keepalive).
- `python devscripts/changelog.py --check --session-id session-376cc2b99544` — passed; exactly one session changelog.

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

- DISPLAY5 bounds: `[-1920,0,0,1080]`; work area `[-1920,0,0,1032]`.
- After move, persisted geometry: `880x760+-1920+25`; persisted Win32 bounds: `x=-1920,y=25,width=880,height=760`.
- Maximize and restore: maximized at `[-1920,0]`, `1920x1032`; restored to `[-1920,25]`, `880x760`.
- Three close/reopen cycles yielded dashboard PIDs `27672`, `14700`, and `26504`. Each keepalive `dashboard.healthy` event showed visible bounds `[-1920,25,-1040,785]`; saved geometry stayed on DISPLAY5.
- Proof records: `task-proof-ed45c1b9e923` and issue evidence `evidence-7acdda6eaee9` are proven. The GUI was left running on DISPLAY5.

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Updated `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md` and Entry 30 in `docs/regressions/dev-loop/README.md` to FIXED with the live evidence. The dashboard monitor-snap issue is closed after verified integration proof. The reusable proof-standard issue remains open.

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Runtime behavior was exercised directly on the user's desktop through three GUI close/reopen cycles and a maximize/restore cycle. No additional human acceptance was claimed.

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Follow up on `issue-0cdac76fb006` / `small-desktop-state-evidence-standard`: document and pilot a reusable intake/evidence checklist for small persistent UI behavior, using this dashboard case as an example. Its proof remains unverified until that documentation/coordination work is completed.

- Owner: dashboard-position-proof
- Focus: devscripts/dev_loop/gui.py
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / ce0009d5b2983f462d1d17dd640c4e66769c2a23 / ce0009d5b2983f462d1d17dd640c4e66769c2a23
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-20T02:29:08.406738Z", "latest_successful_build_id": "2026-11-20T02:29:08.406738Z:1026e13afa4215122cb82cf069a8d1b4fee1e887b6b4d224d97163dec8085755", "latest_successful_source_generation": 57}, "game_current": {"build_mode": "debug", "code_hash": "b1e8824cc320422d440880ae80f1e1bf7c91b76b8cf20983ddd3a7f7126062be", "dll": "build\\game\\generation-394\\game-g394.dll", "generation": 394, "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-394\\game-g394.dll", "generation": 394, "published_utc": "2026-11-20T04:05:35.363Z", "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0", "status": "ready"}, "progress": {"compiled": 5, "elapsed_s": 577.909, "phase": "build_complete", "result": "success", "skipped": 82, "updated_utc": "2026-11-20T02:11:17.625Z"}}
- Generation: 394
- Artifact: build\game\generation-394\game-g394.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_043123\client-01\events.jsonl
- Session ID: session-376cc2b99544

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
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/hot-reload/packet-ledger.json
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/dashboard-keepalive.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/gui.py
 M docs/ROUTER.md
 M docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
 M docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
 M docs/regressions/dev-loop/README.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/networking/contact-authority.md
 M src/config/collision-config.h
 M src/gamemode/gamemode.h
?? devscripts/dev_loop/launch_report.py
?? devscripts/quality/test_launch_report.py
?? devscripts/tech-debt-audit.py
?? docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md
?? docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md
?? docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md
?? docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md
?? docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md
?? docs/issues/architecture/
?? docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md
?? docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md
?? docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
?? docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
?? docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
?? docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
?? docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md
?? docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
?? docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
?? docs/issues/tech-debt/
?? docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md
?? docs/specs/architecture/
?? docs/specs/dev-loop/launch-feedback.md
?? docs/specs/dev-loop/visual-proof-screenshots.md
?? docs/specs/performance/culling-and-work-avoidance.md
```

## Captured diff stat

```text
AGENTS.md                                          |  10 ++
 config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |   4 +-
 config/hot-reload/packet-ledger.json               |  12 +-
 devscripts/agent_coord.py                          |  17 +++
 devscripts/blender-ai                              |   0
 devscripts/dashboard-keepalive.py                  |   6 +-
 devscripts/dev-loop.py                             |  92 ++++++++++----
 devscripts/dev_loop/cli.py                         |   6 +
 devscripts/dev_loop/gui.py                         | 134 +++++++++++++++++----
 docs/ROUTER.md                                     |   2 +
 ...dination-brain-and-build-fan-out-attribution.md |  17 ++-
 .../20261120_040000-live-dashboard-build-health.md |  19 ++-
 docs/issues/README.md                              |  12 ++
 ...tency-limbs-only-capsule--issue-da337d1fe2f3.md |  32 +++++
 docs/regressions/dev-loop/README.md                |   9 +-
 docs/specs/dev-loop/agent-self-improvement.md      |  42 +++++++
 docs/specs/dev-loop/dashboard.md                   |  13 ++
 docs/specs/networking/contact-authority.md         |  86 +++++++++++++
 src/config/collision-config.h                      |   6 +-
 src/gamemode/gamemode.h                            |   4 +-
 22 files changed, 463 insertions(+), 64 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/hot-reload/packet-ledger.json
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/dashboard-keepalive.py
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/gui.py
M	docs/ROUTER.md
M	docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
M	docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
M	docs/regressions/dev-loop/README.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/networking/contact-authority.md
M	src/config/collision-config.h
M	src/gamemode/gamemode.h
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
