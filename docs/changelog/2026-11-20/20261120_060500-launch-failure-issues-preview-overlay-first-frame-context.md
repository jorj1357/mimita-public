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

