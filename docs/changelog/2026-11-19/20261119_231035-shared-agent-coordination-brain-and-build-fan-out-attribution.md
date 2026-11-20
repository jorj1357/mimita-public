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
  devscripts.quality.test_path_containment
  devscripts.quality.test_manual_build_guard`: 26 tests passed.
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










