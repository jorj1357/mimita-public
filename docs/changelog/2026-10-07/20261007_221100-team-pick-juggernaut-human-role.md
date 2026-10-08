# Team-pick Juggernaut human role

Date: 2026-10-07

## User-visible result

- `team_pick 2`, `team_change 2`, and `change_team 2` now select the Juggernaut team in Juggernaut mode.
- Team selection immediately updates the human's authoritative actor role and reapplies the role-owned loadout for the next life. The Juggernaut role uses the `juggernaut_mode` weapon set, whose active weapon is the large machine gun.
- The `juggernaut_mode` weapon set now contains only `large_machine_gun` plus the hidden `nothing` slot; the hitscan rifle is no longer available.
- The Juggernaut Arcade actor preset now forces a 50-degree FOV for the mode.
- The existing roster builder continues filling the selected team and opposing team from the configured team capacities and role assignments.
- `team_list` remains the source for the active numeric order. In the current Juggernaut config: 1 = Fighters, 2 = Juggernauts, 3 = Spectator.

## Source owners

- `src/network/server-gamemode.cpp`: authoritative team-change role assignment and immediate authoritative spawn/loadout refresh.
- `src/network/server-packet-chat.cpp`: accepted command spellings.
- `src/terminal/debug-commands.cpp`: client console commands.
- `config/actor-presets/juggernaut_arcade.json`: Juggernaut mode camera preset.
- `config/gamemodes/juggernaut.json` and `config/roles.json`: team-to-role and capacity/loadout configuration.

## Validation

- `python build_agent.py` with `MIMITA_EXE_NAME=mimita-20261007T-team-pick-juggernaut-v1.exe`: BUILD SUCCESS; 1 translation unit compiled and the named executable linked.
- After narrowing the weapon set, `python build_agent.py` with `MIMITA_EXE_NAME=mimita-20261007T-team-pick-juggernaut-v2.exe`: BUILD SUCCESS; configuration-only relink completed.
- No executable was launched and no gameplay acceptance was claimed.
