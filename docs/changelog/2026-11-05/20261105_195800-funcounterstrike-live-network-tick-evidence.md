# Fun Counter Strike live network and tick evidence

Date: 2026-11-05 (local); runtime continued into 2026-11-06 UTC
Branch: `codex/agent-network-session-diagnostics-20261105`
Snapshot commit: `6acf629f8512f4fec045552ce8f54bc9d6069637`

## Scope and source changes

Continued the user-requested Mode 2 live investigation. No manual build was run. The Mode 2 dev loop performed the initial build and its incremental follow-up. The first attempt failed because the lane snapshot contained a partial grenade-bounce logger change: `src/pobjects/persistent-physics.cpp` passed `tick` to `checkCollisions`, while the header and definition had no tick parameter. The minimal consistency fix updated `src/pobjects/persistent-physics.h` and `.cpp` to pass `player.movementSimulationTick`. The incremental follow-up compiled successfully (49 compiled, 564 skipped; 274.51 seconds).

The snapshot already contained structured diagnostics for session removal (`src/network/server-packets.cpp:433`; explicit removal call at `:1530`; reconnect grace removal in `src/network/server-packet-chat.cpp:1510`), projectile cost (`src/network/server-projectiles.cpp:2075`), NPC slow updates (`src/npc/npc.cpp:3188`), and server tick stalls (`src/network/server.cpp:1176` in the published executable's event metadata). This session did not author those diagnostics. The `npc.tick-cost` records were present in the runtime journal; their authorship is not attributed here.

## Build identity and runtime

Mode 2 published `C:\mimita-v9-agent-lanes\network-session-diagnostics-20261105\.dev\builds\2026-11-06T00-47-13.708652Z\mimita.exe`, build ID `2026-11-06T00:47:13.708652Z:f5493b7769ba83241c79abd6f8be632a739011040500d1f7ace37616350ffe61`. Exact `--versioninfo` reported an events path. Live server and client used that same build ID.

Dev-loop run `20261106_004753` launched mode 45 / Fun Counter Strike on `dust2cyberiav4`, room `R4C2FBQ`, loopback `127.0.0.1:54518` (server PID 6120). This was a local ephemeral port, not the requested port 1357. Port 1357 was free when checked; no existing owner was terminated. The server stayed alive during the observed reconnect.

The server journal is `logs/2026-11-06/20261106_004753/server-01/events.jsonl` plus its rotated `events-0001.jsonl`; client journal is `logs/2026-11-06/20261106_004753/client-01/events.jsonl` plus `events-0001.jsonl`; dev-loop journal is under the same run's `dev-loop/events.jsonl`.

## Runtime evidence

- Client `network.snapshot.timeout` at `2026-11-06T00:52:22.096Z`: complete snapshot age 2501 ms, while last-heard age was 401 ms. This suggests some packet traffic continued while a complete authoritative snapshot did not arrive; it does not by itself prove whether the server failed to send, the network dropped chunks, or client reassembly failed.
- Client reconnect diagnostics then showed attempts #3–#7 with `packet_received_after_disconnect=false` and `heard_since_disconnect=false`. The client's resource probe reported 19 remote NPCs, 3 snapshot chunk buffers / 3210 bytes, no pending weapon requests, and 689228 bytes / 577 chunks queued for avatar upload. These are correlated state, not yet proven causes.
- Server cadence in minute buckets fluctuated below 60 Hz (about 48.88–58.33 Hz across the sampled minutes) and sometimes recovered. This short run does not establish a monotonic decline or a steadily growing NPC/projectile population (NPCs remained 19 and projectile counts fluctuated at low values).
- At `2026-11-06T00:56:58.842Z`, server tick 29437 took 891.839 ms; 879.766 ms was in NPC work, with 19 NPCs and zero connected players. Matching `npc.tick-cost` and `npc.slow-update` records attribute about 879 ms to NPC AI / sensing for actor 100007; `senseWorld` consumed about 874 ms. Owner wrapper is `src/npc/npc.cpp:1481-1484`; the expensive sensing internals include LOS grid traversal around `src/npc/npc.cpp:279-315` and `rayTraverseGridCells` in `src/physics/physics-collision.cpp:460-674`. No inner-operation timer proves which sensing sub-operation consumed that time, and wall-clock timing can include scheduling/preemption. Treat the LOS traversal as a target for profiling, not a proven root cause.
- The 891 ms stall occurred several minutes after the first snapshot timeout and while the server had zero players. It proves a severe server tick-stall path exists, but does not prove it caused the initial disconnect.
- Reconnect-grace removal event was not found in the inspected journal scan; do not infer a removal reason from its absence. The client process stopped logging, then the user-requested restart was issued through Mode 2. The dev-loop status after restart showed client PID 28144 and the original server PID 6120 alive; no competing server was started.
- On the relaunched client, `performance.stall-watchdog` captured a 1.52-second main-thread stall in `Networking` at `src/engine/engine-tick.cpp:206`. Captured stack includes `std::ifstream` construction and `CreateFileA`/`GetDriveTypeW`. This is evidence of synchronous file opening on the networking frame path, but symbolized frames are incomplete; exact owning function and the opened path are not proven yet. This is a strong next trace target.
- No rocket launcher cooldown/ammo rejection scenario was exercised. No weapon fix is claimed.

## Findings and next fixes

There are at least two independently observed performance risks: unusually expensive NPC sensing, and synchronous file I/O during a client Networking frame. Either could contribute to stalls under some conditions, but available evidence does not yet establish the root of the original 00:52 reconnect. The client's missing complete snapshots while recently hearing other packets makes snapshot send/chunk/reassembly tracing the immediate network next step.

Recommended owner-level follow-up: add bounded per-stage timing for `senseWorld` (especially ray traversal and hazard/visibility queries), identify the exact `ifstream` owner/path using symbols or a bounded scope event, and correlate server snapshot chunk send IDs/counts with client receive/reassembly/timeout IDs. Include authoritative server tick in NPC slow-update records: current `npc.slow-update.tick` is a sensor-local tick, not the server tick, which complicates correlation. Keep weapon cooldown investigation separate and capture a real rejected/accepted fire request with ammo reconciliation before changing weapon behavior.

## Validation and limits

Build evidence: Mode 2 incremental build succeeded as described above. Runtime evidence: live mode-45 server/client used the exact published build and produced the journals described here. Human visual/gameplay acceptance was not performed. No cold/manual build or synthetic test was run. Existing unrelated lane edits were preserved; no commit was created for this session.

### Additional client-stall owner trace

The relaunched client later produced `performance.stall-watchdog`: 1.52 seconds in `Networking`, progress location `src/engine/engine-tick.cpp:206-207`. Its captured stack contains `std::ifstream` construction and Windows `CreateFileA` / `GetDriveTypeW`. Source tracing identifies a directly matching synchronous file-read path: `mpAvatarNetworkTick` calls `buildLocalAvatarPublication` from `src/network/avatar-network-client.cpp:324-325`; that walks local PNGs at `:140-148` and calls `readFile` at `:150-152`; `readFile` checks file size then constructs `std::ifstream` and reads the entire image at `:62-73`. These operations run inline in the networking tick. The reconnect probe from the first client also showed 689228 queued upload bytes / 577 chunks. Together this is strong evidence that avatar publication is contributing main-thread networking stalls, though the stack is not fully symbolized and no single captured frame names the path argument. It does not by itself explain the server-side snapshot timeout.

A focused fix should make local avatar publication incremental/asynchronous: enumerate and read bounded assets outside the frame-critical network tick, then enqueue completed packets; do not block `engineTickNet` while opening/reading hundreds of image files. Preserve the existing packet/manifest protocol and validate that joining still publishes the same manifest/assets. Separately instrument chunk send/reassembly by shared transfer ID to find the snapshot failure's first missing side.
