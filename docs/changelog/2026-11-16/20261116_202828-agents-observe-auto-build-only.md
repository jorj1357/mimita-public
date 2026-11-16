// 2026-11-16 15:28 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Agents observe auto-build only

# Task

- Task ID: session-20261116-202826
- Session ID: session-20261116-202826
- Owner: opencode
- Focus: dev-loop
- Summary: Make the root dev-loop the only build trigger, guard manual build requests to human override, archive the superseded live-development doc, and remove agent-build contradictions from current docs.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-16 15:28:47 EST (2026-11-16T20:28:47Z)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7ff11a91b7775d3dcdefdcdf487dd44983a53097
- Final commit: 7ff11a91b7775d3dcdefdcdf487dd44983a53097
- Uncommitted this session: CLI/daemon build guard, guard test, and final doc
  contradiction cleanup

# Pre-existing changes

Files present before this session and not owned by it:

```text
 M config/analytics.json
 m devscripts/blender-ai
 M devscripts/cold_change_guard.py
?? docs/changelog/2026-11-16/20261116_152334-devscripts-inventory-cold-change-guard.md
```

A concurrent agent committed this session's earlier documentation edits and the
host-runtime `.c_str()` fix as `7ff11a91`. This session did not create, revert,
or claim the concurrent `config/analytics.json`, `devscripts/blender-ai`,
`devscripts/cold_change_guard.py`, or devscripts inventory changelog.

# Requested behavior

1. Explain why the ready-manifest watcher needed a cold EXE build and whether it
   is hot now.
2. Remove current documentation contradictions: agents never run builds; they
   only ensure the single root dev-loop/auto-builder is running.
3. Archive `docs/architecture/live-development/live-development.md` because
   `docs/specs/hotreload/hotreload.md` supersedes it.
4. Add the recommended CLI and daemon guard so manual builds are human-only
   (`MIMITA_BUILD_OVERRIDE=1`).

# Specification alignment

- `AGENTS.md` gate 0: agents edit and observe; they never run builds or build
  requests; the root dev-loop builds saved inputs.
- `docs/specs/dev-loop/agent-lanes-build-authority.md`: root dev-loop is the
  only build/run authority; agents never invoke build scripts, compilers, or
  build requests.
- `docs/specs/dev-loop/dev-loop.md` §5: saved compiler inputs trigger the
  incremental build; no manual request is required.
- `docs/specs/hotreload/hotreload.md`: cold core and cold-to-hot promotion; a
  change is cold when its source owner is `resident`, `bootstrap`, or
  `launcher`. Superseded hot-reload documents are pointer stubs.
- `docs/specs/hotreload/hotreload.md` (actual path:
  `docs/specs/hotreload/hotreload.md`): hot behavior includes ordinary game
  behavior, tools, UI, and render policy.
- `docs/specs/dev-loop/agent-lanes-build-authority.md:273-291`: saved host edits
  queue an EXE build automatically; agents keep working.

# Exact implementation changes

## Archive: `docs/architecture/live-development/live-development.md`

- Old: full architecture document, including a stale narrow-hot-DLL statement.
- New: pointer stub to `hotreload.md`, generated `build/source-manifest.json`,
  and `agent-lanes-build-authority.md`.
- Historical content moved to
  `docs/archive/hotreload/live-development-2026-11-16.md`.
- Reason: `hotreload.md` is the current source-checked spec; the old doc
  conflicts with the current whole-game game DLL and generated ownership.

## Current reference updates

- `docs/ROUTER.md`: audio route now points to `hotreload.md`; build route adds
  `hotreload.md` and `agent-lanes-build-authority.md`, removes the archived doc.
- `docs/architecture/live-development/module-migration-plan.md`: removed the
  live-development-invariant link.
- `docs/architecture/live-development/hot-kernel.md`: related links now point
  to `hotreload.md` and build authority.
- `docs/architecture/live-development/project-layer.md`: no longer claims to
  replace `live-development.md`.
- `docs/architecture/live-development/hot-audio-contract.md`: specification
  reference is `hotreload.md`.
- `docs/operations/build-and-exe/build-and-exe.md`: Mode 3 reference is
  `hotreload.md`.

## Build-rule documentation

- `AGENTS.md` gate 0: explicit forbidden list (`build.py`, `build_agent.py`,
  `buildv2.py`, `module-build-worker.py`, `ccache`, compilers,
  `--request build`, `--request game-dll`, `--request cold-link-trial`,
  `--wait-for-build`) and allowed list (`--status`, `--list-instances`,
  `--inspect`, `--ensure-running`). A saved compiler input is the only normal
  build trigger; no second loop, no polling loop, no waiting.
- `docs/operations/build-and-exe/build-and-exe.md`: replaced the general
  `--request build --wait-for-build` entry points with the automatic-build
  workflow and a human-only manual override section.
- `docs/specs/dev-loop/dev-loop.md` §5.2.1: the daemon owns all build queuing;
  agents do not request or wait; only a human may use `MIMITA_BUILD_OVERRIDE=1`.
