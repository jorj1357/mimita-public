# Standard weapon-slot equip -> generic tool identity (Option A, staged)

Date: 2026-09-15 19:00 EST (UTC 2026-09-15T23:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; no live screen)

## SUBSYSTEM
Client presentation / equip identity - standard weapon-slot equip.

## STANDARD WEAPON-SLOT EQUIP BRIDGE
`WeaponSystem::equip`/`unequip` (`src/combat/weapon-system-equip.cpp`) now call
the generic `MimitaNet::actorStateEquipWeaponKey` / `actorStateUnequipTool`
(`src/network/actor-state.{h,cpp}`).

## CANONICAL TOOL IDENTITY
Actor --`equips-item`--> Tool Entity with `ToolRefState.toolKey =
gameHash(weaponId)`. The same EntityId is the identity for gameplay, presentation,
effects/audio, and inspection within the realm. No presentation-only object.

## TOOL ENTITY LIFETIME MODEL
One persistent generic tool entity per (actor, toolKey), created on first equip
and reused across switches; unequip only removes the `equips-item` edge. Entity
identity is never churned per frame.

## TYPED PLAYER MIRROR STATUS
`Player.equippedWeaponId`/`equippedSlot` remain populated as compatibility
mirrors/old fallback. Not removed.

## MIRROR DIRECTION / AUTHORITY
Authoritative action -> generic tool edge updated -> typed fields mirror it.
Single writer (`WeaponSystem::equip`). No two-way authority.

## UNEQUIP STATUS
Removes the `equips-item` edge; tool entity persists; hot claim drops to
`migrated == 0` and cold presentation owns again. Verified headlessly.

## SWITCH STATUS
swordsword -> revolver keeps exactly one equipped tool; the sword identity
persists unequipped. Verified headlessly.

## SPAWN/RESPAWN STATUS
Not yet routed through the bridge (spawn/default assignment paths not migrated
this pass). Recorded.

## JOIN/RECONNECT STATUS
Generic relationship/component replication already covers item/runtime-tool
equip; the client-local weapon bridge state is a projection of the local equip
and is reconstructed by the next `WeaponSystem::equip`. Remote-observer
counterpart not yet wired.

## CLIENT REPLICATION PROOF
Not exercised end-to-end this pass (no network in the headless harness);
`dynamic-replication-selftest` covers generic relationship replication.

## REAL SWORDSWORD NORMAL-EQUIP PATH
Source: `WeaponSystem::equip` -> `actorStateEquipWeaponKey` -> generic tool
identity -> `hot.tool-presentation` -> PresentationState/AttachmentState ->
claim -> `WeaponViewModel::render` yields. Headlessly exercised via the exact
bridge function; not screen-verified.

## REAL REVOLVER NORMAL-EQUIP PATH
Same substrate, one hot `g_bindings` entry (`mesh.tool.revolver`). Verified
headlessly.

## UNMIGRATED WEAPON FALLBACK
Shotgun: generic tool identity exists, but `hot.tool-presentation` yields
`migrated == 0` -> cold `WeaponViewModel` remains the owner. Generic equip
identity does not require presentation migration (= separate concerns).

## NEW ABI REQUIRED?
None. Uses Entity / dynamic components / relationships / ToolRefState /
replication only.

## DUPLICATE OWNER STATUS
No duplicate: `WeaponViewModel::render` yields on the hot claim; unpresented
weapons have no claim and cold owns. `hot.tool-presentation` runs in
`GAME_DOMAIN_POST_MOVEMENT` (before the cold render pass) so the claim is fresh
in the same frame (no one-frame double owner).

## SELFTESTS
`--hot-combat-selftest` PASS: standard weapon-slot bridge creates the generic
identity; real standard-equip swordsword reaches hot presentation; switch keeps
one equipped tool + old identity persists; unequip removes the edge with no stale
claim; revolver same substrate; ordering safety; unmigrated shotgun fallback;
local body one owner. Full suite PASS. `build_agent.py` -> SUCCESS.

## REAL SHIPPING-PATH PROOF
Exercises the exact bridge function `WeaponSystem::equip` calls (not a manual
`actorStateEquipTool` substitution). Network serialization -> client application
not exercised (harness limitation).

## LIVE PROOF
No (no interactive screen).

## TOOL-PRESENTATION CATEGORY PROVEN?
Nearly: real equip identity, real sword/revolver path, cold yield, unmigrated
fallback, switch/unequip cleanup, one local-body owner all verified headlessly on
the real shipping seam. Remaining to call it fully proven: interactive
screen confirmation and the server-authoritative counterpart for remote actors.

## NEXT LARGE OWNER
Nameplates/health overlays (after interactive confirmation).

## Files changed
`src/network/actor-state.h`, `src/network/actor-state.cpp`,
`src/combat/weapon-system-equip.cpp`,
`src/hot-reload/modules/presentation/attachment.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
