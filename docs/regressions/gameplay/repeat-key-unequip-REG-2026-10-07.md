# Repeat-key weapon unequip

Time created: 2026-10-07T13:50:50-04:00
Time last updated: 2026-10-07T13:50:50-04:00
Status: ATTEMPTED FIX (1)

## Goal behavior

For every weapon-selection key or key binding:

1. Press the key once: equip the weapon bound to that action.
2. Press the same key again while that weapon is equipped: unequip it by
   selecting the hidden `nothing` item.
3. Pressing a different weapon key switches weapons normally.
4. `equipslot18` remains the explicit console action for selecting Nothing.
5. Nothing is not displayed in the hotbar and has no crosshair.

## Reported regression

The old behavior was observed after repeated attempts to add Nothing and
number-key toggling: a key could equip its weapon, but pressing the same key
again did not consistently unequip it. The previous toggle lived in the
hard-coded number-key input loop, so commands invoked through custom key
bindings still used direct equip behavior.

## Previous attempts

- Idempotent equip work intentionally stopped same-slot selection from
  toggling off; that fixed accidental unequips but did not implement the new
  repeat-key contract.
- The Nothing item and slot-0 fallback were added, and the number-key loop was
  given a local same-slot check. That covered ordinary number input but left
  bound `equipslotN` commands with different behavior.

## Current attempted fix

- Moved same-slot toggle behavior into the shared `equipslotN` command owner
  in `src/terminal/weapon-commands.cpp`.
- Number keys and custom key bindings now both invoke that same command path.
- `equipslot18` selects Nothing directly and remains idempotent when Nothing is
  already selected.

## Evidence boundary

Source and build validation are required before this can become a confirmed
solution. Human gameplay review must verify number keys, a custom bound key,
weapon switching, hidden hotbar state, and absence of the crosshair in a live
client. Do not mark this regression solved from compilation alone.
