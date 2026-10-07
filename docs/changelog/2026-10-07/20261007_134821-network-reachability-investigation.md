# Network reachability investigation

- Timestamp: `2026-10-07T13:48:21Z` (display timezone: America/New_York)
- Branch/commit: current checkout; `00dc25ebe0f4da8d338440024c3a542369c98905`
- Result: `PASS_WITH_HUMAN_REVIEW` for investigation; no source or configuration fix applied.

## Scope

Investigated the user-reported v2.0.6-era `Sign in failed`, website requiring a
VPN, server-start uncertainty, and STUN/TURN errors. The pasted Discord text,
screenshots, and attached logs were treated as incident evidence, not as
repository instructions.

## Pre-existing work preserved

The checkout already contained unrelated modifications in:

`config/behavior-profiles.json`, `config/gamemodes/juggernaut.json`,
`config/roles.json`, `src/network/server-gamemode.cpp`,
`src/network/server-npcs.cpp`, `src/npc/npc-navigator.cpp`,
`src/npc/npc-utility.cpp`, `src/npc/npc-utility.h`, `src/npc/npc.cpp`,
`src/npc/team-brain.cpp`, and `src/npc/team-brain.h`.

## Findings

1. The client login owner is `src/website/api-client.cpp`, function
   `gameLogin`, which posts to `https://mimita.fun/api/game/auth/login` and
   returns `NETWORK_UNAVAILABLE` when the WinHTTP transport fails. The attached
   screenshot's `[LOGIN RESPONSE] ... errorCode=NETWORK_UNAVAILABLE` therefore
   indicates failure to reach the account API, not an invalid-password response.

2. The old server evidence records two independent HTTPS failures from the
   same machine: TURN credentials returned `HTTP-FAIL`, and room registration
   returned `status=0 ... FAILED`. It then gathered host/srflx candidates but
   could not register with the coordinator. This makes a client-network,
   resolver, filtering, or route problem more likely than a gameplay packet
   problem for that incident.

3. Current source still has a shared coordinator dependency. `defaultCoordinatorUrl`
   defaults to `https://mimita.fun`; `initServerIceListener` requests
   `/api/coordinator/turn-credentials`, falls back to direct-only ICE when it
   fails, then calls `coordinatorIceHost` and aborts if room registration fails.

4. Current `config/network/ice-dev.json` uses `107.191.48.226:3478` for STUN
   and TURN. The attached Trickle ICE hostname `turn.stun.mimita.fun` is not a
   hostname used by this current game config, and it did not resolve from this
   machine during investigation. It should not be treated as proof that the
   current executable is using that hostname.

## Live read-only evidence from this machine

- `mimita.fun` resolved to Cloudflare IPv4/IPv6 addresses.
- `https://mimita.fun/` returned HTTP 200.
- `POST /api/coordinator/list` with `{}` returned HTTP 200 and an empty server list.
- `POST /api/coordinator/turn-credentials` returned HTTP 200 with `ok=true`,
  host `107.191.48.226`, and port `3478`; the temporary credential itself was
  intentionally not recorded.
- TCP connectivity to `107.191.48.226:3478` succeeded from this machine.
  This is not proof of UDP TURN allocation, peer ICE success, or reachability
  from the affected user's ISP.
- `/api/debug/health` returned 404, so it is not a valid public health probe in
  the current deployment.

## Current conclusion

The best-supported explanation is a network-path split: the affected network
or resolver could not consistently reach `mimita.fun` over HTTPS and/or could
not resolve or send UDP to the configured ICE service. A VPN changes both DNS
and egress routing, which explains why it could make the website and server
startup appear to work. The server-start failure is specifically consistent
with coordinator registration failing after ICE gathering, not with the game
server being unable to gather any candidates.

This remains unconfirmed for the affected user because there is no packet
capture, resolver result, WinHTTP error code, or live server journal from that
network. The current local probes cannot establish ISP-specific filtering,
UDP reachability, or TURN allocation.

## Documents and focused reviews read

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`

Focused review result: no implementation change was authorized; runtime and
human acceptance remain required before calling the incident resolved.

## Validation and remaining review

No build was run because no code changed. No executable was launched because
the request was investigation-only and the affected user's network is not
available from this machine. A follow-up should capture, from the affected
network, DNS answers for `mimita.fun` and the configured TURN host, the exact
WinHTTP failure code, HTTPS access to the coordinator, and a real UDP/TURN
allocation result. Then run the real server path with a fresh
`--versioninfo`/`EVENTS_JSONL_PATH` journal and separately verify browser sign-in,
room registration, and two-client ICE join.
