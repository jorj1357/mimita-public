# Stage G: acceptance matrix closeout

Date (UTC): 2026-09-23T19:45:00Z
Status: acceptance matrix run on the current build; all core selftests PASS; nothing deleted

## Scope

Run the Stage G acceptance/verification pass over the migrated networking code
on one build, confirm the end state, and record what remains. Per instruction,
nothing was deleted: the remaining EXE-owned files are marked LEGACY in the
manifest and kept in place.

## Evidence

Build under test: `mimita-20260923T193418.exe` (`python build_agent.py` ->
`Status: SUCCESS`).

Selftests (test evidence), all PASS:
- `--live-code-selftest` PASS
- `--packet-codec-selftest` PASS
- `--snapshot-chunk-selftest` PASS
- `--movement-selftest` PASS
- `--movement-parity-selftest` PASS
- `--transport-generation-selftest` PASS
- `--lagcomp-history-selftest` PASS
- `--capability-selftest` PASS
- `--generation-bootstrap-selftest` PASS
- `--actor-lifecycle-selftest` PASS
- `--npc-entity-selftest` PASS
- `--hot-authoritative-selftest` PASS
- `--reconciliation-policy-selftest` PASS
- `--interpolation-policy-selftest` PASS

Package surface (observed in `--live-code-selftest`):
`systems=39 commands=22 schemas=56 providers=28 requirements=23`.

19 networking capability providers resolve through the generic doorway:
`net.attack-claim`, `net.attack-gates`, `net.broadcast-interp`,
`net.client-snapshot`, `net.connection-policy`, `net.damage-application`,
`net.join-policy`, `net.kill-attribution`, `net.movement.validate`,
`net.npc-ground-clamp`, `net.npc-targeting`, `net.packet-codecs`,
`net.physical-contact`, `net.projectile-correction`, `net.projectile-splash`,
`net.reload-decision`, `net.respawn`, `net.session-policy`,
`net.snapshot-codecs`.

Known pre-existing failure (not a Stage A-G regression): `--hot-combat-selftest`
reports 25 `[FAIL]` lines, all in the animation/`phase2`/`action-state` family
caused by unrelated in-flight audio/animation edits in the working tree. None
reference a networking capability.

## End state (19-file list, nothing deleted beyond the two already-approved in Stage A)

- Deleted in Stage A (human-approved): `src/network/client.cpp`,
  `src/network/server-melee.cpp`.
- Fully hot / dispatch-only bridges: `src/network/snapshot-chunks.cpp`,
  `src/network/movement-validation.cpp` (listed under `bridges`).
- Hot policy with a legacy mechanism remainder (listed under `legacy` with a
  `why`): `server.cpp`, `ice-agent.cpp`, `packets.h`, `server-packets.cpp`,
  `server-npcs.cpp`, `server-players.cpp`, `server-projectiles.cpp`,
  `server-attack.cpp`, `server-physical-contact.cpp`, `server-damage.cpp`,
  `multiplayer-tick.cpp`, `multiplayer-projectiles.cpp`,
  `multiplayer-packets.cpp`, `multiplayer-reconcile.cpp`,
  `reliable-gameplay-events.cpp`. (15 files; manifest `legacy` count = 15.)

## Honest gaps / what remains

1. **`server.cpp` policy not moved.** Mode selection, NPC startup spawn,
   generation announce/quorum/switch, and the local-room/timeout policy are still
   implemented in `server.cpp`; it is marked legacy for its loop/socket/thread
   mechanism, but those policy blocks are a known remaining migration target
   (Stage F residual). This is the main behavior-policy exception to "policy is
   hot".
2. **Stage 0 listen-thread barrier not done.** The listen server ticks on its
   background thread while activation runs on the main thread; this is the one
   real concurrency gap and needs a listen-server-active accessor.
3. **Residual geometry/mechanism** (low value): attack body-part volume test and
   physical-contact contact-detection loops stay cold.
4. **Human/live acceptance** (two-client live reload, reload during NPC fire /
   projectile flight / ACK pending / reconnect / ICE gather / chunk reassembly /
   reconcile) is not performed by this automated pass.

## Note

Every repository-touching session creates exactly one final changelog; this is
the closeout for the Stage G acceptance run.
