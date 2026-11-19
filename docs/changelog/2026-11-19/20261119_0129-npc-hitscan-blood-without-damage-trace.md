# NPC hitscan blood without damage trace

## Source changes

- `src/npc/npc-combat.cpp` adds shot outcome, mirror health delta, and trace
  endpoints to the existing `npc.shot` event.
- `src/network/server-npcs.cpp` transfers mirror damage on cached-target ticks
  by resolving the authoritative player/NPC from the cached target id. It logs
  unresolved mirror damage, enriches the existing `npc.damage` event, and
  logs shotgun entity-impact packets with their target and shot serial.
- `src/network/multiplayer-shots.cpp` logs remote pellet entity impacts before
  client hit-effect presentation, including pellets tagged for the local player
  and the zero damage value passed to the effect path.
- Regenerated the derived packet registry after its check reported a stale
  ledger.

## Evidence

- Before instrumentation, run `20261119_010408` (Duel NPC) logged 345
  `npc.shot` events and 34 server-applied/client-visible damage events. It did
  not identify a blood-effect-without-server-damage shot, so it is not proof of
  the reported failure.
- The first dev-loop compile attempt failed in `src/network/server-npcs.cpp`
  with a compiler subprocess error, `fatal error: stdout: Bad file descriptor`;
  it was not a C++ diagnostic. An automatic retry then stopped because the
  derived packet ledger was stale. The ledger/header were regenerated.
- The root dev-loop accepted automatic game DLL generation `auto-14` after the
  packet files were regenerated. At the last allowed status observation it was
  still `building`; no successful DLL build or activation is claimed.
- The active run `20261119_012255` is Juggernaut with zero NPCs and runs EXE
  build `2026-11-19T00:53:35.016823Z`. Starting a second server was rejected
  because the dev-loop supports only one active server in this run. That run
  cannot reproduce incoming NPC hitscan and was left running.

## Runtime status

No after-fix runtime evidence is available yet. To complete validation, the
root dev-loop must publish and activate the new module, then launch a fresh
NPC-bearing Duel run/client with a distinct test name. Compare server and client
events by shot serial and tick: `npc.shot`, `server.npc.pellet-entity-impact`,
`client.pellet-entity-impact-presented`, `npc.damage`, and the authoritative
damage confirmation/health change.
