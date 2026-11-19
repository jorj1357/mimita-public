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
- Automatic generation `auto-14` failed because the diagnostic referenced an
  unavailable NPC map in `broadcastNpcFiring`; that lookup was corrected to
  use its owned `NpcSystem`. The dev-loop then accepted automatic generation
  `auto-15`; its last status was `building`, so no successful DLL build or
  activation is claimed.
- At the user's request, launched mode 32, “duel npc elite,” as run
  `20261119_013346` on `xxity2` with one NPC. The root loop replaced the prior
  Juggernaut test run. Server PID 35412 and client PIDs 33704 (`NPC Dev`) and
  14368 (`NPC Hitscan Blood Trace`) connected to port 51326. All ran EXE build
  `2026-11-19T00:53:35.016823Z`; the new DLL diagnostics were not in that
  module.
- Baseline journal snapshot: server `npc.shot`=137, including 12 revolver, 4
  shotgun, 28 hitscan-rifle, and 22 big-shotgun shots. It recorded 11
  `server.damage.applied` hitscan events and 6 rocket-explosion damage events.
  Client `NPC Dev` recorded 15 `client.npc.damage-visible` and 15
  `presentation.local_victim_damage` events. This proves hitscan damage was
  applied in this run, but does not prove the reported blood-without-server-hit
  case absent or reproduced: the pre-fix client had no pellet-presentation
  trace. The named client's journal also recorded confirmed damage events.

## Runtime status

No after-fix runtime evidence is available yet. The currently running Duel
clients use the old module while automatic generation `auto-15` is building.
After a successful publish and activation, compare server and client events by
shot serial and tick: `npc.shot`, `server.npc.pellet-entity-impact`,
`client.pellet-entity-impact-presented`, `npc.damage`, and the authoritative
damage confirmation/health change.
