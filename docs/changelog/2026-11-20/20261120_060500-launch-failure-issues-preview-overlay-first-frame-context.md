# Launch failure issues, preview overlays, first-frame event, and issue brief

## Date

2026-11-20

## Agent

`launch-feedback` / `coordination-v2`

## Summary

Closed four gaps in the launch and handoff loop:

- Every failed dev-loop launch now creates or appends a launch-friction issue
  from its failure signature. The issue records the launch ID, report path,
  build ID, generation, failure code, and failed stage. Recovered launches are
  not reported as failures.
- Preview build requests are accepted through the dev-loop bridge when they
  name a base generation and overlay paths. A preview snapshot hard-links the
  base snapshot and replaces only the named files. Preview builds do not
  publish shared current pointers.
- The client emits a one-shot `client.first-frame` event with elapsed seconds,
  launch ID, run ID, and map-ready state at the end of the first world render.
- `agent-coord.py issue brief <id>` returns one bounded handoff document with
  the issue, task proofs, leases, builds, runtimes, decisions, blockers, next
  action, causality, and missing required context.

The creator workflow now has a parent issue
`issue-565902eed8a7` with seven ordered slices covering project persistence,
authored geometry, editor controls, monster authoring, world modes/Infinite
XXITY, Play Test/My Creations, and end-to-end proof.

## Added or changed

- `devscripts/dev_loop/launch_report.py`: automatic launch issue creation and
  occurrence deduplication by failure title.
- `devscripts/dev-loop.py`: preview metadata passthrough; preview pointer
  suppression; launch issue creation on failed completion.
- `devscripts/dev_loop/game_build.py`: `build_metadata`, base-snapshot lookup,
  and overlay snapshot creation.
- `devscripts/agent_coord.py`: `ensure_agent`, `issue_occurrence`,
  `issue_occurrences` in `issue_show`, and `issue_brief`.
- `src/engine/engine-tick-render.cpp`: one-shot `client.first-frame` event.
- `docs/specs/dev-loop/agent-context-contract.md`: handoff contract.
- `docs/ROUTER.md`: route for universal issue context.
- `devscripts/quality/test_agent_coord.py`: brief and recurrence tests.
- `devscripts/quality/test_game_build_snapshot.py`: overlay replacement test.

## Validation

- `python -m unittest` for agent coordination, snapshot, dependency gate,
  launch report, and manual build guard: 50 tests passed.
- `python -m py_compile` passed for changed Python modules.
- The first game-DLL request was rejected with `disk_low` (1386 MB free,
  2048 MB required). Old unlocked generation directories were pruned, and
  `issue-916633ab3313` records the friction.
- Generation 397 published after the cleanup. Generation 395 contains the
  `client.first-frame` compile; source snapshot and compile succeeded.
- Isolated test runs pinned to 395 and 397 failed at server
  `logger.started`; the live launch run `20261120_065624` failed at client
  `logger.started` / mode start. No first-frame runtime proof was obtained.
- The running dev-loop daemon was started before the automatic launch-issue
  change, so its loaded code did not execute the new issue hook. The hook is
  covered by source inspection and tests, not by that daemon.

## Evidence boundary

Source and focused tests only. No DLL build, launch, runtime, visual, or human
acceptance is claimed.
## Autonomy restart follow-up

- Restarted the stale dev-loop daemon through the control path and verified
  exactly one watchdog and one daemon with a matching combined source hash.
- Added `docs/skills/dev-loop-restart-v1.md` and routed it from
  `docs/ROUTER.md`.
- Opened `issue-6fc8f853a674` with the user's verbatim request, readiness
  context, and five tasks for an always-available restart/self-recovery skill.
## Shared user-context bus and strict issues

- Added `user_context`, links, acknowledgements, and routing-correction records
  to the shared brain. Exact user text is immutable and deduplicated by stable
  source reference.
- Added `context add/list/ack/link/move/lint`; `sync`, `brain_check`,
  `issue brief`, status, and the dev-loop coordination summary expose context.
- Added the OpenCode prompt plugin with V1 `chat.message` and V2 `prompt`
  paths. Smoke-tested both paths against a temporary brain; both recorded the
  exact text and stable source reference.
- Normal context broadcasts to all agents as a `user-context` message and is
  acknowledged at sync. High-priority context also queues bounded wake requests
  for active agents.
- Added context-routing repair: `context move` deactivates the old primary link,
  appends a correction with reason, and notifies through the journal.
- Enforced strict new-issue fields: quote/observation, summary, reproduction,
  expected behavior, acceptance, paths, evidence, risk, and proof expectation.
  Added `devscripts/agent-issue.py` as the validated front-end and `issue lint`
  for legacy gaps.
- Added the always-available `.opencode/skills/dev-loop-restart/SKILL.md`.
- Created strict issues `issue-421c5be1669f` (live map/world credited forks),
  `issue-612ce59ed73d` (replay export/video), `issue-2ecdfa48ec44`
  (decentralized durable sharing), and `issue-22e043046cd0` (any-device input).
  Filled readiness fields for map editor, creator, autonomy, awareness, and
  issue-completeness issues.

## Validation

- 58 focused tests passed across agent coordination, launch report, snapshots,
  dependency gate, and manual build guard.
- Python compilation passed for changed modules.
- No game build or runtime was requested. This work proves coordination/tooling
  behavior only; it does not prove map/world live collaboration, replay export,
  monster authoring, Infinite XXITY, distribution, or device support.
## Live world save-as-fork slice

- Added `creatorProjectForkLive` and terminal commands
  `worldproject fork_live <new-id> [title]` / `save_fork`.
- The command writes the current authored map as a new immutable source
  version, forks that exact version into a new project with parent lineage,
  and leaves the active live project unchanged.
- Added `world-project.live-forked` with source/fork project and version,
  previous source head, parent version, owner, contributor, title, visibility,
  root, and `source_unchanged:true`.
- Added `devscripts/scenarios/world_live_fork_v1.json` and updated
  `world-projects` command/event docs.
- Generation 416 includes `src/mapeditor/map-editor-projects.cpp`; the published
  `build/game/generation-416/game-g416.dll` contains the `fork_live` command.
  Isolated run `20261120_232155` used generation 416 and executed
  `worldproject fork_live` against `world_live_fork_probe_live`; client journal
  `world-project.live-forked` reports the child project, source version,
  previous head, parent version, and `source_unchanged:true`. Task proof
  `task-proof-d6099e27411c` is verified.
## Build revision gate and dependency cache

- Whole-game requests now carry a source revision. Requests for one revision
  coalesce, newer pending revisions supersede older queued requests, and a
  failed revision is latched against automatic retry.
- Added one transient retry path for permission/publish failures once the
  environment clears; compile and codegen failures require a new revision or
  explicit retry.
- Added a persistent dependency index at
  `build/game/objects/<flags>/dependency-index.json`; unchanged depfiles reuse
  parsed dependencies and only changed sources/depfiles are parsed.
- Added pre-build aggressive pruning of logs, snapshots, and generations before
  the disk reserve check, and unique atomic temp names with retry for build
  timing/progress writes.
- Added `devscripts/quality/test_build_coordinator.py` and dependency-cache
  coverage in `test_game_build_snapshot.py`. 68 focused tests pass.
- No live build using the new coordinator has been measured yet; the current
  daemon must restart to load it. No speed improvement is claimed until the
  next measured build.
