// 2026-09-11T04:45:49Z
/* purpose
* record temporary per-NPC emotion + bounded memory fed into existing behavior math
* preserve exact source, config, build, and headless runtime evidence
* this file does NOT claim suspicion, stealth, social reputation, or HUD work
* this file does NOT replace the append-only regression record
*/

# NPC runtime emotion and bounded memory

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-11T04:45:49Z`
- Display timezone: America/New_York
- Display time: `2026-09-11 00:45:49 EDT`
- Pre-existing changes: all prior actor/role/behavior/target/weapon/navigation
  slices were preserved. This session separates stable personality (profile)
  from temporary match state (emotion/memory).
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and other
  `docs/changelog/` files were left untouched.

## 1. Files changed

New:
- `src/npc/npc-mind.h` / `npc-mind.cpp`: emotion + memory structs, event hooks,
  decay, and effective-value helpers.

Modified:
- `src/npc/npc-behavior.h` / `.cpp`: nine emotional baseline/sensitivity fields.
- `config/behavior-profiles.json`: emotional values for balanced/aggressive/
  nervous.
- `src/npc/npc.h`: `NpcRuntimeEmotion emotion`, `NpcMemory memory`.
- `src/npc/npc.cpp`: `npcMindUpdate`, reaction delay via emotion, memory-driven
  investigate goals.
- `src/npc/npc-combat.cpp`: emotion added to effective aim error and aggression.
- `src/npc/npc-state-machine.cpp`: emotion added to state scoring (retreat
  bonus + effective aggression).
- `src/network/server-npcs.cpp`: damage/kill hooks; panicked stickiness in
  target scoring.
- `src/network/server-gamemode.cpp`: killer confidence + perceivable witnessed
  deaths; mind reset on spawn/respawn.
- `src/network/server-attack.cpp`, `src/network/server.h`: `ServerNpc` last
  attacker fields used to attribute player damage to the simulated NPC.

## 2. Runtime emotion structure

```cpp
struct NpcRuntimeEmotion { float panic; float fear; float confidence; float stress; };
```
Bounded 0..1, per life, never written into the behavior profile. Reset on
spawn/respawn from `base_fear` / `base_confidence`.

## 3. Memory structure

```cpp
struct NpcMemory {
    uint32_t  lastAttackerId;   glm::vec3 lastAttackerPos;  float lastAttackerAge;
                                glm::vec3 recentDangerPos;  float recentDangerAge;
    uint32_t  lastWitnessedDeathActorId; glm::vec3 lastWitnessedDeathPos; float lastWitnessedDeathAge;
};
```
`lastSeenTarget` memory is intentionally not duplicated: the existing
`NpcStateMachine::lastKnownTarget` / `lastKnownAge` already provide it and are
used by the search/investigate goal path. Memory is a fixed set of fields
(no dynamic allocation, no event history).

## 4. Event -> emotion equations

Damage (`frac = damage / maxHealth`):
```text
panic      += frac * profile.damage_panic_sensitivity
fear       += frac * profile.damage_fear_sensitivity
stress     += frac * 0.8                       (shared)
confidence -= frac * 0.3                       (shared)
```
and memory: attacker id/pos + recent danger pos, ages zeroed.

Kill: `confidence += 0.15`, `panic -= 0.1`, `stress -= 0.1` (shared).
Witnessed death (perceivable only): ally -> `fear += 0.25`, `stress += 0.20`,
`panic += 0.15`; enemy -> `confidence += 0.05`.

## 5. Decay behavior

All dt-based, bounded, no frame counters:
- panic -> 0 at `panic_decay_per_second` (profile), default 0.7.
- stress -> 0 at `stress_decay_per_second` (profile), default 0.4.
- fear -> `base_fear` at 0.15/s (shared).
- confidence -> `base_confidence` at 0.20/s (shared).
- memory ages advance by dt; entries expire naturally (no explicit reset).

## 6. Memory -> goal integration

`makeNavGoal` (no visible target) now chooses, in order: existing
`lastKnownTarget` search (Chase), then `lastAttackerPos` if
`lastAttackerAge < 6 s`, then `recentDangerPos` if `recentDangerAge < 6 s`, then
the original wander. It reuses `NpcGoalKind::ReachPosition`; no new
investigation subsystem was added. Memory supplies information; emotion and the
behavior profile decide how it is used.

## 7. Measurable differences (same role/loadout/movement)

Controlled TDM, funworld3, 12 NPCs, three profiles on opposing teams:

| profile | mean panic | mean fear | mean stress | mean confidence | mean maxError | target switches | mean target life |
|---|---|---|---|---|---|---|---|
| aggressive | 0.11 | 0.33 | 0.27 | 0.68 | 3.42 | 67 | 1.75 s |
| balanced | 0.26 | 0.52 | 0.30 | 0.44 | 6.08 | 62 | 1.63 s |
| nervous | 0.54 | 0.86 | 0.42 | 0.29 | 10.90 | 353 | 0.32 s |
| default (no role) | - | - | - | - | 0.04 | 93 | 1.77 s |

Aim error rises above the configured bases (aggressive 3 -> 3.42, balanced
5 -> 6.08, nervous 8 -> 10.90) due to panic/stress; nervous panics and switches
targets far more; aggressive keeps confidence and commitment. All from numeric
data, no name branches.

## 8. Tests / results

- Build: `python build_agent.py` => `Status: SUCCESS`.
- Event volume in the controlled run: damage 591, ally_death 353, enemy_death
  370, kill 167, memory 591; 88 kills.
- Aim response: see table (base vs effective).
- Memory: `[NPC MEMORY] actor=... attacker=... pos=(...)` written on every
  damage event.
- Witnessed death: perceivable ally/enemy deaths produce distinct emotion logs;
  perception is gated by range (25 m) + line-of-sight (`npcMindCanPerceive`).
- Respawn reset: `npcMindReset` is called on gamemode spawn and respawn, so
  emotion/memory reset while role/behavior profile persist.
- Lost-target investigation: existing `lastKnownTarget` path retained; attacker/
  danger memory extends it (code path verified; see section 6).

## 9. Performance impact

Emotion update is O(1) per NPC per tick (a few comparisons and adds). Memory is
a fixed set of fields. Witnessed-death checks run only when a kill is processed
(few per second), iterate the live NPC list once, and use one world ray each.
No per-tick allocation, no unbounded history, no per-frame emotion logging.

## 10. Regressions

- FFA (10 NPCs): `Match ACTIVE mode=ffa`, 54 kills.
- Elimination (10 NPCs): ended
  (`[PERSISTENCE] Match result emitted: mode=elimination winner=blue`).
- TDM (30 NPCs): 661 kills, ended, completed 40 s in 41.9 s wall time (no stall).
- Role health/loadout/movement logs present; behavior aim/reaction/cadence,
  scored target selection, scored weapon selection, navigation, and traversal
  were not restructured.

## 11. Smallest logical next phase

Stop adding abstract NPC infrastructure and build **Hunt V1** from the systems
that now exist: a preset/submode that assigns a small set of roles, uses
elimination/one-life plus the role/behavior/emotion/combat pipeline, and relies
on the existing HUD role/state fields for presentation.
