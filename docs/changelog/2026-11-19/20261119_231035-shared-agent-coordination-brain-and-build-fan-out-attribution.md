// 2026-11-19
/* purpose
* record the shared agent coordination brain and build fan-out attribution work
* separate source/test evidence from build and runtime evidence
* preserve pre-existing concurrent checkout changes
*/

# Shared agent coordination brain and build fan-out attribution

# Task

- Task ID: agent-coord-brain-20261119
- Session ID: agent-coord-brain-20261119
- Owner: opencode
- Focus: shared agent coordination, build observability
- Status: `PASS_WITH_HUMAN_REVIEW`
- Date: 2026-11-19 23:12 UTC
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Pre-existing changes

The shared checkout already contained extensive concurrent work in build,
map-editor, Juggernaut, movement, networking, and documentation files. This
session preserved those edits and did not treat them as its own. The build
worker also contained pre-existing identity/cache work before the fan-out
attribution change.

# Requested behavior

Provide a shared persistent coordination brain for concurrent agents:

- one authoritative brain with named worker agents
- tasks, directives, dependencies, notes, decisions, votes, messages
- short edit leases and shared-file regions
- autonomous task-owner synthesis without waiting for user approval
- automatically published Markdown decision records
- separate canonical JSONL audit stream
- dev-loop read-only status/message views
- coordination-only halt/freeze
- `edit commit` releases immediately

Also add build-cause and header fan-out attribution and parallel dependency
stat collection to the whole-game build worker.

# Specification alignment

Added `docs/specs/dev-loop/agent-coordination.md`. It specifies:

- shared brain and named agent identity
- activation/turn behavior and next-activation delivery
- tasks, user directives, method conflicts, dependency cycles
- autonomous decisions and automatic stale/supersede behavior
- advisory votes and review documents
- edit leases, shared-file regions, proposals
- halt/freeze limits
- SQLite + JSONL persistence and separate game/journal separation
- dev-loop status integration
- acceptance and failure behavior

Added an AGENTS gate requiring `begin`, `sync`, edit leases, and `end`.
Added a routing entry for concurrent-agent coordination.

# Implementation

## `devscripts/agent_coord.py`

- SQLite WAL brain with tables for agents, tasks, dependencies, directives,
  notes, messages, leases, shared files, conflicts, options, votes, decisions,
  proposals, freezes, halt, and events.
- Canonical `AGENT_COORD` JSONL audit at `.dev/agent-coord/events.jsonl`.
- Named sessions and per-agent event cursors.
- Optional announcements; no mandatory claims.
- Short edit leases with `edit begin`, immediate release on `edit commit` and
  `edit abort`, and conflict recording on hash mismatch.
- Shared-file agreements with disjoint region validation.
- Message inbox, reply, ask/wait, and bounded wait.
- Task/directive/dependency tracking and method-conflict detection.
- Autonomous task-owner decisions; decision publication under
  `docs/coordination/decisions/`; review Markdown under `.dev/agent-coord/reviews/`.
- Advisory votes; decisions do not wait for unanimity or user approval.
- New relevant directives mark active decisions stale.
- Halt, freeze, and pre-commit guard.
- Proposal storage and apply/reject plumbing.

## `devscripts/agent-coord.py`

Thin executable wrapper for the module.

## `devscripts/dev_loop/cli.py` and `devscripts/dev-loop.py`

- Added `--coord-status`, `--coord-inbox`, `--coord-message`,
  `--coord-agent`, and `--no-coord`.
- Read-only coordination views return before normal dev-loop control.
- Normal read-only status appends unread/conflict/lease/halt and decision
  summary; failures are swallowed so coordination cannot break status.

## `devscripts/game_build_worker.py`

- Parses all depfiles once before freshness evaluation.
- Prewarms the dependency stat cache in a bounded thread pool.
- Reports `parse_ms` and `stat_ms` separately.
- Counts project headers by dependent object count.
- Records `build_cause`, changed sources, changed headers,
  `changed_header_fanout`, `header_fanout_top`, and `identity_wrapper_count`.
- No-op attempts report `build_cause=no_op`.

## Launch feedback, diagnostic reports, and failure taxonomy

- Added `devscripts/dev_loop/launch_report.py` with persisted launch timelines,
  failure-code classification, Markdown reports, and probe data.
