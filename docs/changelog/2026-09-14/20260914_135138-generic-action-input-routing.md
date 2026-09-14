# Generic action input routing to the equipped runtime tool

- EST timestamp: 2026-09-14 13:51:38 EDT (UTC 2026-09-14T17:51:38Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--hot-combat-selftest` + full suite)

## What this removes
The input/registration cold owner: primary fire could only reach a runtime tool
if it was a registered `NETWORK_WEAPON_*`/`WeaponRegistry` weapon. Now a tool
created after startup can be equipped and used from real client primary input
with **no registered network weapon id**.

## Design (generic, no per-tool packet/callback)
- `FireIntentPacket` gained a generic `uint64_t toolId` (runtime hash). `0` =
  legacy weapon path; non-zero = generic runtime tool.
- Client `equiptool <name>` sets `Player::runtimeToolId`; primary mouse now
  sends the held-fire intent with that tool id (origin/direction included) when a
  runtime tool is equipped. The STOP packet carries the same id.
- Server `handleFireIntentPacket` stores `held.toolId` and, on first use,
  lazily creates/links the equipped tool **entity** (generic
  `EntityRegistry::createGeneric`) and records ownership with a
  `relationship.owns-tool` edge plus an `EquippedTool` dynamic component. No
  registry definition is required.
- `tickHeldFireIntents` gains a generic branch: when `held.toolId != 0` it
  resolves `userEntity`/`equippedToolEntity` and dispatches `ToolUsePolicyV1`
  (origin/direction) via `LiveBehavior::dispatchToolUse`, bypassing all
  weapon-registry/`NETWORK_WEAPON_*` lookups. The per-entity
  `BehaviorBindingsComponent` path fires when the tool has bindings; otherwise
  the runtime behavior table is used.
- `serverEquipRuntimeTool(...)` exposed for an explicit equip/containment path.

## Success-bar evidence
`--hot-combat-selftest` proves, in one process with the real DLL:
- a **runtime tool** (hash id, no network weapon) is equipped from a real
  `FireIntentPacket` action intent;
- the authoritative held-fire tick **dispatches the generic action** and the
  hot behavior spawns an authoritative projectile (`HotProjectileState` entity);
- the real rocket and grenade use the canonical hot projectile path;
- per-entity behavior binding owns the tool use;
- `damage.apply` still applies real authoritative damage;
- unregistered tools/projectiles fall back to the cold path.

Full self-test suite PASS (13/13).

## Files changed
`src/network/packets.h` (FireIntentPacket.toolId), `src/network/server.h`
(`HeldFireState.toolId`, `ServerPlayer.runtimeToolId`/`equippedToolEntity`,
`serverEquipRuntimeTool` decl), `src/network/server-attack.cpp` (generic
held-fire branch + lazy equip + `serverEquipRuntimeTool`),
`src/network/server-packet-chat.cpp` (`equiptool` server command),
`src/network/multiplayer-packets.cpp` + `multiplayer-context.h`
(`mpSendFireIntent` toolId), `src/engine/engine-tick-combat.cpp` (send runtime
tool intent), `src/entities/player.h` (`runtimeToolId`),
`src/terminal/weapon-commands.cpp` (`equiptool`), `src/network/hot-combat-selftest.cpp`.

## Honest limitations
- The client still needs a small terminal step (`equiptool <name>`) to select a
  runtime tool; a generic inventory/equip UI is not done.
- The `RuntimeTool/WeaponRegistry` string maps still exist for legacy weapons;
  ammo/reload/cooldown have not moved to dynamic components yet.
- `damage.apply` is player-victim only (NPC boundary pending).
- No live single-process or visual acceptance; tree co-edited by the gameplay
  agent (their canonical hot projectile path is integrated and green).

## Next
- NPC damage boundary; inventory/equip as relations + item state; dynamic
  component replication; hot projectile simulation for remaining weapons.
