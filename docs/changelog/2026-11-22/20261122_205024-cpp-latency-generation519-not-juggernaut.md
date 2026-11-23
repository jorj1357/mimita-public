# C++ latency pursuit: generation 519

At 2026-11-22 20:50 UTC, the root dev-loop was idle with successful published DLL generation 519 and no primary run. The isolated run `20261122_204150` remained online in Fun Counter Strike (server PID 19188, client PID 33848). Other agents retain ownership of shared gameplay work; this activation did not edit C++ inputs, request a build, stop a runtime, or start another game instance.

## Build evidence

- Failed candidate `game-dll-20261122T204037352482Z-fe5339`: preflight failed after 15.255 s because `findPathMs` and `straightPathMs` were undeclared. It did not link or publish.
- Successful candidate `game-dll-20261122T204052787084Z-49370a`: published generation 519 in 19.860 s.
- Compiled `src/npc/recast-navigation.cpp` in 10.364 s plus the module identity provider in 0.348 s; skipped 641 objects; processed 1,513 dependencies.
- Object cache scan: 1.735 s. LLD linked 642 objects / 481,271,074 bytes in 3.243 s. Publish: 0.388 s. `build_manifest`: 0.971 s.
- The successful wall time is about 1.99 times the 10 s target. No host or process CPU sample was aligned to the attempt, so this is not a measured resource-pressure result.

## Runtime evidence and limits

The live FCS run later emitted `game.transport.path` / `game.hotness.summary` for generation 519 using `path=dll`, `server_failures=0`, and ongoing server NPC phase-window events for 19 NPCs. NPC behavior decision events continued to identify module generation 517. This demonstrates that DLL generation 519 served live traffic while the unchanged NPC behavior module retained its stable identity. It does not prove that the changed navigation behavior was invoked or changed as intended, and FCS is not the requested Juggernaut/AI pressure workload.

The measurement and caveats were appended to latency issue `issue-136d4553ff92`. A coordination request was sent to the active networking owner for a safe single-TU canary window. Next acceptance requires a matched edit-to-server/client activation measurement under a verified Juggernaut workload, CPU sampling during the build, and an event proving the changed NPC function's behavior.

At 20:49:13 UTC the next root-loop attempt (`game-dll-20261122T204912875128Z-a273a6`) was rejected before worker entry: disk guard measured 1,215 MB free against a 2,048 MB minimum after automatic pruning. It took 476 ms and produced no compile/link timing or candidate. This blocker was appended to issue `issue-136d4553ff92`; no build artifacts were manually removed.
