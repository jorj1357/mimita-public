# REG: holding F did not defuse for non-designated human CTs

Time created: 2026-11-04T11:10:00Z
Time last updated: 2026-11-04T11:10:00Z

Status: ATTEMPTED FIX (1) — live journal proof recorded; human play confirmation pending

Related specification:
`docs/specs/gamemodes/funcounterstrike.md`

Related changelog:
`docs/changelog/2026-11-04/20261104_111000-fun-cs-defuse-progress-npc-stalls.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-11-04T10:42:41Z` and earlier play sessions.

### Expected Behavior

Any Counter-Terrorist in range of a planted bomb can hold Interact (F) and
defuse. The NPC strategy assigns one CT as the primary defuser, but that does
not remove the human interaction.

### Actual Behavior

Holding F produced no `objective.defuse-start` unless the local player was the
single `reportedDefuserId` picked at plant time. Because Fun Counter Strike
respawns instantly, that designation could persist and lock a human out for the
whole match.

### Why This Is Bad

The primary user action in the bomb mode silently did nothing. The user had no
feedback explaining why.

### Specification

`docs/specs/gamemodes/funcounterstrike.md`

Relevant requirement: "After a plant, the nearest CT is assigned to defuse; the
others cover it." This is the NPC strategy rule; it does not prohibit another
human CT from defusing.

### Wrong Code

File: `src/network/server-gamemode.cpp`

```cpp
for (const auto& kv : players) {
    const ServerPlayer& p = kv.second;
    if (p.dead || objectiveActorTeam(d, kv.first) != defenderTeam) continue;
    if (designatedDefuser != 0 && kv.first != designatedDefuser) continue;
    if (!p.input.interactHeld && p.pendingInteractTaps == 0) continue;
    // ...
}
```

The designation was also never reset when the bomb left `Planted` or at round
assignment.

### Confirmed Cause

`designatedDefuser` came from `teamBrainA.reportedDefuserId`, which is chosen
once from players and NPCs. The player loop treated it as an exclusive lock.
The lock survived round resets and instant respawns.

Evidence: source inspection of the player and NPC loops and the assignment
block; commit `20578d11` added the player filter.

### Attempted Fix 1

Time: `2026-11-04T10:35:33Z`

Change:

- Keep the designated-defuser filter for NPC candidates only.
- Prefer the current human defuser while they keep holding F.
- Add `objective.interaction-rejected` evidence.
- Reset the designation whenever the objective is not planted and at round
  assignment.

Result: live journal `logs/2026-11-04/20261104_104100/events.jsonl` recorded
`objective.defuse-start` with `operator=1`, `operator_kind="player"`, and
`designated_defuser=100002` (an NPC), followed by `objective.defused` with
`operator=1` and a CT round win. Human play confirmation is still pending.

### Corrected Code

File: `src/network/server-gamemode.cpp`

```cpp
// Human players are never gated by the NPC designation.
for (const auto& kv : players) {
    const ServerPlayer& p = kv.second;
    if (p.dead || objectiveActorTeam(d, kv.first) != defenderTeam) continue;
    if (!p.input.interactHeld && p.pendingInteractTaps == 0) continue;
    // ...
}

// NPCs still use one designated defuser.
if (designatedDefuser != 0 && kv.first != designatedDefuser) continue;
```

Plus:

```cpp
if (d.objective.state != ObjectiveState::Planted)
    d.teamBrainA.state().reportedDefuserId = 0;
```

### Proof

Automated/runtime proof:

- Build `2026-11-04T10:35:33Z`.
- `logs/2026-11-04/20261104_104100/events.jsonl`:
  - `objective.defuse-start` `operator=1`, `operator_kind=player`,
    `designated_defuser=100002`.
  - `objective.defused` `operator=1`.
  - `round.result` winner `Counter-Terrorists`.

Human review: pending play confirmation of the prompt and world readout.

### Solution

`ATTEMPTED FIX (1)`, not yet `SOLUTION AS OF`, until human play confirms the
visible behavior.
