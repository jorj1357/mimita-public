# Empty-handed player arms use locomotion pose

Date: 2026-09-17

## Outcome

The player presentation bridge now derives `ActorActionState.weaponKey` from
the generic `equips-item` relationship instead of stale typed weapon mirrors.
When no tool is actually equipped, the hot animation path receives
`weaponKey=0`, so normal idle/walk arm poses remain authoritative. Equipped
tools continue to receive weapon carry and tool-phase animations.

## Evidence

- `applyWeaponArms()` only overrides arms when `weaponKey != 0`.
- `writeActionState()` previously preferred `runtimeToolId` or a non-empty
  `equippedWeaponId`, even after the generic tool relationship was removed.
- The fix is in `src/render/presentation-entities.cpp`.
- `git diff --check` is required before installation.

## Boundary

The fix is in the cold presentation bridge, not the hot DLL manifest. It
requires the next intentional no-process bootstrap build before it can affect
the running executable. After installation, animation-axis and weapon-pose
changes remain live-editable in the hot animation files.
