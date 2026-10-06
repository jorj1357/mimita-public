# Preserve the dev-loop server when leaving a client match

Time (EST): `2026-10-05T20:19:00-04:00`
Branch: `afad20a-rebuild`

## Result

The dev-loop client now identifies the dedicated server as dev-loop-owned.
Client-side duel/leave cleanup no longer stops that server. Leaving a match can
disconnect or change the client state, but it does not terminate the durable
dev-loop server or claim ownership of its console.

GUI-created servers keep their previous behavior: a normal GUI client without
the marker can still stop the server through `stopExternalServerProcess()`.

## Root cause

The Python loop already kept its dedicated server outside `self.processes`, but
the launched client could enter `DuelQueue::stopQueue()`,
`DuelQueue::returnToQueue()`, `DuelQueue::handleClientMatch()`, or queue retry
cleanup. Those paths called the GUI-owned `stopExternalServerProcess()` helper
unconditionally. That mixed two server ownership models and could close the
server when the client left.

## Changes

- `devscripts/dev-loop.py:863-870`
  - The client environment now includes `MIMITA_DEV_LOOP_SERVER=1`.
  - The existing shared `MIMITA_EVENTS_FILE` behavior is preserved.
- `src/duel/duel-queue.cpp:45-60`
  - Added one ownership check and one cleanup wrapper.
  - When the marker is present, cleanup logs
    `[DUEL QUEUE] preserving dev-loop-owned server on client leave` and does
    not stop the external server.
  - Without the marker, cleanup still calls the GUI stop helper.
- `src/duel/duel-queue.cpp:184,234,264,356,426,438`
  - Routed all six client-side external-server cleanup call sites through the
    ownership-aware wrapper.
- `docs/regressions/2026-09-29/dev-loop-server-closes-REG.md`
  - Appended Attempted Fix 3 with the cause, correction, and acceptance test.

## Documents and review

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- Existing regression: `docs/regressions/2026-09-29/dev-loop-server-closes-REG.md`

Specification review result: `PASS_WITH_HUMAN_REVIEW`. The requested behavior
matches the networking requirement that the server is an independent
authoritative process, but live client/server exit behavior still needs direct
human acceptance.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check -- devscripts/dev-loop.py src/duel/duel-queue.cpp docs/regressions/2026-09-29/dev-loop-server-closes-REG.md`: passed.
- Forced the focused `duel-queue.cpp` translation unit to rebuild after the
  first incremental check skipped it.
- `python build_agent.py`: `Status: SUCCESS`, return code `0`, duration
  `9.31s`; compiler output included `[CXX ] src\\duel\\duel-queue.cpp`.
- No running MiMITA process was terminated by this work.

## Human acceptance still needed

Restart the currently running dev-loop so it loads the new Python code, then
launch a client from it. Leave the match and verify:

1. the client remains open or returns to its expected menu/queue state;
2. the dedicated server console remains open and continues heartbeats;
3. the next client launch reuses the same room/server; and
4. the client log contains the preservation line when a duel-queue cleanup path
   is exercised.
