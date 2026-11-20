# Zombie Attack

**Purpose:** define an endless cooperative monster survival mode built around
escalating waves.
**Audience:** gamemode, NPC, combat, map, and content contributors.
**Does not:** replace Zombie Tower's authored floor progression, checkpoints,
or boss-run contract; this mode uses the shared gamemode lifecycle and existing
monster, spawn, damage, and map systems wherever practical.

## Core run

- Players defend themselves together against successive monster waves. The
  run continues until the party is defeated or the server ends the match.
- Wave 1 contains five monsters total, selected randomly from the starting
  monster pool. Spawn locations must be valid authored spawn locations for the
  active map.
- A wave is cleared when every monster spawned for it is dead. The next wave
  then begins after a short, visible breather so players can reload, regroup,
  and recognize the transition.
- Track and present the current wave. The wave transition must not silently
  remove living monsters or reset the party.

## Escalation and monster roster

- Each successive wave increases the total number of monsters and raises the
  monsters' movement speed and damage. Apply these as server-authoritative
  wave modifiers through existing NPC/combat owners; do not create a second
  damage or movement system for this mode.
- Scale difficulty by wave number using mode data where practical. Keep the
  count and stat curves configurable so playtesting can tune them without
  changing shared lifecycle code. Avoid values that exceed the supported
  movement, damage, or actor limits.
- Begin with a small, readable monster pool. Add distinct monster types at
  progressively later waves; once introduced, a type may appear in later
  random selections. The pool and each type's first eligible wave are
  configurable mode/content data.
- Random selection should vary composition while respecting the unlocked
  roster and any authored spawn or actor limits. A wave must still reach a
  finite, clearable set of spawns even when a selected type cannot be spawned.
- Bosses and special wave events are optional extensions. This initial
  contract does not require a fixed final wave or a boss schedule.

## Mode identity and completion

- Stable mode ID: `zombie_attack`; player-facing name: **Zombie Attack**.
- Use the common gamemode manager for map choice, roster, countdown,
  participants, results, and transitions. Wave state and wave-spawn decisions
  are authoritative on the server and replicated for presentation.
- A defeat ends the run and shows the highest wave reached in results. Any
  persistent rewards, currency, unlocks, or leaderboard rules need their own
  explicit progression contract before implementation.

## Runtime event evidence

The server journal is the authority for wave state. Emit transition events only
after the shared gamemode/NPC systems apply them. Keep the normal event envelope
and correlate by `run_id`, mode, wave, actor, and tick. Reuse the shared
`monster.spawned` event for each wave actor; extend it with `mode_id`,
`wave_number`, `monster_id`, and the authored spawn source instead of emitting a
parallel spawn-success event.

| Event | Required fields | Proves |
|---|---|---|
| `zombie-attack.wave.started` | `mode_id`, `wave_number`, `wave_seed`, `planned_monster_count`, `unlocked_monster_ids`, `wave_revision`, `tick` | The server began this wave using the selected data revision. |
| `monster.spawned` | Existing actor/role/map/zone/position fields plus `mode_id`, `wave_number`, `monster_id`, `spawn_source`, `wave_revision` | One actual actor was created as part of the named wave. Wave 1 passes only with five distinct successful spawn records. |
| `zombie-attack.wave.modifiers.applied` | `mode_id`, `wave_number`, `actor_id`, `speed_multiplier`, `damage_multiplier`, `source_revision`, `tick` | The actor received the effective server-side wave scaling. |
| `zombie-attack.roster.unlocked` | `mode_id`, `monster_id`, `first_wave`, `pool_revision`, `available_pool_ids`, `tick` | A newly introduced monster became eligible for random selection at the configured wave. |
| `zombie-attack.wave.cleared` | `mode_id`, `wave_number`, `spawned_count`, `killed_count`, `remaining_alive`, `next_wave_delay_ticks`, `tick` | Every spawned monster for this wave is dead and the next-wave transition is eligible. |
| `zombie-attack.run.ended` | `mode_id`, `reason`, `highest_wave`, `participants`, `tick` | The shared run ended and reports its actual final wave. |

Per-actor modifier events are emitted on spawn or actual modifier change, not
every tick. Rejected/invalid spawn attempts retain their reason and must not
count toward the successful five-monster wave-one total.

## Acceptance scenarios

1. Start Zombie Attack and observe wave 1 spawn exactly five monsters.
2. Clear a wave and observe one subsequent wave begin without a mode restart.
3. Compare consecutive waves and confirm the later wave has more monsters,
   faster movement, and higher damage under the configured scaling.
4. Reach the configured introduction point for another monster type and
   confirm it can enter the random pool from that wave onward.
5. Continue through multiple waves and confirm the run ends on party defeat,
   with the final results reporting the highest wave reached.

The live journal must show one `zombie-attack.wave.started` for wave 1, exactly
five distinct successful `monster.spawned` records for that wave, and one
`zombie-attack.wave.modifiers.applied` for each spawned actor. Later waves must
show increased successful count, speed and damage modifiers, then a roster
unlock at its configured first wave. Clear a wave in live play and confirm the
breather and following wave occur without restarting the match. Synthetic
simulation or config parsing alone is not runtime proof.

Compilation and configuration parsing alone do not establish that waves,
scaling, or roster introductions work in a live match.
