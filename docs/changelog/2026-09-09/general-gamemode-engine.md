# General gamemode engine, actor reset, and NPC combat attribution

Branch: `8292026stash`
Commit: working tree; no commit created
Timestamp: 2026-09-09T14:55:00Z

## Pre-existing working-tree changes

The following edits were present before this task and were preserved without
claiming them as part of this work: `config/accounts/default.json`,
`config/analytics.json`, and `config/gui/help-menu.json`.
`docs/specs/networking/networking.md` also contained an unrelated working-tree
edit and was preserved. `config/gui/tab-leaderboard.json` existed as an
untracked placeholder before this task; it was intentionally replaced as part
of the requested Tab leaderboard implementation.

## Implemented changes

- Added `CommunityMatchClient::reset()` in
  `src/network/community-match-client.h/.cpp`. It resets replicated mode,
  phase, countdown/ticks, timers, match identity/version, scores, results,
  Bomb Tag state, and clears `MatchLeaderboard` and `KillfeedManager`.
- Updated `src/network/multiplayer-packets.cpp` so old-session teardown,
  inactive shutdown, and new multiplayer initialization all use the same reset
  boundary. This prevents a stopped server's countdown and HUD state from
  surviving into a new server session and allows a restarted match ID to be
  accepted after a new connection.
- Added `MatchLeaderboard::clear()` in
  `src/gui/hud/match-leaderboard.h/.cpp`.
- Generalized the managed spawn operation in
  `src/network/server-gamemode.cpp` to
  `resetGamemodeActorsAtMapSpawn`. Map reload, rotation, countdown, and active
  reset paths now update player and simulated NPC positions, respawn positions,
  velocity, impulses, health/dead state, epochs, and authoritative mirrors.
- Applied the active community weapon-set filter during actor reset and player
  spawn synchronization in `server-gamemode.cpp` and `server-players.cpp`.
- Added actor-neutral NPC attribution to the compact damage-confirmation packet
  in `src/network/packets.h`, `server-damage.cpp`, and `server-npcs.cpp`.
  NPC-to-player kills now carry the NPC ID and actor kind instead of attacker
  ID zero; the client resolves the registered NPC name in
  `confirmed-damage-presentation.cpp`.
- Added participant-name storage to the authoritative gamemode state so FFA
  top-three entries use registered player/NPC names, with `NPC-<id>` fallback.
- Standardized newly spawned NPC names to `NPC-<id>` in `src/npc/npc-spawn.cpp`.
- Updated killfeed weapon lookup in `src/killfeed/killfeed.cpp` to accept a
  weapon ID or display name and resolve configured presentation through the
  weapon registry. `config/killfeed.json` documents
  `config/weapons.json` as the display-name source.
- Replaced the Tab leaderboard placeholder with valid JSON and updated
  `src/engine/engine-tick-ui-overlays.cpp` to load its panel, title, header,
  row, local-row, and NPC-row presentation through the hot-reloadable layout.
- Documented engine-first general ownership, good-map-pool behavior, session
  reset, and actor-neutral lifecycle rules in
  `docs/specs/gamemodes/gamemodes.md` and
  `docs/architecture/code-ownership/code-ownership.md`.
- Added the confirmed regression record
  `docs/regressions/2026-09-09/gamemode-session-actors-REG.md` with exact wrong
  and corrected code, cause, attempted-fix status, proof, and live-review
  requirements.

## Exact source contract changes

Old NPC damage attribution passed `0` as the attacker player ID:

```cpp
queueServerDamageConfirmedEvent(
    sock, players, tick, totalPacketsOut, 0, *nearest, damage, result,
    realHit, realNormal, knockback, ServerDamageSource::Hitscan, hitWeapon);
```

New attribution passes the NPC ID through the existing compact ID field and
marks it as an NPC:

```cpp
queueServerDamageConfirmedEvent(
    sock, players, tick, totalPacketsOut, 0, *nearest, damage, result,
    realHit, realNormal, knockback,
    ServerDamageSource::Hitscan, hitWeapon, 0, 0, n.id);
```

```cpp
event.attackerPlayerId = attackerNpcId != 0 ? attackerNpcId : attackerPlayerId;
event.attackerEntityType = attackerNpcId != 0 ? ENTITY_NPC : ENTITY_PLAYER;
```

The packet remains within its existing `sizeof <= 104` wire contract.

## Documents and skills used

Read and applied: `AGENTS.md`, `docs/ROUTER.md`,
`docs/regressions/regressions-v1.md`, the gamemode, GUI, networking, JSON
configuration, code-ownership, and debug-logging specifications, plus
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/gui-hardcoding-checker-v1.md`, and
`docs/skills/chat-checker-v1.md`.

## Validation

- JSON parsing passed for `config/gamemode-good-maps.json`,
  `config/gui/gamemode-meta-gui.json`, `config/gui/tab-leaderboard.json`,
  `config/gui/help-menu.json`, `config/killfeed.json`, and
  `config/weapons.json`.
- `python build_agent.py` completed and linked the canonical executable with
  `Status: SUCCESS` and return code `0` at 2026-09-09 09:49:50 local build
  time.
- The packet-size compile regression was corrected before the successful
  rebuild.
- `git diff --check` was run. It reported a pre-existing trailing-whitespace
  warning in `docs/specs/networking/networking.md`; no new source error was
  introduced by the final patch.
- No active `modepick` registration was found under `src` or `config/gui`.

## Remaining human review

Live two-client/server acceptance is still required for: stopping server A and
joining server B; repeated countdown cycles; NPC/player scoring in both
directions; NPC killfeed names and weapon display names; NPC deletion and
leaderboard removal; selected restricted weapon sets; map-transition void
respawns; Tab hot reload; and live `modestart`/`modestartnow` results and
intermission behavior. The regression record therefore remains
`ATTEMPTED FIX (1)`, not a human-confirmed solution.
