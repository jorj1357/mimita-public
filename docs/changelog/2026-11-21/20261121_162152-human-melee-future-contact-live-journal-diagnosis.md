# Human melee future-contact live journal diagnosis

## Findings

- Inspected run `20261121_144350` and its rotated `client-01/events-0010.jsonl`. At line 53372, `input.state` records a focused client with `ai_active=false`. At line 55468, client PID 11492 queues a Mace contact against NPC 200005 with `action_active=true` and `contact_tick=43683`. At line 55482, server PID 29352 rejects that same contact ID as `future_contact` at server tick 8998, 34,685 ticks behind.
- The run contains 264 client melee claims and 254 `future_contact` rejections. A later client snapshot diagnostic at line 85234 reports cached `server_tick=43683` while the incoming snapshot is `snapshot_tick=16053`; subsequent snapshots advance to 16337 while the cached tick remains 43683. This is the first proven failure in the damage path: server claim-time validation rejects the contact before authoritative damage application.
- Source matches the journal: `src/network/server-packet-handlers.cpp:1085-1097` rejects contacts more than 120 ticks ahead. `src/network/multiplayer-tick.cpp:1923,2087` only updates `latestServerTick` when an incoming snapshot is greater. `src/network/multiplayer-packets.cpp:386-419` tears down prior session state and resets interpolation/snapshot fields but does not reset `latestServerTick`. `src/combat/melee-client.cpp:493-503` clamps the claim to that cached value. The likely defect is that the client retained an old server tick after the server restarted into a lower tick epoch; incoming snapshots were ignored as regressions, so human Mace claims remained stamped with 43683 and were rejected. The correct fix is epoch-aware reset/re-anchor using existing session identity, while preserving server validation.
- The journal/run identity is inconsistent: the manifest says server PID 7556 and client PID 11492 is now offline, while the rejection events are from server PID 29352. The process registry currently reports no managed run; the remaining process is server-only. Treat the journal as strong evidence of the observed failure, but do not claim it is a currently active human session without reconciling the replacement server PID.

## Issue updates

- Triaged `issue-adfeaafa1136` as high priority and appended the exact journal, source, process-identity, and likely tick-epoch findings. It now requests a current pinned human Fun Counter Strike Mace proof after the fix.
- Expanded and triaged `issue-9b5be6c88de0` as high priority: autonomous investigation must inspect an existing run read-only or launch an isolated pinned executable through the root dev-loop, aggregate rotated journals, correlate client claims through server rejection/damage, and validate PID/start time plus EXE and DLL identities. The user quote is preserved on the issue.

## Validation limits

- No gameplay source was edited. No new isolated run was launched because the published EXE is stale (source generation 0), the latest candidate game-DLL generation 425 failed linking, and the registered run has no connected client. The root dev-loop status and exact per-run journals were inspected. The existing runtime journal is not a fresh proof against current source; no visual or human acceptance is claimed.
- The user's separate version-specific regression tracking request already has `issue-37a27bfdc575`; this session did not create a duplicate.
