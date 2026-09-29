# healthme self-only respawn override

- EST timestamp: 2026-09-28 19:45
- Branch: `afad20a-rebuild`
- Result: PASS_WITH_HUMAN_REVIEW

## Scope and source change

The reported behavior was that `healthme N` either did not affect the real
player or was lost on death. The old command only changed the local
`Player::currentHp`/`maxHp` and a process-global client developer flag. The
authoritative server's `resetPlayerForSpawn()` then selected the normal role
health on every spawn.

The implementation now:

- adds `ServerPlayer::healthOverrideEnabled` and
  `ServerPlayer::healthOverrideValue` as a per-connected-player server state;
- accepts `healthme N` and `healthme default|reset` before the host-only command
  gate, so any player can change only their own health;
- applies the self-only value immediately on the server and again in every
  authoritative initial spawn/respawn;
- routes networked client commands through `mpSendServerCommand()` while
  retaining immediate local feedback;
- leaves `healthall` as the separate explicit all-entity override, with its
  existing precedence.

Files and owners:

- `src/terminal/debug-commands.cpp:296-359` — command validation and client
  dispatch.
- `src/network/server-packet-chat.cpp:438-470` — authenticated self-only
  server command handling and acknowledgement.
- `src/network/server.h:269-270` — per-player authoritative override state.
- `src/network/server-players.cpp:352-363` — shared spawn/respawn application.

Exact behavior change: the old spawn expression was `healthall override or
role/default health`; it is now `healthall override, otherwise this player's
healthme override, otherwise role/default health`. The existing unrelated
working-tree edits in `server.h` and `server-packet-chat.cpp` (host identity
diagnostics) were preserved.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/ingame-chat/ingame-chat.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

Focused review result: PASS for ownership, argument validation, authoritative
dispatch, self-only scope, and respawn application. No specification decision
was required for the requested behavior.

## Validation

- Source search confirmed the command registration, server packet path, per-
  player state, and shared respawn owner are connected.
- `python build_agent.py` completed with return code 0 and `Status: SUCCESS` in
  `build/changelog.txt` at 2026-09-28 19:40:01. The affected server command,
  server player, terminal command, and dependent header objects were rebuilt.
- No live MiMITA process was running during the build.

## Still required

Human/live acceptance remains open: run `healthme 500`, confirm the local HUD
and authoritative health become `500/500`, die and respawn, confirm `500/500`
again, and confirm a second connected player remains at their normal health.
Then run `healthme default` and confirm the next respawn returns to normal.

