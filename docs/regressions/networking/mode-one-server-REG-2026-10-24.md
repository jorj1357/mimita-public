# Dev-loop mode 1 reached an older server on a shared UDP port

Time created: 2026-10-24T14:29:58Z
Status: FIXED IN LOCAL RUNTIME (one fresh run; repeated human play remains open)
Related specification: `docs/specs/dev-loop/dev-loop.md`, sections 7 and 9.
Related changelog: `docs/changelog/2026-10-24/20261024_142958-mode-one-launch.md`.

## Observed regression

Mode 1 launched a new server and registered a room, but the client displayed
“Server identity mismatch.” In `logs/2026-10-24/20261024_140417/events.jsonl`,
`network.server-identity-mismatch` recorded expected run
`20261024_140417` and actual run `20261024_140320`, with the same build ID.
Two dev-loop servers, PIDs 24876 and 27716, had both requested
`127.0.0.1:1357`; the client received the old server's welcome packet.

## Expected behavior

An explicit mode 1 launch selects one matching server/client build and joins
the newly registered room through coordinator/ICE. The welcome packet must
carry the selected run identity. The dev-loop specification says, “Stale
dev-loop servers cannot remain the authoritative server for a newer client.”

## Wrong and corrected source

`devscripts/dev-profiles/npc-navigation.json` previously contained:

```json
"server_bind": "127.0.0.1:1357"
```

It now contains:

```json
"server_bind": "127.0.0.1:0"
```

Windows assigns each local dev server a distinct UDP port. The server already
registers its actual bound port with the coordinator; the client still joins
using the room code and ICE path.

The client ICE worker also previously treated every failed preflight lookup as
“Room not found” with `if (!lookup.exists)`. It now uses
`if (lookup.reachable && !lookup.exists)`, allowing a temporarily unreachable
coordinator to proceed through the normal retry path and reporting
“Coordinator unreachable” if retries ultimately fail. The live journal now
records the lookup result and a completed join failure once each.

## Proof and limits

Build 3015 succeeded through the dev loop. In
`logs/2026-10-24/20261024_142856/events.jsonl`, server PID 30880 registered
room `SKB6ME4`; client PID 33104 logged `network.ice-room-lookup` with
`reachable=true, exists=true`, received a `join_accept`, loaded map `xxity2`,
and transitioned to `Connected` on `ice:SKB6ME4`. There were zero identity
mismatch and zero ICE join failure events in that run. The bound local UDP
port was 65430, while the old fixed-port servers were preserved.

This proves one fresh mode 1 run. It does not establish repeated join success
under coordinator outages or human gameplay acceptance.