- `docs/specs/dev-loop/agent-lanes-build-authority.md`: removed “Agent A owns
  build requests”; no agent owns or issues builds; no `--wait-for-build` and no
  status polling loop.
- `docs/specs/gamemodes/payload.md`: replaced the canonical build command with
  auto-build-on-save + status/ensure + latest-artifact selftests.
- `docs/handoffs/hot-reload-agent-focus.md`: removed A-owns-build and the
  `--request game-dll --wait-for-build` block; allowed checks are status/ensure.
- `docs/handoffs/hot-reload-multi-agent-sync-20261113.md`: marked historical;
  replaced build/wait commands with status/ensure and auto-build.
- `docs/handoffs/cpp-game-dll-diagnostic-link-20261112.md`: marked historical;
  agents never run `game_build_worker.py`.
- `docs/specs/gamemodes/zombie-tower/implementing/zombie-tower-20261019.md`:
  validation step now uses status/ensure and auto-build.

## CLI guard: `devscripts/dev-loop.py`

- Added `_MANUAL_BUILD_ACTIONS = {"build", "game-dll", "cold-link-trial"}`.
- Added `_manual_build_override_enabled()` reading `MIMITA_BUILD_OVERRIDE=1`.
- Added `_build_request_authorized(requested_by)` accepting only `human` and
  `dashboard`.
- `run_control_client()` refuses manual build actions and `--wait-for-build`
  without the override, returning code 3 with the auto-build message.
- Manual requests that pass the override set `requested_by="human"`.
- `DevLoop.process_control_requests()` rejects manual build actions whose
  `requested_by` is not `human` or `dashboard`, records
  `dev-loop.build-request.refused`, and saves `request_denied` state.
- Automatic watcher builds and `_queue_build_if_needed()` are unchanged.
- Dashboard build requests already send `requested_by="dashboard"` and remain
  allowed.

## Help text: `devscripts/dev_loop/cli.py`

- `--request build/game-dll/cold-link-trial` is labeled human-only and requires
  `MIMITA_BUILD_OVERRIDE=1`.
- `--wait-for-build` is labeled human-only; agents never wait.

## Test: `devscripts/quality/test_manual_build_guard.py`

- Verifies the override environment gate.
- Verifies only `human` and `dashboard` are authorized.
- Verifies the CLI refuses `--request build` without the override.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `AGENTS.md` | no explicit command list | explicit agent allow/deny | one build trigger | `git checkout -- AGENTS.md` |
| `docs/ROUTER.md` | routed to archived live-development doc | routes to `hotreload.md` + build authority | remove stale authority | `git checkout -- docs/ROUTER.md` |
| `docs/architecture/live-development/live-development.md` | full stale doc | pointer stub | superseded | restore from archive |
| `docs/archive/hotreload/live-development-2026-11-16.md` | absent | archived full content | preserve history | delete file |
| `docs/architecture/live-development/hot-audio-contract.md` | old spec reference | hotreload reference | consistency | `git checkout -- <file>` |
| `docs/architecture/live-development/hot-kernel.md` | old related links | current links | consistency | `git checkout -- <file>` |
| `docs/architecture/live-development/module-migration-plan.md` | old invariant link | removed | consistency | `git checkout -- <file>` |
| `docs/architecture/live-development/project-layer.md` | names `live-development.md` | names `hotreload.md` | consistency | `git checkout -- <file>` |
| `docs/operations/build-and-exe/build-and-exe.md` | agent build entry | auto-build + human-only override | no agent builds | `git checkout -- <file>` |
| `docs/specs/dev-loop/dev-loop.md` | agents submit requests and wait | daemon-only queue | no agent builds | `git checkout -- <file>` |
| `docs/specs/dev-loop/agent-lanes-build-authority.md` | Agent A owns build requests | no agent owns builds | no agent builds | `git checkout -- <file>` |
| `docs/specs/gamemodes/payload.md` | canonical build command | auto-build + latest artifact | no agent builds | `git checkout -- <file>` |
| `docs/handoffs/hot-reload-agent-focus.md` | A-owns-build + game-dll wait | status/ensure only | no agent builds | `git checkout -- <file>` |
| `docs/handoffs/hot-reload-multi-agent-sync-20261113.md` | build/wait commands | historical + auto-build | no agent builds | `git checkout -- <file>` |
| `docs/handoffs/cpp-game-dll-diagnostic-link-20261112.md` | build command without banner | historical banner | no standing agent build | `git checkout -- <file>` |
| `docs/specs/gamemodes/zombie-tower/implementing/zombie-tower-20261019.md` | build/wait step | status/ensure + auto-build | no agent builds | `git checkout -- <file>` |
| `devscripts/dev-loop.py` | any caller could request build | human/dashboard guard | enforce policy | `git checkout -- devscripts/dev-loop.py` |
| `devscripts/dev_loop/cli.py` | neutral help | human-only help | document policy | `git checkout -- devscripts/dev_loop/cli.py` |
| `devscripts/quality/test_manual_build_guard.py` | absent | focused tests | guard proof | delete file |
| `docs/changelog/2026-11-16/20261116_202828-agents-observe-auto-build-only.md` | absent | this record | session history | delete file |

# Contract / ABI changes

