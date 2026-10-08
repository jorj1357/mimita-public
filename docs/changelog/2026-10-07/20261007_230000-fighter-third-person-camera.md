# Fighter third-person camera

Date: 2026-10-07

## Change

- Removed the Juggernaut mode-wide first-person override so it no longer
  overrides the Fighter role.
- Fighters now use an explicitly configured third-person perspective and do not
  force the camera perspective.
- Juggernauts remain forced into first-person through the
  `juggernaut_arcade` actor preset.

## Evidence

- Source/config evidence: `config/gamemodes/juggernaut.json` no longer forces
  first-person; `config/actor-presets/juggernaut_fighter.json` selects
  third-person without forcing it; `config/actor-presets/juggernaut_arcade.json`
  still forces first-person.
- Build: `mimita-20261007T-fighter-third-person.exe` completed successfully;
  548 translation units were reused and the new executable was linked.
- Version info: `logs/10-07-2026/20261007_230658/events.jsonl`.
- Runtime/human visual acceptance: not performed in this turn; the game was not
  launched, so the Fighter third-person camera and Juggernaut first-person lock
  still need to be observed in a real session.
