# GUI Juggernaut launch inherited Captain Falcon weapons

Related changelog: [20261117_221804-juggernaut-gui-weapon-override.md](../../changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md)

Recorded: `2026-11-17T21:52:47Z`  
Status: **FIXED IN SOURCE; GUI LAUNCH AND ROLE LOADOUTS VERIFIED IN THE LIVE JOURNAL**

## Expected behavior

Selecting launch mode `9 — juggernaut` in the dev-loop GUI should use the
Juggernaut mode and role weapon rules unless that launch preset explicitly
chooses another weapon set. A host can also start Juggernaut through the in-game
mode command and receive its role-based weapon loadouts when no host override
was selected.

## Observed behavior

Run `20261117_214010` reported launch mode 9, Juggernaut, and weapon set 12.
The server's NPC attack-gate events showed `equipped=captain_falcon_moveset`.
The user screenshot shows `9 — juggernaut` selected in the GUI.

After launch mode 9 was changed to `weapon_set: auto`, runs `20261117_215600`
and `20261117_221239` still launched in Juggernaut mode with Captain Falcon.
The latter launch included `--weapon-set 12` even after the profile was changed
to Auto, showing that a one-shot override could survive a mode change. Its
server journal is `logs/2026-11-17/20261117_221239/server-01/events.jsonl`.

## Cause

`build_server_args` omitted a weapon argument for the Auto preset, but the
persistent daemon's one-shot override could still add `--weapon-set 12` before
the mode's arguments. Mode changes did not clear that state. Separately, the
`npc-navigation` profile supplied 12 as a fallback. Either explicit value
overrode role-specific weapons. The profile is used for SSBM, where Auto
already resolves Captain Falcon from SSBM's configuration.

## Correction

Launch mode 9 declares `weapon_set: auto`. The dev-loop now clears one-shot map
and weapon overrides when the selected mode is changed. Dashboard launches in
an Auto mode ignore stale weapon values, then reload the current launch mode
and profile before building the arguments. The `npc-navigation` profile also
uses Auto. SSBM continues to get Captain Falcon from its mode configuration;
Juggernaut resolves its own roles.

The server already preserves role-specific loadouts when there is no explicit
host weapon-set override. Juggernaut roles use existing `juggernaut_mode`
(Heavy Shotgun, Large Machine Gun, Force Punch); Fighters use their own role
loadout. An explicitly selected concrete weapon set remains a host override.

## Validation and remaining evidence

- GUI-selected mode 9 launched run `20261117_221550`; its
  `dev-loop.launch.args` has no `--weapon-set` option and the launcher recorded
  successful matching server/client readiness. The exact journal is
  `logs/2026-11-17/20261117_221550/dev-loop/events.jsonl`.
- Server `gamemode.match.configured` reports Juggernaut and
  `weapon_set_source=gamemode_default` in
  `logs/2026-11-17/20261117_221550/server-01/events.jsonl`.
- Four `juggernaut_mode_juggernaut` NPCs resolve role set 11 with
  `heavy_shotgun`, `large_machine_gun`, and `force_punch`. Fighter NPCs resolve
  role set 10; the player's post-mode-transition spawn also resolves set 10.
  None of these role records uses Captain Falcon.
- No build was needed or run because the changes are Python launch logic and
  JSON launch/profile data. Runtime build identity matched the published
  artifact; this does not claim a new native build.
- Visual inspection and a separate in-game `modestart` command were not
  performed. The game specification keeps in-game mode switching on the same
  server gamemode and role-loadout path; this run directly verifies GUI launch.