- Contract: none. No C++ ABI, packet, or state schema changed.
- Version bump: none.
- Compatibility: unchanged game/host binaries.
- Reverse: revert the Python/doc changes.

# State / schema changes

- Domain: none.
- Old schema: n/a.
- New schema: n/a.
- Migration: n/a.
- Reverse: n/a.

# Deleted paths

- None. The superseded document was moved, not deleted.

# Diagnostics

- Owner/category: dev-loop control plane.
- Input: control request `action`, `requested_by`.
- Decision: manual build actions require `human` or `dashboard`.
- Output: refusal message plus `dev-loop.control` (status `rejected`) and
  `dev-loop.build-request.refused`.
- Failure or rejection reason: agent-originated build request.
- Rate limiting: per request; no repeated polling.

# Validation

- Focused skill paths and results:
  - `docs/skills/documentation-checker-v1.md` read; current contradictions were
    rewritten, historical changelogs/gold/archive left as history.
  - `docs/skills/logging-checker-v1.md` read; the daemon refusal is journaled
    with the reason and requested_by.
- Tests and exact commands:
  - `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop/cli.py devscripts/quality/test_manual_build_guard.py` -> PASS.
  - `python -m unittest devscripts.quality.test_manual_build_guard -v` -> 3 tests PASS.
  - `python devscripts/dev-loop.py --request build` without override -> exit 3
    and human-only refusal (covered by the test).
- Build status: no C++ source changed; no EXE or game-DLL build required.
- Runtime or hot-reload evidence: CLI guard is effective immediately. Daemon
  guard needs one dev-loop restart to load the new Python; not restarted in this
  session.
- Output files: none.

# Measured evidence

- Before values: current docs offered `--request build --wait-for-build` as a
  general command; the daemon defaulted unauthenticated requests to
  `requested_by="ai"`.
- After values: CLI refuses without `MIMITA_BUILD_OVERRIDE=1`; daemon refuses
  manual build actions from `ai`; dashboard and human overrides pass.
- Timestamps: 2026-11-16 15:28 EST.
- Tick/frame/network measurements: not applicable.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: policy/documentation
  contradiction, not a runtime behavior regression.
- Related regression paths: none.

# Human acceptance

- Visual review: not applicable.
- Gameplay review: not applicable.
- Multiplayer review: not applicable.
- Still unverified:
  - Daemon guard live after a dev-loop restart.
  - Dashboard build still accepted after restart.
  - Automatic watcher build still triggers after restart.

# Agent handoff context

- Owner: opencode
- Focus: dev-loop
- Files to check: `devscripts/dev-loop.py`, `devscripts/dev_loop/cli.py`,
  `devscripts/quality/test_manual_build_guard.py`.
- What another agent should not redo: do not add manual build requests to agent
  docs; do not let `requested_by="ai"` queue a build.
- Next exact step: restart the root dev-loop once through its control path, then
  run `python devscripts/dev-loop.py --request build` (expect refusal) and use
  the dashboard build button (expect acceptance).

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 7ff11a91b7775d3dcdefdcdf487dd44983a53097 / 7ff11a91b7775d3dcdefdcdf487dd44983a53097
- Git status: captured below
- Diff stat: captured below
- Build identity: no C++ build was requested or needed for this change.
- Generation: unchanged by this session
- Artifact: unchanged by this session
- Journal path: unchanged by this session
- Session ID: session-20261116-202826

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert 7ff11a91` (only for the commit containing these
  documentation edits; preserve concurrent work)
- Generated file: rerun its generator
- ABI/state/schema change: n/a
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: none.

## Captured git state

```text
 M config/analytics.json
 m devscripts/blender-ai
 M devscripts/cold_change_guard.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M docs/handoffs/cpp-game-dll-diagnostic-link-20261112.md
 M docs/handoffs/hot-reload-agent-focus.md
 M docs/handoffs/hot-reload-multi-agent-sync-20261113.md
 M docs/specs/gamemodes/zombie-tower/implementing/zombie-tower-20261019.md
?? devscripts/quality/test_manual_build_guard.py
?? docs/changelog/2026-11-16/20261116_152334-devscripts-inventory-cold-change-guard.md
```

## Captured diff stat

```text
 devscripts/dev-loop.py                              |  build guard
 devscripts/dev_loop/cli.py                          |  help text
 devscripts/quality/test_manual_build_guard.py       |  new focused tests
 docs/handoffs/... (current contradiction cleanup)   |  docs only
```

## Captured changed paths

```text
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	docs/handoffs/cpp-game-dll-diagnostic-link-20261112.md
M	docs/handoffs/hot-reload-agent-focus.md
M	docs/handoffs/hot-reload-multi-agent-sync-20261113.md
M	docs/specs/gamemodes/zombie-tower/implementing/zombie-tower-20261019.md
??	devscripts/quality/test_manual_build_guard.py
```

## Captured build identity

```json
{
  "build_required": false,
  "reason": "Python and Markdown changes only",
  "daemon_restart_required_for": "daemon-side build guard",
  "cli_guard": "active immediately on the next dev-loop.py invocation"
}
```
