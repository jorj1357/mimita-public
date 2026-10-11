# Damage-volume behavior recovered from `20261010-inefficientlog`

## Scope

Compared the detached current checkout at `2e5c8fe6` with branch
`20261010-inefficientlog` and recovered only the authored damage-volume
behavior. The branch's unrelated logging, NPC, checkpoint, editor, and config
changes were not copied.

## Source change

- `ServerGamemodeState` now owns per-volume cooldown ticks.
- The fixed-tick authored map-entity runtime now checks `damage_volume`
  containment for active players and living NPCs.
- Player damage uses the shared authoritative `applyServerDamage` path and the
  existing confirmed-damage packet path.
- Environmental damage has an explicit server damage source and packet source
  value.
- Applied damage emits one bounded `damage-volume.damage` structured event.

## Build evidence

- `git diff --check` passed.
- The first managed dev-loop build failed because the current checkout lacked
  the `Environment` damage-source enum and packet mapping; that integration gap
  was corrected.
- A second managed dev-loop build was still `building` at session completion;
  no successful build artifact was claimed.

## Runtime evidence

No new runtime scenario was run. The existing server/client were left running,
and no visual, health-bar, multiplayer, or human acceptance claim is made.
