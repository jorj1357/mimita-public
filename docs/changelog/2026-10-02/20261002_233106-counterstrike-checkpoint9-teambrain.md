# Counter-Strike Checkpoint 9 — TeamBrain and AI objective/grenade reasoning

Date: 2026-10-02
EST timestamp: 2026-10-02 23:31:06 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and pure team/grenade policy evidence are proven. Live NPC
objective play is NOT visually verified and remains required.

## Scope

Checkpoint 9 of `docs/specs/20261002plan.md`: the first useful TeamBrain and AI
grenade/combat reasoning (Stages 14, 15).

## Pre-existing / external edits (not mine)

Same external `config/weapons.json` `beam_thickness` change and runtime-written
user settings flagged since Checkpoint 3. Untouched.

## Files changed

### `src/npc/team-brain.h` / `.cpp` (new)

- `TeamAssignment`, `TeamSiteInfo`, `EnemyReport`, `TeamObjectiveContext`,
  `TeamAssignmentPolicy`, `TeamBrainState`.
- `TeamBrain`: `reportEnemySighting`, `tickReports` (confidence decay + drop),
  `bestReport`, `updateAssignments` (deterministic apportionment; one bomber
  carrier, attackers per site; CT defenders + one rotator; retake when planted),
  `assignmentFor`, `objectiveTargetPosition`.
- `teamBrainSelfTest`.

### `src/network/server-gamemode.h` / `.cpp`

- `ServerGamemodeState` gained `teamBrainA{0}` and `teamBrainB{1}`.
- `serverTeamBrainTick`: fills sites/objective context, records shared visual
  enemy reports, recomputes assignments, and pushes objective context onto every
  NPC's `UtilityContext`. Never moves actors.
- Called from `serverGamemodeTick` after `updateActorStates`/area effects.

### `src/gamemode/gamemode.h` / `.cpp`

`GamemodeTeam` gained `attackersPerSite`, `defendersPerSite`, `oneRotator`
(parsed from JSON, 0/-1 = brain default).

### `src/npc/npc-utility.h` / `.cpp`

- `GrenadeThrowContext`, `scoreGrenadeThrow`, `grenadeThrowAllowed` (rejects
  wall collision, self-damage, friendly fire, duplicate utility, no benefit).
- `FightMemory` (bounded dodge/jump/held counts + decay).
- `npcGrenadeReasoningSelfTest`.

### `src/game/game-cli.cpp`

Added `--team-brain-selftest` and `--grenade-reasoning-selftest`.

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 9.

## Reasoning

TeamBrain owns team-level intent and shared information, while the local
ActorBrain and shared navigation remain the only executors — matching the plan's
"TeamBrain assigns intent, ActorBrain executes". Objective context now reaches
the utility goals built in Checkpoint 5, so objective scoring is live. Grenade
reasoning is a pure, bounded, tested decision surface that rejects obviously
invalid throws.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stages 14, 15; Checkpoint 9).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.

## Validation

Build: `BUILD SUCCESS`, no warnings.

Runtime (`mimita.exe`):

```text
[TEAM BRAIN SELFTEST] PASS
[GRENADE REASONING SELFTEST] PASS
[GRENADE SELFTEST] PASS
[AREA EFFECT SELFTEST] PASS
[MAP CONFIG SELFTEST] PASS
[OBJECTIVE SELFTEST] PASS
[NPC UTILITY SELFTEST] PASS
[NPC NAV REQUEST SELFTEST] PASS
[NPC PERCEPTION SELFTEST] PASS
[GAMEMODE SELFTEST] PASS
[ACTOR PRESET SELFTEST] PASS
[CS ROUND SELFTEST] PASS
```

Note: one transient link failure (`mimita.exe: Permission denied`) occurred
because the output was briefly locked; a retry linked successfully.

## Known limitations / follow-ups

- `serverTeamBrainTick` runs after `simulateSharedNpcs`, so objective context
  reaches NPCs one tick later (fine at 60 Hz).
- Assignments are computed but consumed via the utility objective context rather
  than the assignment enum directly; the enum surface is ready for the next
  migration step.
- Enemy reports are exact/visual only; hearing reports are not yet pushed into
  `reportEnemySighting`.
- Grenade decisions are not yet triggered from the NPC combat/throw loop; the
  scoring/rejection API is ready and tested.

## Human review still needed

- In a live CS round confirm Terrorists advance/carry/plant and Counter-
  Terrorists defend/rotate/retake/defuse, that NPCs share enemy information, and
  that NPCs do not teleport.
- Confirm NPCs reject obviously invalid grenade throws.

## Explicitly not done yet

Debug tooling and full runtime acceptance (Checkpoint 10).
