# Leave-match lifetime fix

Time: 2026-10-06 02:13 EST
Branch: current working branch (not changed)

## Result

The pause-menu Leave Room action now disconnects the client and returns to the
main menu without terminating the external dedicated server or closing the
client executable. Server shutdown remains owned by the explicit online-server
controls.

## Scope and pre-existing work

The worktree already contained broad unrelated edits and untracked assets,
configuration changes, navigation work, audio work, and a prior dev-loop
server-ownership change. Those changes were preserved. `devscripts/dev-loop.py`
and the existing Attempted Fix 3 edits in `src/duel/duel-queue.cpp` were
pre-existing before this session.

## Source change

- `src/gui/menus/pause-menu.cpp`, `leaveRoom()` active-duel branch: replaced
  `DuelQueue::returnToQueue()` with `DuelQueue::stopQueue()`, then explicitly
  transitions to `GAME_MENU` / `GUI_MENU_MAIN`. This makes Leave Room a client
  leave operation rather than a re-queue/server-lifecycle operation.
- `src/duel/duel-queue.cpp`, `DuelQueue::stopQueue()`: removed the external
  server termination call. `DuelQueue::returnToQueue()` now documents and
  follows the same independent-server boundary; fresh queue startup and
  retry/failed-connect paths retain their existing cleanup behavior.

## Specification and focused review

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/2026-09-29/dev-loop-server-closes-REG.md`

Finding: high-severity behavior regression. The client leave path reached
`stopExternalServerProcess()` through the duel queue cleanup owner, allowing a
client UI action to terminate the separate server process. The expected
networking lifetime is an independent authoritative server and visible client;
the corrected owner is the explicit server-control path.

Focused review result: PASS_WITH_HUMAN_REVIEW. Runtime confirmation is still
required.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: PASS.
- `git diff --check -- src/duel/duel-queue.cpp src/gui/menus/pause-menu.cpp devscripts/dev-loop.py`: PASS.
- `python build_agent.py`: PASS; `Compiled: 2`, `Skipped: 525`, link completed,
  return code 0. The linked root artifact was `C:\mimita-v9\mimita.exe`.

## Human acceptance still required

Start a fresh client/server pair from the rebuilt artifact, enter a match,
choose Pause → Leave Room → Yes, and confirm that the client remains open at
the main menu while the dedicated-server console and process remain alive.
Then confirm a later client can still join the retained room. No live gameplay
acceptance was performed by this source/build-only session.
