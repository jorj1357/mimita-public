# Juggernaut team outlines and first-person camera

Date: 2026-10-07

## Change

- Juggernaut mode now explicitly forces first-person view.
- Both Fighter and Juggernaut actor presets explicitly declare first-person
  perspective, while preserving their separate FOV and movement values.
- Added a bounded structured identity record showing the local team and actor
  counts used by the green-friendly/red-enemy outline decision.

## Evidence

- Source/config changes are present in the working tree.
- Build: `mimita-20261007T-juggernaut-outline-first-person.exe` completed successfully;
  15 translation units compiled and 533 were reused.
- Version info: `logs/10-07-2026/20261007_225328/events.jsonl`.
- Runtime/human visual acceptance: not performed in this turn; the game was not
  launched, so the live outline colors and first-person camera still need to be
  observed in a real Juggernaut session.
