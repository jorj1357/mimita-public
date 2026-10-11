# Juggernaut mode loadout fix

The previous change updated weapon set 7 (`juggernaut`), but the active
Juggernaut mode role uses weapon set 11, `juggernaut_mode`. Updated set 11 to
grant Heavy Shotgun, Large Machine Gun, and Force Punch, followed by Nothing.

The authoritative path is:

`config/roles.json` role `juggernaut_mode_juggernaut` → `juggernaut_mode` →
`serverResolveActorSpawnProfile` → `resetPlayerForSpawn` →
`getInitialInventory` → `ServerPlayer::ownedWeaponIds`.

The new logical slots for the role are:

1. Heavy Shotgun
2. Large Machine Gun
3. Force Punch
4. Nothing

The existing player inventory is rebuilt on the next authoritative spawn or
respawn, so an already-running life may need a respawn or match restart before
the new inventory appears.
