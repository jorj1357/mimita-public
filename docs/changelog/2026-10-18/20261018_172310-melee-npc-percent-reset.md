# Melee NPC percent persistence fix

- Local time: 2026-10-18 13:23 EDT; recorded at 2026-10-18T17:23:10Z.
- Branch: `20261011zt`; no commit created.
- Preserved existing user edits in the Melee action implementation and left the active build 2593 game session running.

## Change

- `src/network/server-melee-actions.cpp:727-734`
- Prior hit result used the NPC's published `target.meleePercent` as the percent multiplier input, then incremented only `target.meleePercent`:

```cpp
const float percentScale = 1.0f + std::max(0.0f, target.meleePercent) / 100.0f;
const float percentAdd = action->basePercent * force * percentScale;
const float kb = action->baseKnockback * action->knockbackForceScale * force;
target.meleePercent += percentAdd;
```

- Exact updated behavior: increment the persistent `NpcMeleeState::percent`, include the percentage multiplier in knockback, then mirror the persistent value to `ServerNpc::meleePercent`:

```cpp
NpcMeleeState& targetState = g_npcMelee[target.entityId];
const float percentScale = 1.0f + std::max(0.0f, targetState.percent) / 100.0f;
const float percentAdd = action->basePercent * force * percentScale;
const float kb = action->baseKnockback * action->knockbackForceScale * force * percentScale;
targetState.percent += percentAdd;
target.meleePercent = targetState.percent;
```

- Added `percent_added` and `percent_after` fields to the player-to-NPC confirmed-hit event, and recorded the persistent-state regression in `docs/regressions/2026-10-18/melee-npc-percent-reset-REG.md`.

## Evidence

- Existing live run `20261018_171459` recorded 9 player-to-NPC confirmed hits and 13 NPC-to-player confirmed hits. One player-to-NPC `a_ground_punch` hit at tick 5569 reported about 2.998 percent added. Positive hit events prove the calculation executed, but do not prove player-visible NPC percent persistence.
- Canonical dev-loop build 2594 succeeded: 1 translation unit compiled, 554 skipped. The published executable's `--versioninfo` check passed and identified `.dev/builds/2594/mimita.exe`.
- The active client and server remain on build 2593; no new gameplay run was started. Runtime persistence and visual acceptance of this fix remain unverified.
- `git diff --check` passed for the edited C++ file and the new regression record. No tests were added or run.
