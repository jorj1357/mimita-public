# Fix dev-loop server crash

- Date: 2026-09-28 (UTC)
- Scope: dedicated server startup, NPC physics, and GUI-equivalent room-code join
- Status: code and smoke validation complete; live dev-loop join still needs human confirmation

## What happened

The dev loop was already using the GUI multiplayer sequence: start a dedicated
server, wait for its room file, launch the client with `--room`, and use the
normal ICE path. The server crashed before answering the ICE request, so the
client remained in `NatNegotiating` and displayed “waiting for server answer.”

## Confirmed cause

The fly feature added this client-only check to shared physics:

```cpp
if (MP_CONTEXT.active && MP_CONTEXT.flyEnabled)
```

`MP_CONTEXT` expands to `(*gpMpContext)`. Dedicated-server NPC simulation has
no client multiplayer context, so the first NPC physics update dereferenced a
null pointer. Windows recorded `0xc0000005` in
`.dev/builds/0355/mimita.exe`; the fault offset mapped to
`physicsMainUpdate_Internal` in `src/physics/physics-mini.cpp:196`.

## Fix

`src/physics/physics-mini.cpp` now checks the pointer before reading client
flight state:

```cpp
if (gpMpContext && gpMpContext->active && gpMpContext->flyEnabled)
```

Client flying remains enabled when a client context exists. Dedicated-server
and NPC physics safely skip that branch.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `MIMITA_FORCE_LINK=1 python build.py build-only`: passed and linked
  `mimita.exe`.
- Direct dedicated-server smoke test with `--npcs 1 --timeout 5`: passed. The
  server loaded the map, simulated the NPC, registered an ICE room, continued
  polling, and shut down normally after the timeout.
- Live human validation remains: restart `C:\mimita-v9\devscripts\dev-loop.py`,
  confirm the room code appears, and confirm the client completes ICE and joins.

Unrelated working-tree edits were preserved.
