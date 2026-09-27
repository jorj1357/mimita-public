# Automatic dev client is now the first host joiner

Fixed the development-loop identity mismatch that caused the automatically
launched client to be treated as a non-host.

The old loop advertised `NPC Dev` as the server host, but the real client uses
the logged-in AuthSystem display name. The server therefore rejected host-only
commands from the automatic client.

The profile now leaves `host_player_name` empty by default, using the existing
server rule that makes the first room-code joiner the host. An explicit fixed
host name remains available for specialized profiles. No host command logic or
`healthall` behavior was changed.

Related regression:
`docs/regressions/2026-09-27/dev-loop-first-client-not-host-REG.md`
