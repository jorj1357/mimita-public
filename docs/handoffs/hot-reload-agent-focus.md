# Hot-reload agent focus

Temporary coordination scratchpad. This is not a specification and not a source
of truth. The current specification is
`docs/specs/hotreload/hotreload.md`.

## Build authority

The root dev-loop is the only build loop. No agent owns, requests, or waits for
a build. Every agent edits and saves; the dev-loop watcher queues the build. No
agent invokes a compiler, `game_build_worker.py`, a build request, or a second
watcher directly.

One build at a time. One running generation at a time.

Allowed agent checks:

```text
python devscripts/dev-loop.py --status
python devscripts/dev-loop.py --ensure-running
```

Deterministic hot-client test:

```text
python devscripts/dev-loop.py --request launch --launch-mode 57 --hot-client
python devscripts/dev-loop.py --request start-client --hot-client
```

## Focus

### Agent A

- specification and documentation cleanup;
- ABI and host boundary;
- ensure the single root dev-loop is online and observe its status;
- status/audit/changelog tooling;
- integration;
- runtime acceptance;
- final merge.
- Owns the networking Agent A split for this phase: ICE hot/cold policy,
  server session/pump policy, network ABI co-ownership, source-owner cleanup,
  and verification. Follow `docs/handoffs/networking-hot-boundary-split.md`.

### Agent B

- reconnect, health, reconnect/token events, and client transport continuity;
- `multiplayer-packets.cpp` reconnect/health functions;
- `connection-health.*`, `network-events.*`, `networkingconfig.*`, and
  `network_trace.py`;
- server packet reconnect/token fields without co-editing Agent A's ICE files;
- server packet handling and validation except the ICE/server-pump split owned
  by Agent A;
- resident fallback removal and server runtime proof where not conflicting.
- Follow `docs/handoffs/networking-hot-boundary-split.md` for the confirmed
  Cutover-1 networking gap and file ownership.

### Agent C

- hot-client default and presentation/render proof first;
- black screen, viewport, GL-state, and swap acceptance;
- then client networking proof;
- do not edit `packets.*` or B's server pump.
- Client packet handlers already live in game-owned
  `multiplayer-tick.cpp` / `multiplayer-packets.cpp`; there is no separate
  handler-copy task.
- **Hot-client result:** the hot frame and connection work. Run
  `20261115_030654` launched a normal client (`hot_client=false`) with the latest
  EXE, loaded generation 169 directly, reached `Connected` as local player 1,
  rendered `path=dll`, and reached 8,600+ frames. The earlier hot6 disconnect was
  a build-identity rejection, not a hot-frame defect. Evidence:
  `logs/2026-11-15/20261115_030654/client-01/events.jsonl`.
- **Hot client is now the default:** `src/main.cpp` no longer requires
  `MIMITA_GAME_HOT_CLIENT`; it sets `MIMITA_GAME_OWN_FRAME=1` whenever a valid
  generation exists. `MIMITA_GAME_DISABLE=1` remains the resident escape hatch.
- **Different-DLL swap:** proven for generations 168 and 170. Generation 170
  activated live at sequence 27336 in run `20261115_030654`; the connection
  stayed active and frames resumed at 2,600+.
- **Swap diagnostic defect fixed:** generation 168 showed false watchdog stalls
  after unload because the module-owned watchdog thread survived the swap.
  `shutdownWholeGameEntry()` now stops the watchdog before unload. Generation
  170 swapped without the stale `last_completed_frame=44883` reports; only the
  expected short bootstrap stalls remained.
- **Next blockers:** rejected hot generations still fall back to legacy
  rendering. Remove the silent resident fallback, add the automatic hot
  acceptance gate, and perform the EXE cutover. Server split, state domains, and
  remaining safe-unload work stay open.

## Shared rules

- B and C may both work on networking.
- Do not edit the same file at the same time.
- Split shared files by function, or ask A to arbitrate.
- Record finished work in `docs/changelog/` with the exact files, build identity,
  runtime evidence, remaining work, and next step.
- Do not add claims JSON or lock files. The old `hot-reload-work-plans.json` and generated claim views are historical only.
