# Public server announcement notifications

- Branch: `8292026stash`
- Commit: not committed; implementation remains in the working tree.
- Timestamp: `2026-09-08T16:20:00Z`
- Pre-existing changes: the worktree already contained unrelated edits across configuration, documentation, camera/physics/gameplay, networking, and terminal files. Those edits were preserved and not claimed as part of this task. Existing untracked changelog files in this directory were also preserved.

## Final state

Implemented client-side public-server announcements using the existing notification popup system and coordinator server-browser data. The runtime now polls the server browser during the engine tick, detects newly appearing room codes, queues each new room once while it is present, removes absent rooms from the deduplication set so a later reappearance can be announced again, and exposes announcements to the notification layer.

The popup includes host username, server name, measured ping or `unknown`, mode, map, and player count. It has a `JOIN` action and is also activated by `K`. `K` is consumed only when a join action exists, preserving the existing K behavior otherwise. The action uses the existing pending room-connect path. Password-protected rooms open the existing password popup; the password submission preserves the server name and protected state. Connection-state success now says `Connected to <server>!!!!` when the server name is known, and failed ICE startup produces a critical failure notification.

The notification command owner now also registers `notifmute` as an alias of `notifs` and `notiftempmute` as an alias of `notifstempmute`. The existing fixed 60 Hz temporary-mute conversion remains unchanged.

## Exact files and changes

- `src/network/server-browser.h:43-76`: added `ServerAnnouncement` and a thread-safe-consumer API declaration. Old behavior exposed only browser rows; new behavior also exposes newly discovered rooms.
- `src/network/server-browser.cpp:37,115-151,199-204`: added known-room tracking, reappearance handling, announcement queueing, discovery diagnostics, and queue consumption. Existing coordinator list retrieval and UDP ping probing were preserved.
- `src/engine/engine-tick.cpp:45,95-124`: runs browser polling during normal engine ticks and pushes formatted actionable `PUBLIC SERVER` notifications. The callback creates the existing pending connection request and pushes `Attempting to join ...`.
- `src/notifications/notifications.h:80` and `src/notifications/notifications.cpp:444-453`: added activation of the newest callback action labeled `JOIN`.
- `src/input/input-commands.cpp:95`: reserves K only when a current JOIN action exists; otherwise input continues through the existing path.
- `src/gui/gui-main.h:82`: extended `MultiplayerConnectInfo` with server name and password-protected state.
- `src/engine/engine-tick-state.cpp:292-321`: carries server metadata into the multiplayer context, opens the existing password popup when required, and reports ICE startup failure.
- `src/gui/password-popup.h:20`, `src/gui/password-popup.cpp:58`, and `src/gui/gui-main.cpp:1193-1197`: preserve the server name and protected state when submitting a password.
- `src/network/multiplayer-context.h:468` and `src/network/multiplayer-packets.cpp:1296`: retain the target server name and report connection success with the requested server-specific message.
- `src/notifications/notification-commands.cpp:22-49,62-94`: added the singular command aliases without changing existing command behavior.

## Validation

- Routed documentation read: `docs/ROUTER.md`, GUI/networking/debug-logging specifications, build instructions, and focused behavior, terminal-command, and logging skills.
- `git diff --check` on the changed task files: no whitespace errors. Git also reported pre-existing line-ending warnings in the dirty worktree.
- `python build_agent.py`: `Status: SUCCESS`, canonical `C:\mimita-priv-v8\mimita.exe` linked successfully; 6 objects compiled and 465 skipped on the final run.
- Focused runtime tests were not performed. Coordinator discovery, two-client joining, password acceptance/rejection, K activation, and the final connected popup still require live human acceptance.

## Remaining human review

Verify with two running clients and a real public coordinator: observe discovery during gameplay, confirm duplicate suppression and reappearance behavior, press K on public and password-protected rooms, verify the attempting/connected/failure notifications, and confirm existing replay-editor K behavior remains unchanged when no JOIN notification is present.