- Added `dev-loop --launch-report`, `--last-launch-report`, and
  `--launch-doctor` read-only views.
- The dev-loop appends each launch stage to `.dev/launch-reports/<id>/` and
  records the first failed stage/code.
- Added the successful-launch contract (artifact identity, server/client,
  connection, map, mode, first frame, events).
- Added `docs/specs/dev-loop/launch-feedback.md` and router entry.
- Added focused launch-report tests.

## Build bridge, pointers, wake contract, runtime registration

- Added `agent-build` dev-loop request action with `--build-request`; the
  daemon validates the approved brain request, queues the game DLL, and updates
  request status.
- Added atomic per-agent and per-cohort pointer publication on successful
  builds (`build/game/agents|cohorts/<name>/current.json`).
- Rejected preview requests with a concrete reason until the overlay builder
  exists.
- Added `agent-runtime --pointer/--cohort/--issue`; pointer resolution and
  automatic runtime registration/stop updates.
- Added `agent-coord wake-check` with actionable/blocked/idle exit codes.
- Existing `agent-wake.py` adapter remains the model-agnostic wake queue.
- Added wait/no-wait and wake policy to coordination/build specs.
- Created/broadcast the waiting-principle issue and linked wake/wait tasks.

## Build queue, runtime registry, and cohort metadata

- Added `build_requests`, `build_owner`, and `runtime_registry` tables.
- Added `agent begin --parent --cohort --capability` and agent parent/cohort/
  capabilities columns.
- Added `build request/list/owner/approve/reject/wait` commands with wait
  reasons and build statuses.
- Added `runtime register/list/update` commands for generation/DLL/run/journal
  tracking; runtime stop remains explicit and does not kill unowned processes.
- Added build/runtime state to coordination status and board.
- Added tests for queues, runtime registration, and subagent cohorts.
- Created a high-priority build-friction issue for the zero-wait principle with
  the exact human quote, and broadcast a build/wake policy thread to other
  agents.

## Reviewable build/runtime/subagent/self-improvement specs

- Added `docs/specs/dev-loop/agent-build-queue.md`: build requests, rotating
  build owner, resource-aware concurrency, integration vs preview, per-agent/
  cohort DLL pointers, preview overlays, ABI boundary, and wait budgets.
- Added `docs/specs/dev-loop/agent-runtime-registry.md`: runtime identity,
  DLL pinning, health, warm runtimes, ownership, evidence links, and cleanup.
- Added `docs/specs/dev-loop/agent-subagents.md`: parent/cohort/subagent
  delegation, shared pointers, budgets, nested-depth limits, and authority.
- Extended `docs/specs/dev-loop/agent-self-improvement.md` with reversible vs
  critical autonomy classes, mandatory retro, improvement scanner, and trust
  growth.
- Linked the new specs from the router and coordination spec.
- Created issue-owned tracks for build queue, runtime registry, subagents,
  map editor slices, and spec review.

## Next work, observe bundle, and task proof gate

- Added `agent-coord next --agent` with priority sorting, ownership claim, first
  unblocked task, and blocked-issue reporting.
- Added `agent-coord observe <issue>` returning a bounded issue/task/quote/review/
  evidence/activity/build/runtime context bundle.
- Added task-level proof storage and `proof task-add`, `task-verify`, `exempt`,
  and `status`.
- `task_status done` now requires verified proof or an explicit exemption.
- Source hashes make verified proof stale when changed files move.
- Created issue-owned tracks for wake/context, runtime registry, subagents,
  map editor slices, and a risk issue for premature hot-layer migration.
- Added the hot-reload architecture guard to the coordination spec.

## Mandatory issue proof and trace

- Added `issue_evidence` storage and `proof record/verify/list`.
- `proof record` records build generation, DLL path/hash, runtime/run IDs,
  required events, server/client journals, and captures the proof kind.
- `proof verify` checks generation presence, DLL hash, journal existence,
  required events, and `scenario.finished` success.
- `issue close` now refuses without proven integration evidence or a unit proof.
- Added `issue quote` for exact user statements with support/oppose context;\n  `trace <issue>` includes issue, tasks, reviews, quotes, and evidence.
- Existing issue lifecycle now remains usable; tests were updated to prove
  before closing.

## Per-attempt snapshots and pinned runtimes

