# Unified authoritative kill event (FFA scoring + killfeed + GO)

- Task ID: unified-kill-event
- Summary: Replace duplicated NPC-kill handling and per-viewer killfeed
  reconstruction with one authoritative kill owner and one reliable
  `KillEventPacket`; make `GO!!!` hold for its full window.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T15:52:52Z` (2026-09-10 11:52:52 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- `git status --porcelain` also lists these not created or modified by this
  session: `config/analytics.json`, `config/ragdoll.json`,
  `src/engine/engine-tick-camera.cpp`, `src/ragdoll/ragdoll-mode-config.cpp`,
  `src/ragdoll/ragdoll-mode-config.h`, `src/ragdoll/ragdoll-mode.cpp`,
  `src/ragdoll/ragdoll-mode.h`, and untracked `src/physics/physical-body.cpp`
  and `src/physics/physical-body.h`. They are not claimed here.
- The countdown broadcast/client-gate work from the earlier session on this
  branch is uncommitted prior work built on by this session.

## Requested behavior

- Player killing an NPC and NPC killing a player must each award one FFA/TDM
  point to the killer and update the on-screen score/leaderboard.
- Both kill directions must produce exactly one killfeed/chat line visible to
  the killer, the victim, and every observer.
- The countdown must show `3`, `2`, `1`, `GO!!!` (GO now 1.0s).

## Specification alignment

- `docs/specs/gamemodes/gamemodes.md`: `COUNTDOWN 3 → COUNTDOWN 2 → COUNTDOWN 1
  → GO → ACTIVE`; `docs/features/gamemodes/ffa mode issues.md` acceptance:
  five NPC→player kills give five NPC kills, five player→NPC kills give five
  player kills, every kill makes one reliable killfeed event with correct names.
- `docs/specs/networking/networking.md`: one authoritative gameplay event per
  outcome; important gameplay events are delivered reliably.
- The change follows the specification by giving kills one owner and removing
  parallel code paths.

## Exact implementation changes

- `src/network/packets.h`: `PROTOCOL_VERSION` 32→33; added `PACKET_KILL_EVENT = 68`
  and `KillEventPacket` (eventId/eventSessionId first for the reliable envelope,
  killer/victim id+kind, serverTick, correlationId, resolved names/weapon).
- `src/network/server-gamemode.h`: `ServerGamemodeKillEvent` gained
  `killerName`, `victimName`, `killerPos`, `victimPos`; added `currentKill`;
  replaced the three legacy enqueue declarations with `serverGamemodeRecordKill`.
- `src/network/server-gamemode.cpp`: added `serverGamemodeRecordKill` which
  credits/heals a player killer, broadcasts one `KillEventPacket` reliably, and
  queues one kill event when the gamemode is enabled; promotion stores
  `currentKill`; the consume block now emits `emitPvPKillPersistenceEvent` /
  `emitNpcKillPersistenceEvent` once from the unified event.
- Lethal sites now call `serverGamemodeRecordKill` and no longer credit/heal/
  persist themselves: `src/network/server-attack.cpp`,
  `src/network/server-projectiles.cpp`, `src/network/server-packet-handlers.cpp`
  (godball + spy knife), `src/network/server-damage.cpp`,
  `src/network/server-packet-chat.cpp`.
- `src/network/multiplayer-tick.cpp`: dispatch `PACKET_KILL_EVENT` and present
  via `KillfeedManager::onKill` with `(eventSessionId<<32)|eventId` dedup.
- Removed client killfeed reconstruction: `src/network/multiplayer-shots.cpp`,
  `src/network/multiplayer-projectiles.cpp` (victim + observer branches),
  `src/network/confirmed-damage-presentation.cpp`.
- `src/combat/death-system.cpp`: local `KillfeedManager::onKill` only runs when
  not in an active networked session, so the authoritative event is the only
  live source (replay capture unchanged).
- Countdown/GO: `config/gamemodes/ffa.json` and `tdm.json` `go_seconds` 1.0;
  defaults to 1.0 in `src/gamemode/gamemode.h` and `src/network/server-gamemode.h`;
  `CommunityMatchClient` stores `mGoVisibleUntilTick` from the GO packet's
  `phaseTimer` and exposes `goVisible()`; the HUD
  (`src/engine/engine-tick-ui-overlays.cpp`) renders `GO!!!` while `goVisible()`
  is true; the `[CountdownSync]` diagnostic moved from `Duel` (off) to
  `Networking` (enabled).

## Diagnostics

- `[KILL EVENT]` (Networking) at the server kill owner: killer/victim names and
  kinds, weapon, tick, event id.
- `[KILL EVENT RX]` (Networking) at the client presenter: same fields.
- `[CountdownSync]` (Networking) now records accepted phase, authoritative and
  synced tick, match-start tick, rendered number, and GO visibility.

## Validation

- Skill `docs/skills/spec-behavior-review-v1.md`: PASS — implementation matches
  the specified lifecycle and single-source rule.
- Skill `docs/skills/logging-checker-v1.md`: PASS — diagnostics sit at the kill
  owner and the replication consumer, include decision inputs, and are
  low-volume.
- Build: `python build_agent.py` -> `BUILD SUCCESS`, return code 0
  (`build/changelog.txt`). Final pass compiled 8 files; the protocol bump pass
  compiled 116.
- Runtime: not performed this session.

## Regression review

- Regression entry appended: yes,
  `docs/regressions/regressions-v1.md` (2026-09-10T15:52:52Z).
- Cause recorded: duplicated kill handling across many files.
- AI-model note recorded there: believed origin is model `mimo v2.5`; the model
  hypothesis is not strong enough to assert, and the falsifiable test is to
  solve the same issue with that model.

## Human acceptance

- Gameplay review required: FFA and TDM, kill an NPC and be killed by an NPC;
  confirm the killer's score increments in both directions and the leaderboard
  updates.
- Visual review required: exactly one chat killfeed line per kill for killer,
  victim, and observers.
- Visual review required: `3 → 2 → 1 → GO!!!` each visible, GO for ~1.0s.
- Still unverified: all runtime behavior; only compilation was proven.

## Related feature record

- `docs/features/gamemodes/ffa mode issues.md`
