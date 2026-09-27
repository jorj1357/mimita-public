# Automatic dev client was not recognized as the server host

Time created: 2026-09-27T01:58:28Z
Time last updated: 2026-09-27T02:05:22Z

Status: SOLUTION AS OF 2026-09-27T02:05:22Z

Related specification:
`docs/specs/networking/networking.md`

Related changelog:
`docs/changelog/2026-09-27/20260927_015828-dev-loop-host-client.md`

## Observed

The automatically launched room-code client could join the development
server, but host-only terminal commands such as `healthall 999` were rejected.

## Expected behavior

The client automatically launched by the development loop should be the host
client for that newly started development server. Existing host-only commands
must remain unchanged and should be accepted through the normal server-command
path.

## Confirmed cause

`devscripts/dev-loop.py` passed `--host-player "NPC Dev"` to the server, while
the actual client identity is taken from `AuthSystem::instance().displayName()`
inside the game. The server's `computeHostFlag` compares the joining player's
name with `gServerHostPlayerName`, so the names did not match.

## Wrong code

```python
"--host-player", str(self.profile.get("client_name", "Dev")),
```

## Attempted fix 1

The automatic profile now leaves `--host-player` unset. The existing server
rule already treats the first joiner as host when the configured host name is
empty. Since the dev loop starts the server and immediately starts its own
room-code client, that first joiner is the user-controlled client.

An explicit `host_player_name` profile value remains available when a test
needs a fixed identity.

## Corrected code

```python
configured_host = str(self.profile.get("host_player_name", "")).strip()
if configured_host:
    server_args.extend(["--host-player", configured_host])
```

The default profile contains:

```json
"host_player_name": ""
```

## Scope boundary

`healthall` was not changed. This fix only corrects automatic host identity
selection for the dev-loop startup path.

## Proof status

- Python syntax validation is required after this edit.
- Human review must launch the profile, run `healthall 999` in the client
  terminal, and observe that the existing command is accepted.
- Do not mark this regression solved until that command is observed working.

## Attempted fix 2

Time:
`2026-09-27T02:05:00Z`

The running daemon was still an older Python process. Its build-13 command
line still contained `--host-player "NPC Dev"`, while the room-code client did
not put its explicit `--name "NPC Dev"` into the real multiplayer identity;
the gameplay path continued to use the AuthSystem display name.

The connection info now carries the explicit launch name through the room-code
join path. The client therefore sends the same identity the server was told to
use, while ordinary GUI joins continue using the authenticated profile name.

The existing empty-host-name first-joiner fallback remains in place for the
fresh default profile.

Human confirmation completed: after build 14 launched the automatic room-code
server and client, `healthall 999` was run in the client terminal and worked;
the entities received the requested health change.

## Final solution

The first attempted correction—letting an empty `--host-player` value make the
first joiner host—was not enough for the already-running old daemon. That
daemon still launched `--host-player "NPC Dev"`, while the gameplay client
discarded its explicit `--name "NPC Dev"` and sent the authenticated profile
name instead.

The confirmed solution preserves the explicit launch name through
`MultiplayerConnectInfo.playerName` and uses it when the room-code client sets
its gameplay identity. The server and automatic client now agree on the host
identity. Normal GUI joins still use the authenticated profile name, and the
`healthall` implementation itself was not changed.

### Solution proof

- Build 14 compiled and launched from the current `C:\mimita-v9` dev loop.
- The server started through the room-file and room-code/ICE path.
- The automatic client joined with the matching host identity.
- Human test: `healthall 999` succeeded and changed health for all entities.
