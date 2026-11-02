2026 10 31 2100 todo this combine this with the gui spec needs to eb all at one file
this file place is for gammeode idesa 

# General Objective and Round HUD

**Status:** Required behavior for every gamemode that has a timed objective.

## Purpose

A player must be able to answer these questions without opening a debug view:

- What phase is the match in?
- How much time is left?
- What is the objective doing?
- What does my team need to do next?
- Why did the round end?

The HUD is presentation only. It reads replicated authoritative round and
objective state; it must not become a second source of game truth.

## Required indicators

Every mode with a round timer or objective must provide layout elements for:

| Question | Required display |
|---|---|
| Phase | `INTERMISSION`, `COUNTDOWN`, `GO!`, `ACTIVE`, `ROUND OVER`, `MATCH OVER` |
| Unplanted/active time | Round timer, usually minutes:seconds |
| Armed/planted time | Objective timer replaces the round timer |
| Objective state | `CARRIED`, `DROPPED`, `PLANTED`, `DEFUSED`, `EXPLODED` as applicable |
| Action prompt | Plant, defuse, pick up, or defend/hold hint for the local team |
| Interaction mode | Whether the mode requires holding or repeated taps |
| Interaction range | Prompt appears within the mode's replicated `interaction_range_m`, not a hardcoded radius |
| Interaction progress | Percentage for hold mode and percentage plus tap count for repeated-tap mode |
| Round result | Winner and reason: objective, defuse, detonation, timeout, elimination, or draw |

A mode may add more specific elements, but it must not remove these questions
from the player experience.

## Timer ownership and switching

- Unplanted or unarmed: show the round timer.
- Armed/planted: hide the round timer and show the objective timer in the same
  visual slot.
- Do not show two competing countdowns at once.
- The objective timer uses the authoritative deadline and is never guessed by
  the client.
- Defused/exploded is an explicit objective state and a result reason, not just
  a generic "objective complete" state.

## Configuration ownership

- Mode objective timers (`plant_seconds`, `defuse_seconds`,
  `explosion_seconds`) are authoritative.
- Map `bomb` timer blocks are compatibility fallbacks only when the mode does
  not configure a value.
- The effective owner and values are logged once per round.
- Display strings and layout positions live in
  `config/gui/gamemode-meta-gui.json`.
- The client must tolerate a missing layout section by showing no HUD rather
  than inventing a second state model, but every shipped mode must have a
  layout section.

## Minimum layout elements

The shared HUD code recognizes these element IDs:

- `intermissionText`
- `phaseText`
- `modeTitle`
- `scoreText`
- `roundText`
- `matchTime`
- `objectiveStatus`
- `objectiveHint`
- `objectivePrompt`
- `objectiveResultText`
- `roundOverText`
- `countdownText`

Existing mode-specific elements (payload, Zombie Tower, disasters, etc.)
remain additive and keep their existing ownership.

## Acceptance

Runtime evidence must show the phase label, round timer, objective state, timer
switch on plant, team hint, and result reason. Human review must confirm the HUD
is readable at normal resolution and does not overlap other required elements.
