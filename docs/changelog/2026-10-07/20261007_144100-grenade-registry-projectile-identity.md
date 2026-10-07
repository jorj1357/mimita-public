# Grenade registry and projectile identity

- Added shared throwable registration for `frag`, `smoke`, `fire`, and `darkbang`, plus the `bomb` objective-item definition.
- Added Counter-Strike weapon-set entries in logical slots 3 through 7, backed by native weapon slots 19 through 23.
- Reused the grenade-launcher projectile behavior and bouncing physics while rendering named throwables as colored spheres: pale green frag, gray smoke, orange fire, and black darkbang.
- Aligned `config/grenades.json` weapon IDs with the new definitions.
- Extended projectile replication with the dynamic weapon-definition ID while retaining the legacy grenade projectile family byte, so the selected throwable survives prediction, authoritative spawn/state, explosion, despawn, rendering, and effect lookup.
- Added the terrorist bomb to the weapon registry only. Planting, defusing, bomb-world state, and NPC utility decisions remain future stages.

Validation:

- `config/weapons.json`, `config/weaponsets.json`, and `config/grenades.json` parsed successfully with PowerShell JSON parsing.
- Forced canonical build completed successfully: 3 newly compiled units, 546 skipped, link succeeded.
- `C:\mimita-v9\mimita.exe --versioninfo` completed and emitted run metadata at `logs/10-07-2026/20261007_143935/events.jsonl`.
- Live human equip/throw/explosion/HUD acceptance was not performed in this stage.
