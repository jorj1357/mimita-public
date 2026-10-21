# Invisible avatar report and pipeline investigation

- Timestamp: 2026-10-21T09:07:48Z (America/New_York display date 2026-10-21)
- Branch: `20261011zt`
- HEAD: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and pre-existing work

Added the recurring invisible-avatar report to the open gameplay issue inbox and traced available source and live journal evidence. The worktree was already dirty with many unrelated edits and the inbox was already an untracked working file. Preserved all existing work; no commit or code changes were made.

## Issue inbox edit

`docs/specs/open-gameplay-issues.md:90`
- Before: no Invisible avatars row.
- After: added an entry recording the report, source/journal findings, uncertainty about the exact visible actor, and the information needed on the next reproduction.

Exact added summary: “More importantly, remote avatar cache folders use `remote_` plus 24 hash characters (31 characters total), but snapshot avatar fields are 24 bytes including the terminator (23 usable characters). The server copies names into that field with `strncpy(..., sizeof(field)-1)`, so a 31-character remote ID is truncated on the wire.”

## Source trace

- `src/network/avatar-network-client.cpp:266` creates remote cache names as `"remote_" + manifest.manifestHash.substr(0, 24)`, a 31-character name.
- `src/network/packets.h:69,526,603` sets `MAX_AVATAR_NAME_BYTES` to 24 and uses that size for replicated avatar-name fields, leaving 23 bytes for text plus the terminator.
- `src/network/server-players.cpp:1805` copies replicated player avatar names with `strncpy(..., sizeof(out.avatarName) - 1)`, truncating longer names.
- `src/avatar/avatar-atlas.cpp:718-720` reports every cache state other than `Ready`, including `Failed`, as still pending.
- `src/render/render-player.cpp:71-73` only reapplies a mismatched/unbound avatar when `isAvatarLoadPending()` is false. A permanently failed identity therefore blocks that retry.
- `src/network/avatar-network-client.cpp:286` applies the remote cache alias, then stores `manifest.avatarName` on the player. Renderer identity/cache-name mismatch is another point to inspect during a reproduction.

This is a strong source-level mechanism for a remote avatar to fail resolution and remain on fallback presentation. It is not proof that this was the particular avatar the user saw disappear.

## Live journal evidence

Inspected the already-running Duel session journal `logs/2026-10-21/20261021_084608/events.jsonl` (client PID 35288, server PID 13732; older build 2733). Bounded aggregation found:

- 62 `avatar.load.queue-rejected` events, all `worker_queue_full`, queue depth 3 against limit 2, all marked retryable.
- 119 `client.npc.avatar.bind` events: 102 initially waited for async avatar loading; 17 bound an avatar instance while the model was not yet loaded.
- 58 `client.npc.avatar.rebind` events; all recorded an avatar instance and `model_loaded=true`.
- One `avatar.load.failed` for `remote_568d80ddb4e934de`, with `avatar.json missing or invalid`. The ID is 23 characters and matches the truncated prefix shape of a 31-character remote cache alias.
- The current logger records NPC bind/rebind and model readiness, but does not record normal live actor render submission; the log cannot prove whether the actor was drawn or visible.

The log contains real load failures, but it does not identify an actor as the user's invisible avatar. NPC rebind evidence shows those sampled NPCs recovered; it does not rule out remote-player failure or another path.

## Validation and remaining work

- No build or tests run: this turn changed only documentation and inspected existing source/journal evidence.
- No client/server process was restarted or interrupted.
- Next, reproduce with the affected actor and capture the mode/map, actor kind, avatar ID, executable/run ID, manifest receipt, authoritative snapshot identity, cache alias/path, model completion, and render submission. Then correct the avatar identity/wire contract and failed-state retry behavior at their existing owners, followed by the real two-client runtime scenario.
- No regression file added: the source mismatch is concrete, but user-visible regression identity and prior working behavior are not yet established.
