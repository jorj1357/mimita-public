# Nothing inventory item

Time: 2026-10-07T13:02:13-04:00

## Change

- Added the existing `nothing` weapon/tool definition to every explicit weapon set. It remains the next logical slot in each set, so existing weapon slots do not move.
- The shared equip path already treats `equipslot 0`, `equip 0`, and the normal unequip action as equipping `nothing` at native slot 18. The item has no model, no collision capsule, no attack, and no audio.
- The server now receives `nothing` as an owned loadout item for role and community weapon-set validation, so selecting empty hands is replicated like any other equip.

## Validation

- JSON validation passed for `config/weapons.json` and `config/weaponsets.json`.
- Fresh cold build succeeded: `mimita-20261007T-nothing-item.exe`.
- Version identity and canonical logging passed. Journal: `logs/10-07-2026/20261007_130130/events.jsonl`.
- Connected gameplay and visual acceptance of switching to Nothing still require human review.

## Hidden Nothing and repeat-key unequip

Time: 2026-10-07T13:12:59-04:00

- Normal number-key weapon selection now toggles the active weapon: pressing
  the same key again selects the Nothing item through the shared `equipslot 0`
  path. Explicit `equipslot18` remains available for directly selecting the
  native Nothing slot.
- The hotbar now builds a compact visible-entry list and filters out the
  `nothing` definition, so Nothing is never displayed as a slot or label.
- Both the screen crosshair and world/physical-aim crosshair are suppressed
  while Nothing is equipped. Wildcard weapon sets retain their normal hotbar;
  explicit sets omit only Nothing.
- Validation: the v2 fresh build succeeded as
  `mimita-20261007T-hidden-nothing-toggle-v2.exe`, with the changed HUD
  translation unit compiled. `--versioninfo` reported
  `logs/10-07-2026/20261007_131257/events.jsonl`. Connected gameplay and
  visual key-toggle acceptance remain open.
- The Juggernaut fighter picker lock now applies only while the picker is
  visible. After a fighter has selected a weapon, pressing that same number
  key again also toggles to Nothing.
- Final rebuild: `mimita-20261007T-hidden-nothing-toggle-v3.exe` succeeded
  with the input translation unit compiled. Its versioninfo journal is
  `logs/10-07-2026/20261007_131326/events.jsonl`.

## Shared repeat-key toggle and regression record

- Moved same-slot toggling into the shared `equipslotN` terminal command
  owner. Number keys and arbitrary key bindings that invoke `equipslotN` now
  share the same behavior: equip on the first press, select hidden Nothing on
  the second press.
- Added the append-only regression record
  `docs/regressions/2026-10-07/repeat-key-unequip-REG.md`, currently marked
  `ATTEMPTED FIX (1)` until live key-binding gameplay confirms it.
- Validation: `mimita-20261007T-repeat-key-unequip.exe` built successfully
  with `src/terminal/weapon-commands.cpp` compiled. Its versioninfo journal is
  `logs/10-07-2026/20261007_135123/events.jsonl`.
