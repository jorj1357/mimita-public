# Damage volume recovered and proven in mode 10

Date: 2026-10-11

## Summary

Recovered the authored `damage_volume` runtime behavior from branch
`20261010-inefficientlog` into the current checkout and kept the managed live
test on launch mode 10. The live player now routes through the shared
authoritative damage path with `ServerDamageSource::Environment`.

## Root cause found

The volume loop rejected the player when `spawnState != Active`, even though
the same server already considered the alive player eligible for authoritative
rocket damage. That gate caused `damage-volume.damage` to report only NPC hits
or zero player hits. The volume gate now uses the authoritative alive check
plus authored containment; dead players are still excluded.

## Live proof

- Launch profile: `npc-navigation`, launch mode `10`, map `zombietower4`.
- Executable: `C:\mimita-v9\.dev\builds\2058\mimita.exe`.
- Journal: `C:\mimita-v9\logs\2026-10-11\20261011_155518\events.jsonl`.
- Startup commands recorded: `hex`, teleport to
  `[-1263.483154296875, -820.2572021484375, 379.64031982421875]`, and
  `npc_delete_all`.
- `hex` was dispatched once at journal line 127.
- The first authoritative volume hit appears at lines 131-132: health changed
  from `99999` to `99994` with `source: "environment"`, followed by
  `damage-volume.damage` with `players_hit: 1`, `npcs_hit: 0`, and `damage: 5`.
- The run recorded 64 `source: "environment"` damage events and 64 volume
  events with `players_hit: 1`.

## Build and scope evidence

- Managed dev-loop build 2058 completed and server/client executable identities
  matched.
- The authored test volume radius was restored to 5.0 after the intermediate
  diagnostic run; temporary hunter HP and NPC-count test overrides were also
  restored.
- The mode 10 launch configuration retains the requested `hex`, teleport, and
  `npc_delete_all` startup commands.
- `git diff --check` passed for the current working tree.

Human visual acceptance was not claimed; this result is authoritative runtime
and journal evidence from a real executable session.
