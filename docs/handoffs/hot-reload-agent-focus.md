# Hot-reload agent focus

Temporary coordination scratchpad. This is not a specification and not a source
of truth. The current specification is
`docs/specs/hotreload/hotreload.md`.

## Build authority

Agent A owns the build. Only A triggers the root dev-loop build. B and C edit and
save their claimed areas, then wait for A to build. No agent invokes a compiler,
`game_build_worker.py`, or a second watcher directly.

One build at a time. One running generation at a time.

## Focus

### Agent A

- specification and documentation cleanup;
- ABI and host boundary;
- dev-loop game-DLL build request;
- status/audit/changelog tooling;
- integration;
- runtime acceptance;
- final merge.
- May help networking when not conflicting with B or C.

### Agent B

- server networking;
- `server.cpp` pump/session split;
- ICE/session behavior;
- server packet handling and validation;
- resident fallback removal;
- server runtime proof.

### Agent C

- client networking;
- remaining client packet handlers;
- client packet dispatch and packet coverage;
- client runtime proof.
- If the black window reproduces, C handles it before client networking.

## Shared rules

- B and C may both work on networking.
- Do not edit the same file at the same time.
- Split shared files by function, or ask A to arbitrate.
- Record finished work in `docs/changelog/` with the exact files, build identity,
  runtime evidence, remaining work, and next step.
- Do not add claims JSON or lock files. The old `hot-reload-work-plans.json` and generated claim views are historical only.
