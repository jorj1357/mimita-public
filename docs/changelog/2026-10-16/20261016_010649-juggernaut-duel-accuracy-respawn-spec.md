# Juggernaut Duel NPC accuracy and immediate respawn specification

- Timestamp: 2026-10-16T05:06:49Z (2026-10-16 01:06:49 America/New_York, EDT)
- Branch/HEAD: `20261011zt` / `3a345200`
- Result: `SPEC_UPDATED; SPAWN_EQUIP_SYNC_IMPLEMENTED_UNBUILT; LIVE_ACCEPTANCE_BLOCKED`

## Outcome

Updated `docs/specs/gamemodes/juggernaut-duel.md` with the existing NPC aim
owner and baseline, a Duel-scoped requirement for substantially improved
accuracy, fairness constraints, live acceptance observations, immediate
respawn timing goals, stale-life-state checks, and an explicit unresolved
storage-scope question. Follow-up live evidence identified a missing active
weapon synchronization step at player spawn; that source fix is implemented
but not yet built or runtime-verified.

## Evidence and decisions

- Duel roles currently select `juggernaut_limited` from `config/roles.json`.
  `config/behavior-profiles.json` configures it with `aim_error_deg: 10.0` and
  `reaction_delay: 0.55`; the spec now calls for a Duel-specific tuning owner
  so this mode's difficulty does not silently alter standard Juggernaut.
- Duel currently sets `respawn_seconds: 0.01` in
  `config/gamemodes/juggernaut_duel.json`. `serverMatchRespawnSeconds()` in
  `src/network/server-gamemode.cpp` returns that duration; NPC respawn countdown
  in `src/network/server-npcs.cpp` advances on fixed ticks. The spec clarifies
  the desired zero-delay lifecycle boundary and separates simulation scheduling
  from network/render latency.
- `docs/architecture/player-npc-systems/npc-movement.md` sections 49-51 permit
  prediction from observed state and prohibit future-input reading. The Duel
  contract preserves perception, cover, weapon, cooldown, and authoritative
  trace rules.
- “Storage issues” has no identified stored value or symptom. It is marked
  `NEEDS_SPEC_DECISION`; no persistence behavior was invented.

## Follow-up source change and live evidence

- `docs/specs/gamemodes/juggernaut-duel.md` now records the 2026-10-16 05:07Z
  live finding: the role has `heavy_shotgun` as starting weapon, but
  `resetPlayerForSpawn` did not set the server equipped slot and
  `PlayerRespawnedPacket` did not transmit the active weapon identity. Build
  2427's run `logs/2026-10-16/20261016_050752/events.jsonl` emitted 2,411
  `weapon.equip.rejected` events over about 46 seconds for requested native
  slot 10 (HAFS), set 11, resolved slot -1. No attack verdict was recorded.
- Implemented source changes: `src/network/server-players.cpp` selects a
  validated role starting weapon and places its network identity in the spawn
  packet; `src/network/packets.h` carries that identity;
  `src/engine/engine-tick-net.cpp` validates and equips it locally before the
  next input. Invalid requests remain unauthorized.
- The Dev Loop observed source generation 5 and remained at `building` for
  several minutes without publishing a new build. Status/control calls then
  stopped returning promptly amid Windows memory pressure. Build 2427 is still
  the last successful executable and predates these source changes. Therefore
  compilation, fresh spawn synchronization, accepted attacks, damage, and
  client health replication are not verified. Do not claim the Duel networking
  fix is complete.

## Review route and validation

- Read `docs/ROUTER.md`, the documentation and behavior review skills, the Duel
  and Juggernaut specs, NPC movement architecture, networking respawn section,
  animation ownership, data-saving spec, and relevant source/config owners.
- Router references `docs/doc-review-09-03-2026.md`, but that file is absent.
- Router's TODO rule also surfaced `docs/specs/gamemodes/gamemodes.md` line 3
  (“todo this not good but move tothis version more more general things for gamemodes”)
  and line 191 (“Todo explain this 9 2 2026 0848”). Left those existing notes
  unchanged; the first suggests a scalable general gamemode contract, and the
  second needs its adjacent weapon-set/hotbar behavior explained.
- `git diff --check` passed for documentation edits. The canonical build was
  attempted through Dev Loop but is stalled as described above; no tests were
  added or run. Accuracy and respawn acceptance remain future runtime work.
