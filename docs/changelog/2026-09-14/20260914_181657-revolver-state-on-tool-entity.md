# Migrate the revolver's ammo/cooldown/reload to its tool entity component

- EST timestamp: 2026-09-14 18:16:57 EDT (UTC 2026-09-14T22:16:57Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--hot-combat-selftest` + full suite)

## Scope
Exactly one registered weapon: **revolver** (hitscan). No input or networking
redesign; the packet path is unchanged.

## What changed
- New `network/server-weapon-state.{h,cpp}`: component-authoritative weapon
  runtime state for migrated weapons. The state (`WeaponToolStateV1`:
  magazine/reserve ammo, `nextAllowedFireTick`, reloading/reloadCompleteTick/
  stateRevision/initialized) lives as a dynamic component on a per-weapon **tool
  entity** owned by the player, found through the generic `contains-item`
  relationship and a `WeaponToolId` component. `serverWeaponIsMigrated("revolver")`
  is the single switch; other weapons stay on the map.
- `serverWeaponStateLoad/Store` copy between the tool component and the legacy
  `ServerPlayer::ServerWeaponRuntime` **scratch view**. The map is overwritten
  from the component at every authoritative read and copied back after every
  authoritative write, so it is no longer the source of truth for the revolver.
- Wired at all three authoritative sites:
  - `handleAttackRequest` (`server-attack.cpp`): load before the cooldown check,
    store after the shot consumes ammo / sets cooldown.
  - `tickWeaponRuntimes` (`server-players.cpp`): load before, store after the
    reload timer advances.
  - `handleReloadRequest` (`server-packets.cpp`): load before client-ammo
    adoption, store after the reload state is decided.

## Evidence (`--hot-combat-selftest`, real DLL)
- revolver state tool entity created;
- revolver ammo/cooldown live on the tool entity **component** (6/12/100);
- `serverWeaponStateLoad` overwrites bogus legacy-map values (999/4242) with the
  component values -> **legacy weapon map is not authoritative (component wins)**;
- revolver tool **EntityId + state survive equip -> drop -> pickup -> re-equip**
  (same EntityId, ammo still 6);
- the runtime-tool action path is unaffected (all prior checks still pass).
- Full self-test suite PASS (13/13).

## Bridge that remains
- `ServerPlayer::weaponRuntimes` (string-keyed) is still the **per-call scratch
  view** for the revolver and the only store for every other weapon.
  `ownedWeaponIds`, `equippedSlot`, and the initial-inventory/weapon-set JSON
  still resolve registered weapons.
- `ServerPlayer::runtimeToolId` / `equippedToolEntity` and the global keyed tool
  router remain the runtime-tool bridges.
- Client-side `Player::weaponRuntimes` prediction/reload is unchanged; the
  client is not yet component-authoritative.
- Only the revolver is migrated; migrating the rest, removing the scratch map,
  and making the client read component state are the follow-ups.

## Files changed
`src/network/server-weapon-state.h` (new), `src/network/server-weapon-state.cpp`
(new), `src/network/server-attack.cpp`, `src/network/server-players.cpp`,
`src/network/server-packets.cpp`, `src/network/hot-combat-selftest.cpp`; docs +
this changelog.

## Next
Migrate the remaining registered weapons one at a time (same `serverWeaponIsMigrated`
switch), then remove the legacy scratch map and `runtimeToolId` bridges once all
call sites read component state; then generic dynamic-component replication.
