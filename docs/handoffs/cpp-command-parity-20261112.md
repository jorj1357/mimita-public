# PACKET_SERVER_COMMAND parity and authority (2026-11-12)

**Status:** runtime parity and the first authoritative command slice are proved
in a real server/client run. Exact `admincmds` now runs in the game generation;
the resident handler remains authoritative for argument forms and all other
commands. No cutover and no state-continuity proof.

## Logging note

Probe events are written at `EXECUTABLE`/`IMPORTANT`, but
`config/debuglogger.json` sets `executable` to `errors` by default, so those
events are suppressed. The probe run temporarily set it to `important` and then
reverted it to `errors`. The `probe.transport.response-sent` event is `NETWORK`
and remains visible.

## Successful parity run

Run: `C:\mimita-v9\logs\2026-11-12\20261112_215614`

- Server and client loaded `game-g16.dll`.
- `game.host.probe.started` was written by both processes.
- Terminal command `admincmds` was delivered by the dev loop.

Server journal:

```json
{
  "event": "game.module.log",
  "message": "game.transport.inbound type=19 bytes=260 player=1 command=admincmds accepted=1 ack=1 status=admincmds=1 (host)"
}
```

Resident result from the earlier non-authoritative run `20261112_214742`:

```json
{
  "event": "network.server-command.result-sent",
  "accepted": true,
  "command": "admincmds",
  "status": "admincmds=1 (host)",
  "player_id": 1
}
```

The game-side and resident decisions matched: accepted, `admincmds=1 (host)`.

## Response envelope

`TransportDispatchV1` carries the inbound message plus a bounded
`TransportResponseV1`. The probe writes `ServerCommandResultPacket` into the
response payload. The resident server sends it to the exact inbound endpoint,
using the player transport when present and `sendto` otherwise.

Run: `C:\mimita-v9\logs\2026-11-12\20261112_223642`

```json
{"event":"probe.transport.response-sent","bytes":264,"type":54,"sent":true,
 "transport":false,"player_id":1,"magic":1296649528,"version":52,
 "header_type":54,"header_tick":2065,"header_player":1}
```

Client journal:

```json
{"event":"network.server-command.result-packet","bytes":264,
 "header_player":1,"header_tick":2065,"transport":"active"}

{"event":"network.server-command.result","accepted":true,
 "local_player_id":1,"status":"admincmds=1 (host)"}
```

The game-owned response is received and processed by the client.

## Authority slice

Exact `admincmds` is now authoritative:

- `admincmds 0` -> resident path; client result `applied: admincmds 0`.
- exact `admincmds` -> game path; client result `admincmds=0 (host)`.
- `admincmds 1` -> resident path; client result `applied: admincmds 1`.
- exact `admincmds` -> game path; client result `admincmds=1 (host)`.

For the game-handled exact command there is **no**
`network.server-command.result-sent` from the resident handler. The argument
forms remain resident. This is the first hot command round trip.

## Evidence separation

- Source: `TransportDispatchV1`, `HostRuntime::dispatchInbound`, server probe
  dispatch, and the probe authority slice compile.
- Build: probe generation 20 links; EXE built by the dev loop.
- Runtime: parity and exact `admincmds` authority proved in one server/client
  run; client receives the game-owned result.
- Cutover: not started.
- Next: extend the same response path to `player_list` (needs a bounded player
  list query), then map/team/spawn/gamemode slices, then live activation.
