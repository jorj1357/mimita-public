# NPC kill attribution, NPC-vs-NPC targeting, and time-driven GO

- Task ID: npc-kill-attribution-go
- Summary: Credit and log NPC kills in both directions (and NPC→NPC), make the
  killfeed show real NPC names/weapons, and guarantee `GO!!!` renders even when
  the client misses the short GO phase on the first round after joining.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T16:43:40Z` (2026-09-10 12:43:40 EDT)
- Branch: `8292026stash`
- Base commit: `32a9296`
- Final commit: none (uncommitted)

## Pre-existing changes

- `git status --porcelain` also lists changes not created by this session:
  `config/accounts/default.json`, `config/analytics.json`,
  `config/ragdoll.json`, `src/engine/engine-tick-camera.cpp`,
  `src/physics/physical-body.cpp`, `src/physics/physical-body.h`,
  `src/ragdoll/ragdoll-mode-config.cpp`, `src/ragdoll/ragdoll-mode-config.h`,
  `src/ragdoll/ragdoll-mode.cpp`, `src/ragdoll/ragdoll-mode.h`, and the
  untracked `docs/changelog/2026-09-10/20260910_163323-ragdoll-collision-solid.md`,
  `docs/changelog/2026-09-10/20260910_163852-ragdoll-stiffness-limits-camera.md`,
  and `docs/gold/2026-09-10-ragdoll-rigid-body-thought-process.md`.
- Base commit `32a9296` already contains the earlier killfeed/score and
  countdown work this session builds on.

## Requested behavior

- Killing and being killed by an NPC must award one FFA/TDM point to the killer
  and produce exactly one killfeed/chat line for killer, victim, and observers.
- NPC→player and NPC→NPC kills must show resolved names and weapon, e.g.
  `NPC-1000 killed NPC-1001 with Rocket Launcher`.
- `GO!!!` must be visible on the first countdown after joining, not only on the
  second round.

## Specification alignment

- `docs/specs/gamemodes/gamemodes.md`: lifecycle `COUNTDOWN → GO → ACTIVE`, and
  `docs/features/gamemodes/ffa mode issues.md` acceptance: five NPC→player kills
  give five NPC kills, five player→NPC kills give five player kills.
- `docs/specs/networking/networking.md`: one authoritative outcome per kill;
  important events delivered reliably.

## Exact implementation changes

- `src/network/server-projectiles.cpp`: resolve the projectile owner once
  (`ownerNpcId ? ownerNpcId : ownerPlayerId`, kind `ENTITY_NPC`/`ENTITY_PLAYER`);
  pass `attackerNpcId` into `queueServerDamageConfirmedEvent` for player splash;
  use the resolved owner (not `ownerPlayerId`) for NPC splash kills; record
  NPC-owned splash in `lastNpcDamageSourceId`/`lastNpcDamageTick`.
- `src/network/server-damage.cpp`: re-attribute a kill to the recent NPC damage
  source when the killing blow has no real player attacker (ownerless or
  self-inflicted), not only when a different player attacker exists.
- `src/network/server-npcs.cpp`: NPC target selection now scans players **and**
  other NPCs, skips self and same-team actors (unteamed modes are all-hostile);
  NPC targets are mirrored into the shared `mirrorPlayer`; NPC victim damage is
  written to `ServerNpc.health`/`knockbackImpulse` with the body synced, and a
  kill calls `serverGamemodeRecordKill(killerNpc, victimNpc, weapon)`.
- `src/network/packets.h`: `DuelStatePacket.goSeconds` added; `PROTOCOL_VERSION`
  33→34.
- `src/network/server-gamemode.cpp`: `broadcastDuelState` fills
  `pkt.goSeconds = d.goSeconds`.
- `src/network/community-match-client.h/.cpp`: client keeps `mSawGoThisMatch`;
  if the first ACTIVE that starts a match arrives without a GO packet seen, it
  sets `mGoVisibleUntilTick = serverTick + goSeconds*60` so `GO!!!` renders even
  when the GO phase was missed while loading.

## Diagnostics

- `SERVER_NPC_KILLS_NPC` (Network) at the new NPC→NPC damage branch: npc/victim
  ids and names, weapon id/display, damage, victim health, killed, tick.
- `SERVER_NPC_KILLS_PLAYER` and `KILL EVENT`/`GAMEMODE_ENQUEUE` now fire for
  rocket-attributed NPC→player kills.
- `[CountdownSync]` (Network) continues to log accepted phase/ticks; `goVisible`
  is now time-driven.

## Validation

- Skill `docs/skills/spec-behavior-review-v1.md`: PASS — one owner for kills and
  the specified lifecycle.
- Skill `docs/skills/logging-checker-v1.md`: PASS — new diagnostics sit at the
  kill owner and are low-volume.
- Build: `python build_agent.py` -> `BUILD SUCCESS`, return code 0
  (`build/changelog.txt`).
- Runtime: not performed this session.

## Regression review

- Regression entry appended: yes (`docs/regressions/regressions-v1.md`,
  2026-09-10T16:43:40Z).
- Cause: projectile explosion damage ignored the owning NPC; GO depended on a
  short phase packet the client can miss while loading.

## Human acceptance

- Gameplay review required: let the NPC rocket-kill the player and confirm the
  NPC scores and one chat line appears with the right names/weapon; force an
  NPC→NPC kill; confirm both directions score.
- Visual review required: confirm `3`, `2`, `1`, `GO!!!` on the **first**
  countdown after joining, and again on the second round.
- Still unverified: all runtime behavior; only compilation was proven.

## Related feature record

- `docs/features/gamemodes/ffa mode issues.md`
