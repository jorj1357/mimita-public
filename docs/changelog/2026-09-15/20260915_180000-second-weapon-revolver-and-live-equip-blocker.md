# Second weapon (revolver) on the same substrate + live-equip blocker traced

Date: 2026-09-15 18:00 EST (UTC 2026-09-15T22:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (migration verified headlessly; live equip BLOCKED)

## SUBSYSTEM
Client presentation - tool-presentation substrate, second weapon.

## LIVE EQUIP RELATIONSHIP PROOF
Not available. Traced the real path and found the exact blocker (below).

## LOCAL ACTOR ENTITY ID
`Ecs::ensureLocalPlayerEntity()`; published to hot code via
`GameSharedStateV1.localPlayerEntity` (set in `sim/simulate-tick.cpp:109`).

## REAL EQUIPPED TOOL ENTITY ID
Does NOT exist for standard weapon-slot equip. Created only for items/runtime
tools: `serverItemEquip`/`serverEquipRuntimeTool`
(`src/network/server-attack.cpp:1482,1540`) and runtime-tool use with
`toolId != 0` (`:1285`).

## CLIENT REPLICATION PATH
Generic and working: `dynamic-replication.cpp` replicates dynamic components and
`RelationshipStore` edges (including `equips-item`) to the client. Replication is
not the blocker; creation of the state is.

## TOOL MATERIALIZATION ORDER SAFETY
Handled: an `equips-item` edge without `ToolRefState` yields no claim and no
presentation; a missing parent/socket hides; no crash. Verified by selftest.

## SWORDSWORD LIVE FIRST-PERSON PROOF
No (no live screen).

## SWORDSWORD LIVE THIRD-PERSON PROOF
No (no live screen).

## HOT CPP EDIT PROOF
No (no live screen). Meshes/offsets/context are hot policy data
(`attachment.cpp` bindings + attachment fields) and are re-read each frame.

## BAD-EDIT LAST-GOOD PROOF
No live screen. The external `devscripts/live-build.py` last-good mechanism is
unchanged; the hot DLL built successfully this pass.

## RESOURCE HOT-SWAP PROOF
No live screen. Provider generation swap + last-good already covered headlessly.

## OWNER INVARIANT
For migrated tools: hot tool-presentation + attachment is the single owner
(claim written; `WeaponViewModel::render` yields on the claim). Unmigrated tools:
no claim, cold owner. No hot+cold duplicate in either case.

## WEAPON SWITCH FALLBACK
Substrate-level: `claim.migrated` follows the currently equipped tool key; an
unmigrated key (shotgun) produces `migrated == 0` and cold fallback. Full live
switch test not possible without the equip bridge.

## LIFECYCLE STATUS
Not live-tested; the generic design fails safe (missing state -> no claim/no
draw). Death/respawn tool-claim cleanup depends on the equip-authority fix.

## SECOND WEAPON MIGRATED?
Yes - revolver (tool key 1) on the same substrate, one hot `g_bindings` entry
(`mesh.tool.revolver` -> `mimita-revolver-v1.glb`). Selftest proves the real
revolver carries its logical mesh + claim.

## SECOND WEAPON NEW ABI REQUIRED?
None. Only hot policy data changed. (Falsification test satisfied.)

## TOOL-PRESENTATION CATEGORY PROVEN?
No. Gate items 1-3 (real swordsword equip, real first/third-person) are not met
because the standard weapon equip path does not create the generic tool entity /
`equips-item` relationship.

## EXACT BLOCKER
`WeaponSystem::equip`/`unequip` (`src/combat/weapon-system-equip.cpp:84-170`)
mutate only `Player.equippedWeaponId`/`equippedSlot`; no generic tool entity or
`equips-item` edge is created for weapon-slot weapons. Options:
(A) route weapon-slot equip through the existing generic tool entity +
`equips-item` (true convergence; touches equip authority), or
(B) a client presentation projection for the local equipped weapon (analogous to
`PresentationEntities::projectLocalPlayer`).
Neither is unilaterally implemented here: (A) materially changes gameplay/equip
authority and neither is verifiable live in this environment.

## NEXT LARGE OWNER
Do NOT proceed to nameplates/overlays until the tool-presentation gate passes.
Decision needed on (A) vs (B).

## Files changed
`src/hot-reload/modules/presentation/attachment.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
