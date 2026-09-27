# NPC spawn audio hot + remaining audio-policy audit

Date: 2026-09-16 05:30 EST (UTC 2026-09-16T09:30:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live audio is proof debt)

## NPC AUDIO
- cold owners found: `npc-spawn.cpp` (`npc_spawn` one-shot, owner id),
  `npc.cpp` dash (migrated Round 62), `npc-combat.cpp` weapon fire (already hot).
- one-shot migrated? Yes: `npc-spawn.cpp` now emits `effect.actor.sound`
  ("actor.spawn") -> hot `audio.play`; cold `AudioManager::play` fallback.
- owner/loop migrated? No real shipping NPC persistent loop found; the mechanism
  exists (Round 63) but no policy to migrate.
- same EntityId across sound-policy changes? N/A (one-shot; no owner state used).

## OWNER POSITION-FOLLOW
- required by real shipping case? No (spawn is one-shot; music is streamed).
- implemented or deferred? Deferred (recorded as a future generic extension).
- why? No real persistent spatial loop currently needs it; mission says do not
  build it speculatively.

## MUSIC
- global slot: mechanism available (`owner=0`, `slot=music.primary`).
- hot track policy: not migrated.
- stop/replace behavior: generic SET/STOP ready but music uses a streaming path.
- cold streaming status: `MusicManager` (miniaudio streaming, menu random /
  ingame playlist) — a real policy leak needing streaming in the generic slot
  path; recorded, not migrated.

## AMBIENT
- hot policy? Not migrated.
- multiple slots? Supported by the mechanism; no separate cold ambient owner
  found to migrate.

## INTERACTION AUDIO
No interaction-specific cold sound manager found beyond existing one-shot paths;
recorded.

## TOOL/WEAPON AUDIO RE-AUDIT
Weapon fire sound is hot (`effect.weapon.fire.sound`); no cold per-weapon sound
branch remains for fire sound.

## GENERATION SAFETY
Slots live cold-side; a swap does not invalidate them (Round 63, tested).

## ENTITY DESTRUCTION SAFETY
Slots owned by a dead entity are stopped/cleaned (Round 63, tested).

## RUNTIME-UNKNOWN SLOT/SOUND
Accepted with no cold enum (Round 63, tested).

## COLD AUDIO POLICY LEAKS REMAINING
1. `MusicManager` track selection/streaming (real leak; streaming mechanism gap).
2. `AudioManager::play` direct one-shot sites not yet migrated (e.g. some
   `audio/audio.cpp` helpers, misc interaction sounds) — legacy/one-shot.

## AUDIO MECHANISM COMPLETE ENOUGH?
Yes (one-shot, owner, slot, loop SET/STOP, idempotence, replace, entity cleanup,
unknown slot/sound; owner-follow deferred).

## AUDIO POLICY COMPLETE ENOUGH?
Nearly for one-shots: UI, NPC one-shot (dash/spawn), weapon fire hot. Remaining:
music state (streaming), ambient (none found), interaction (legacy). Not yet
declared complete.

## DID ANY WORK REQUIRE KILLING mimita.exe?
No (a concurrent `tool-entity-continuity-selftest.cpp` edit broke one cold build;
waited/retried).

## COLD-RESTART MATRIX
Hot now: UI sound policy, NPC dash/spawn one-shot, weapon fire, slot mechanism.
Remaining: music track policy, ambient (if any), interaction.

## LIVE-PROOF DEBT
Audible spawn/loop/music/ambient behavior.

## NEXT LARGEST REAL COLD OWNER
Music streaming policy (needs a streaming-capable generic slot or reuse of the
MusicManager stream behind the slot), then the resource-lane convergence sequence.

## Files changed
`src/npc/npc-spawn.cpp`, `src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
