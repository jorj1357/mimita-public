# Heavy shotgun implementation

Date: 2026-10-10 (America/New_York)

## Outcome

Implemented a distinct `heavy_shotgun` weapon for the actual Juggernaut role.
This session does not add Juggernaut4 map work or 240 FPS work.

## Implemented

- Added the Heavy Shotgun definition with the supplied GLB and WAV assets.
- Set 1.0 second fire delay, 2.5 second reload, 8-round magazine, 888 reserve,
  40 pellets, 35 base damage, strong victim knockback, and shotgun poses.
- Added the Juggernaut role/loadout mapping; Fighters were not given this weapon.
- Added a dedicated `heavy_shotgun` entry in `config/weaponcollisions.json`.
- Added distinct legacy network presentation mapping, remote fire audio, nearly
  white 0.3-thickness tracers at 0.9 start alpha, and a 2x-scale muzzle light.
- Reworked recoil so `1.0` is neutral, larger values produce bounded camera kick,
  consecutive shots accumulate, recovery is weapon-configured, and switching
  weapons clears the active accumulator.

## Evidence

- Source/config build: `python build_agent.py` passed; 75 files compiled and the
  executable linked successfully with return code 0.
- Version evidence: `mimita.exe --versioninfo` recorded
  `logs/10-10-2026/20261010_222028/events.jsonl`.
- The actor-preset self-test loaded the Juggernaut presets but reported FAIL on an
  existing Counter-Strike expectation; that result is not used as heavy-shotgun
  acceptance evidence.
- Human in-game visual/audio, firing, reload, damage, knockback, and multiplayer
  acceptance remain pending direct playtest.

## Preserved

Pre-existing edits to `config/accounts/default.json`, `config/analytics.json`,
`src/entities/player.h`, `src/physics/movement/physics-collision-shared.h`,
the supplied heavy-shotgun assets, and the existing planning document were
preserved.
