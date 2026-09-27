# Weapon transform and collision JSONL probes

## Change

Added weapon-only structured probes to the existing single process
`events.jsonl` stream. The probes record:

- whether the hot attachment transform or cold fallback was used;
- hot attachment position, rotation, muzzle, and forward direction;
- collision grip, tip, radius, and tip-to-muzzle delta;
- body/weapon collision passes, correction, weapon contacts, body sphere count,
  grounded state, position, and velocity.

Events use the `WEAPONS` category and names `weapon.transform_probe` and
`weapon.collision_result`. No hot-reload lifecycle records were added.

## Evidence

- Hot DLL build succeeded as `build/hotreload/mimita-live-g20260921.dll`.
- Canonical cold build succeeded as
  `mimita-20260921T000857.exe` to compile the final changed legacy physics
  owner.
- No executable was launched and no existing process was replaced.
- Runtime JSONL and visual alignment remain unverified because no game process
  is available in this checkout.

## Next

Run the game, collect `WEAPONS` records during idle/aim/reload/melee/wall tests,
and compare the recorded hot muzzle and collision endpoints before tuning
hitbox sizes.
