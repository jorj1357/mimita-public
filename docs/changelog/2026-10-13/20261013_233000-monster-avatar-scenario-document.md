# Monster avatar spawn and rebind scenario document

Date: 2026-10-13

Updated `C:\mimita-v9\devscripts\scenarios\readmewhat.md` and standardized
the scenario-file references in
`C:\mimita-v9\docs\specs\dev-loop\dev-loop.md` to that exact path.

The scenario now covers:

- immediate `monstertest1` avatar load and snapshot;
- fixed-tick forward movement;
- Jason as a known-working control avatar;
- reloading the monster avatar after Jason;
- a second forward movement and snapshot;
- command-delivery, avatar-policy, position, pose, and offset-rebind checks;
- required `events.jsonl` event order and first-divergence reporting.

The JSON block was parsed successfully: scenario ID
`monster_avatar_spawn_walk_offsets_v1`, eight steps, and nine assertions.
No runtime code was changed and no live gameplay acceptance is claimed.

