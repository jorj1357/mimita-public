# Juggernaut Horde

**Purpose:** define the roster, round rules, reward feedback, NPC combat, and
acceptance requirements unique to Horde.
**Audience:** gameplay, NPC, networking, presentation, and mode contributors.
**Does not:** create separate actor, weapon, damage, or round-lifecycle systems.


`jugger_horde` is a separate selectable mode. It fills the normal
Juggernaut Fighter team to its current 64-actor capacity and fields four
Juggernauts. Humans join Fighters; NPCs fill open Fighter slots and all four
Juggernaut slots. A Fighter respawns on the next fixed update after death;
a dead Juggernaut stays out until the next match. Each Juggernaut kill of a
Fighter restores 10% of the Juggernaut's role maximum HP, capped at maximum,
for both human and NPC Juggernauts. Fighter kills grant no kill healing.

Fighters win when every Juggernaut is eliminated. Juggernauts win if at least
one survives until the authoritative active-match deadline at 300 seconds,
which is 18,000 ticks at 60 Hz. Elimination is evaluated before timeout on
the same tick, so a final Juggernaut death on the deadline wins for Fighters.
Horde Fighters reuse the Juggernaut Fighter role and actor preset: fast
`retrograd_fast` movement and forced 100-degree FOV. Juggernauts reuse the
Juggernaut mode role and `juggernaut_arcade` preset: `juggernaut` movement and
forced 50-degree FOV.
The mode reuses the shared team roster, actor lifecycle, kill credit, health,
and results owners. Team respawn delay, team kill-heal percentage, and a
configured timeout winner are general mode data rather than Horde-only logic.
The match HUD shows the authoritative time remaining at the top, starting at
`5:00` and counting down to `0:00`. Every match uses the shared visible
`3 → 2 → 1 → GO!` countdown. Results name the winning team, then the next
intermission and countdown start automatically. This cycle repeats indefinitely.

### Horde presentation and reward feedback

Horde uses the shared presentation settings declared for Juggernaut mode: damage numbers and hit effects off; world impacts, blood, killfeed, ragdolls, and hit flinch on; hit markers and hit sounds off; friendly outlines off; enemy outlines red at 25% opacity. Existing effect scales remain unchanged. `presentation.gold_reward_popup: true` shows the ordinary NPC reward popup; the authoritative reward remains +0 gold and +10 XP.

### NPC teams and combat proof

The human is a Fighter. Fighter NPCs target only Juggernaut actors, and Juggernaut NPCs target only Fighter actors. Friendly fire remains off. For any failure, inspect the same run's server and client `events.jsonl` from roster/team assignment through target selection, attack gate, firing, confirmed damage, and client receipt. A build or target-selection event alone does not prove the attack or damage worked.

### Smooth loading and performance acceptance

Initial joining is shown as Loading with the current loading step and total elapsed time (seconds and milliseconds) until the client is ready. Reconnecting is shown only after an established connection is actually lost. Each stage reports its duration and uses its existing watchdog; a stall names the stage and offers Retry or Cancel. The ideal target is 0.01 seconds (10 ms), measured per stage and improved toward; it is not a universal pass/fail limit. Horde investigations record server tick cost, NPC work, client frame time, snapshot age, and connection-state transitions in the same run before assigning a slowdown cause.