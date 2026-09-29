# Live actor preset runtime overrides

- Fixed first-person camera evaluation so the effective `camconfig` FOV also reaches the projection; Counter-Strike's JSON FOV 70 is no longer only stored in config state.
- Fixed local weapon-slot lookup to use the active in-memory preset definition, enabling preset fire delay, reload time, damage, hitscan, and ammo rules during real weapon actions.
- Added Counter-Strike overrides for revolver and shotgun and added both to its role weapon set.
- Made presentation suppression a single top-level preset policy and applied it at hit effects, damage-number, impact, and muzzle-flash spawn boundaries.
- Removed the duplicate per-weapon presentation block from `counter_strike.json`.
- Validation: `mimita.exe --actor-preset-selftest` passed with FOV 70, forced first person, both weapon overrides, and global presentation values parsed.
