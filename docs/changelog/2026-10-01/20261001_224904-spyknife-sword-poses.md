# Spy Knife normal melee poses

## Change

- Removed the Spy Knife backstab-readiness geometry and automatic re-arm path.
- Spy Knife now remains a physical contact melee weapon while equipped, and a
  left click restarts the shared slash pose for the configured 126 ticks.
- Added a JSON-owned defensive pose triggered by secondary fire. It uses the
  existing generic weapon pose state and returns to the normal equipped pose
  after 126 ticks.
- Renamed the shared runtime pose key from `swordPoseState` to
  `weaponPoseState`, including remote-player presentation paths.
- Kept the existing packet byte reserved and always zero for current Spy Knife
  contacts so the wire layout does not change.

## Evidence

- Active `config/weapons.json` and `config/animations.json` parse successfully.
- Repository search found no active Spy Knife backstab-readiness symbol or old
  `swordPoseState` reference.
- `python build_agent.py` compiled the changed source and reported `BUILD
  SUCCESS` with return code 0.
- The linker also reported `mimita.exe(.rsrc) is too large`; no clean linked
  executable artifact was claimed from that run.

## Human acceptance

Not performed in-game. Verify that left click visibly enters the slash pose,
right click crosses the right arm defensively, repeated left clicks restart the
pose, and both poses return to idle after the configured tick duration.
