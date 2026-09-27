# Preserve the automatic client name through room-code joins

The automatic dev server could still reject host-only commands when an older
running dev-loop daemon launched the server with `--host-player "NPC Dev"`.
The client command line also had `--name "NPC Dev"`, but the actual gameplay
join path discarded that value and used the authenticated profile name.

`MultiplayerConnectInfo` now carries an optional process-launch player name.
The room-code client uses that name for its real join identity; normal GUI
joins remain authenticated-profile based. This makes the server and automatic
client identities agree without changing `healthall` or host-command logic.

Validation confirmed: build 14 rebuilt and launched through the room-code/ICE
path, and human testing confirmed that `healthall 999` changed health for all
entities. The host-identity regression is now solved.