- `dev_loop/game_build.py` now creates an immutable per-attempt source tree at
  `.dev/build-snapshots/<attempt>/source` by hardlinking the verified source
  mirror. Mirror refreshes use temp-file replacement so existing snapshots keep
  their old content.
- The worker receives the per-attempt tree as `--source-root`; live edits after
  capture cannot alter that build.
- `_prune_snapshots` retains the newest five snapshot attempts.
- `agent-runtime.py` accepts `--agent`, `--game-generation`, and `--dll`, records
  generation/DLL hash in `run.json`, and sets `MIMITA_GAME_DLL` to the pinned
  DLL.
- `game_build_worker.py` records `source_root` and filters project header
  fan-out to `src/`, excluding GLM/GLFW vendor headers.

## Header fan-out gate

- `devscripts/dependency_graph.py` now has `--gate`, `--limit`, and
  `--allowlist`. It compares direct header fan-in against
  `config/header-fanout-allowlist.json`, fails unlisted headers above the limit,
  fails growth beyond an allowlisted maximum, and reports stale allowlist
  entries without failing.
- Path normalization collapses `src/debug/../config.h` to `src/config.h`.
- Baseline gate run on the current game object set: 1,539 depfiles, 642
  sources, 594 project headers; `ok=true`, 0 violations, 70 allowlisted wide
  headers.
- Top current fan-in: `src/pch.h` 629, `src/debug/structured-log.h` 338,
  `src/physics/physics-types.h` 299, `src/map/map_common.h` 286,
  `src/physics/movement/movement-types.h` 277.

## Map mesh header split

- Added `src/map/map-mesh-types.h` owning `Vertex`, `Face`, `MapGLBObject`, and
  `Mesh`/`Mesh::Batch`.
- `src/map/map_common.h` now owns only `AuthoredGLBBase` and includes the
  mesh-types header for compatibility.
- `src/entities/player.h` includes `map-mesh-types.h` instead of `map_common.h`.
- Narrowed the same include in avatar, combat, effects, render, and mesh utility
  consumers that do not use `MapGLBObject` or `AuthoredGLBBase`.
- Queued build notes `3ee1c2665eb9` and `200ac56d4ca2` for the next whole-game
  DLL attempt.

## Docs

- `docs/specs/dev-loop/agent-coordination.md`
- `docs/specs/dev-loop/build-time-observability.md`
- `docs/ROUTER.md`
- `docs/operations/devscripts/script-inventory.md`
- `docs/coordination/decisions/README.md`

# Validation

- `python -m py_compile` passed for:
  - `devscripts/agent_coord.py`
  - `devscripts/agent-coord.py`
  - `devscripts/dev-loop.py`
  - `devscripts/dev_loop/cli.py`
  - `devscripts/game_build_worker.py`
- `python -m unittest devscripts.quality.test_agent_coord
  devscripts.quality.test_game_build_snapshot
  devscripts.quality.test_dependency_graph_gate
  devscripts.quality.test_path_containment
  devscripts.quality.test_manual_build_guard`: 48 tests passed.
- CLI smoke tests:
  - begin/announce/status
  - task/directive/method conflict
  - edit lease conflict, failed commit release, second-agent acquisition
  - shared-region validation
  - dev-loop `--coord-status` and `--coord-inbox`
  - decision publication to a temporary test directory, then removed
- Build script was not run by this agent, per repository build authority. The
  worker changes are source/syntax/test evidence only. No build or runtime
  acceptance is claimed.

# Human review needed

- Review the autonomous-decision policy and decision Markdown format.
- Review whether the default 5/15/15 timeout values feel correct in use.
- Review whether separate coordination JSONL is acceptable versus any desired
  dev-loop journal mirroring.
- Confirm the shared-file region workflow in a real two-agent session.
- Confirm the build worker attribution appears in the next dev-loop build and
  that parallel stat does not regress dependency freshness.

# Changed files

- `devscripts/agent_coord.py` (new)
- `devscripts/agent-coord.py` (new)
- `devscripts/quality/test_agent_coord.py` (new)
- `docs/specs/dev-loop/agent-coordination.md` (new)
- `docs/coordination/decisions/README.md` (new)
- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/operations/devscripts/script-inventory.md`
- `docs/specs/dev-loop/build-time-observability.md`
- `devscripts/dev_loop/cli.py`
- `devscripts/dev-loop.py`
- `devscripts/game_build_worker.py`
