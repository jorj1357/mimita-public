# Real equip flow on the generic substrate + local body single owner

Date: 2026-09-15 17:00 EST (UTC 2026-09-15T21:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; interactive equip/yield not screen-verified)

## SUBSYSTEM
Client presentation - equipped-tool presentation via the generic substrate.

## LOCAL PLAYER DUPLICATE OWNER RESOLUTION
`hot.presentation-mesh` now skips the local possessed actor
(`GameSharedStateV1.localPlayerEntity`), so the cold body mechanism
(hot pose -> SkeletonInstances -> cold body draw) is the single local-body
owner. Tools/attachments/effects/arbitrary entities on other EntityIds still
draw generically. Regression test proves the actor's own mesh is submitted
exactly once.

## REAL EQUIP IDENTITY PATH
Actor --`relationship.equips-item`--> tool Entity with `ToolRefState.toolKey`;
written by the existing cold generic API `actorStateEquipTool`, read by
`actorStateGetEquippedTool`. No new ABI, no weapon enum.

## GENERIC POSSESSED TOOL BRIDGE
Hot `hot.tool-presentation` (RENDER, order 3) reads the local actor's equipped
tool EntityId via `relationshipQuery(equips-item)` + `ToolRefState`, and writes
`PresentationState`/`AttachmentState` onto the REAL tool EntityId. No
`weapon.queryEquipped`, no new context field.

## TOOL PRESENTATION DATA MODEL
The tool entity itself carries `PresentationState.meshResourceId` (logical id) +
`AttachmentState{parent=actor, socket=rightArm, local TRS, context}`. Hot C++ maps
tool key -> logical mesh (`g_bindings`), no kernel database and no JSON
requirement.

## RESOURCE MAPPING MODEL
Resource layer: logical id -> path -> content hash -> generation -> mesh.
Presentation layer: tool entity -> `PresentationState.meshResourceId`. Gameplay/
presentation policy chooses the logical id.

## REAL THIRD-PERSON SWORDSWORD MIGRATED
Source implemented: an actor equipped with tool key 4 gets
`mesh.tool.swordsword` attached at the `rightArm` socket in WORLD context. The
cold `weapon-swordsword.cpp::render` is already a no-op.

## REAL FIRST-PERSON SWORDSWORD MIGRATED
Source implemented: the local possessed actor's equipped tool uses the VIEW
context (`GAME_RENDER_MESH_SPACE_VIEW`).

## COLD WeaponViewModel OWNER STATUS
Fallback, yielding. `WeaponViewModel::render` returns early when the local actor
has a `ToolPresentationClaim` matching the equipped tool key. Unmigrated weapons
have `migrated == 0` and keep the cold path.

## RUNTIME-UNKNOWN TOOL REAL-PATH PROOF
`hottool` only creates the entity + `equips-item` + `ToolRefState`; presentation
then flows through `hot.tool-presentation -> hot.attachment -> render.mesh`
(same systems as a real tool). No debug-only rendering shortcut.

## RESOURCE HOT-SWAP PROOF
Not screen-proven this pass: the provider generation mechanism is unchanged and
already covered ("changed content hash swaps generation", "failed load preserves
last-good"); the tool EntityId is proven preserved across presentation changes.

## ATTACHMENT HOT-EDIT PROOF
Not screen-proven (no live screen). Attachment fields are hot and re-resolved
each frame; tool EntityId identity is proven preserved.

## SELFTEST PROVEN
`--hot-combat-selftest` PASS: local possessed body submitted exactly once;
runtime-unknown tool via `equips-item`; tool carries its logical mesh;
attachment resolves; hot tool claim declares one owner; view-space context; real
equip API resolves the tool EntityId; real swordsword carries
`mesh.tool.swordsword`; sword attachment resolves; tool EntityId preserved across
presentation changes; unmigrated weapon falls back cold. Full suite PASS.
`build_agent.py` -> SUCCESS.

## LIVE HOT-EDIT PROVEN
No (no screen/live session available).

## CONCURRENCY BOUNDARY STATUS
Clean - no movement/network/reconciliation files touched; generic equip state
consumed read-only.

## HONEST LIMITATION
The "real equip" evidence is headless through the real generic equip API, not an
interactive equip. Whether the live client populates `equips-item` for the local
actor (so the cold viewmodel actually yields) is not screen-verified; until then
the cold path is the safe fallback (no regression, no duplicate owner).

## WOULD SWORDSWORD PRESENTATION STILL REQUIRE COLD RESTART?
Mesh choice, world/first-person attachment, offsets, visibility, swing policy:
**no** (hot tool-presentation + hot C++ data). GLB parser, socket.query
implementation, GPU/view-space renderer: yes (cold mechanism by design).

## WOULD ANOTHER WEAPON REQUIRE NEW ABI?
No. Another weapon needs only a hot `g_bindings` entry (tool key -> logical mesh)
+ attachment/action/effect/audio policy. The substrate is generic.

## Files changed
`src/hot-reload/hot-presentation.h`,
`src/hot-reload/modules/presentation/attachment.cpp`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/combat/weapon-viewmodel.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
