# Generic socket/attachment + arbitrary logical mesh resources + world/view space

Date: 2026-09-15 16:00 EST (UTC 2026-09-15T20:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PARTIAL_WITH_HUMAN_REVIEW` (primitives done; real swordsword equip migration pending)

## SUBSYSTEM
Client presentation - generic attachment/presentation substrate.

## GENERIC ABI PRIMITIVES ADDED
- `GAME_CAP_SOCKET_QUERY` / `GameSocketQueryV1` (`socket.query`).
- `HOT_ATTACHMENT_COMPONENT` / `HotAttachmentStateV1` (generic presentation
  attachment; hot schema, not a `GameplayContextV1` field).
- `GAME_CAP_RESOURCE_REGISTER` / `GameResourceRegisterV1` / `GameResourceKind`.
- `GAME_RENDER_MESH_SPACE_VIEW` (flags bit on `GameRenderMeshCommandV1`).

## WHY EACH PRIMITIVE IS GENERIC
- socket.query: "world transform of named point X on entity Y" - works for
  skeletal actors, props, tools; no weapon/hand/muzzle-specific query.
- AttachmentState: parent entity + socket hash + local TRS + presentation
  context; usable by held tools, hats, carried props, muzzle points, bone
  particles, child entities.
- resource.register: logical id + kind + path; reusable for meshes/textures of
  any feature; no per-tool resource slot.
- view space: one generic world/view flag; the cold renderer only knows
  "camera-relative transform", never "swordsword viewmodel".

## RESOURCE PROVIDER CHANGES
`PresentationRender::registerLogicalResource` registers arbitrary hot logical
mesh/texture ids in the existing generation-aware provider; `poll()` re-applies
file-backed dynamic resources so a generation can swap while entities keep their
EntityId. Malformed GLB preserves last-good (rejected, no bad generation).
`submitMesh` records EntityId -> logical mesh id (a hash) and supports view space.

## ATTACHMENT AUTHORITY MODEL
Attachment is a PRESENTATION override. `hot.attachment` (RENDER, order 4)
resolves each `AttachmentState` through `socket.query` and stores the world
transform in the component's out fields; the child's authoritative `Transform`
is never overwritten. `hot.presentation-mesh` consumes the resolved transform;
an unresolved attachment hides the entity (fail safe). Missing parent/socket
yields `valid=0`.

## RUNTIME-UNKNOWN MESH PROOF
`hotmesh <glb path>` registers an arbitrary logical mesh id; `resource.register`
+ provider load + `submitMesh` resolve it. No enum/switch.

## RUNTIME-UNKNOWN TOOL PROOF
`hottool [path]` creates a tool entity with an arbitrary logical mesh +
`AttachmentState` to a parent `rightArm` socket (world/third-person);
`hottool1p [path]` uses the view/first-person context. No weapon enum, no new
game-api field, no dedicated renderer.

## REAL THIRD-PERSON TOOL MIGRATED
No - see remaining work. The runtime-unknown tool uses the real swordsword GLB
(`assets/objects/weapons/mimita-hafs-v1.glb`) through logical ids, but it is not
yet driven by the real equip flow.

## REAL FIRST-PERSON TOOL MIGRATED
No - same.

## COLD OWNER REMOVED
None for real weapons: cold `WeaponViewModel` remains the single owner (no
duplicate owner introduced). The new mechanisms are additive.

## COLD MECHANISM REMAINING
GPU draw, GLB parse, skeleton/socket composition, view/projection, depth.

## COMPATIBILITY FALLBACK
Cold viewmodel/remote weapon draw is unchanged. Unresolved attachments hide; the
generic presentation path is used only by entities carrying `PresentationState`.

## SELFTEST PROVEN
`--hot-combat-selftest` PASS: "socket.query capability resolves",
"resource.register capability resolves", "socket query falls back to entity
transform + local offset", "socket query on missing entity fails safe",
"runtime-unknown logical mesh resource registers", "malformed mesh keeps no bad
generation", "attachment resolves a socket world transform (fail-safe)",
"first-person tool uses the generic view-space context". Full suite PASS.
`build_agent.py` -> SUCCESS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## CONCURRENCY BOUNDARY STATUS
Clean - no movement/network/reconciliation files touched; actor pose consumed
read-only.

## REMAINING WORK (next cold owner)
Real `swordsword` migration needs (a) a generic tool->mesh data mapping
(resource manifest extension, per the no-per-weapon-registration rule) and (b) a
client possessed-tool bridge so the equipped tool identity selects the logical
mesh. Both are deliberate data/ownership decisions, not new mechanisms.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
Attachment/policy bugs: **no** (hot). Resource-registration, socket mechanism,
view-space, GLB/skeleton/GPU bugs: yes (cold mechanism). Real weapon presentation
bugs: yes until migrated.

## Files changed
`src/hot-reload/game-api.h`, `src/render/presentation-render.h`,
`src/render/presentation-render.cpp`, `src/live-code/live-behavior.cpp`,
`src/hot-reload/hot-presentation.h`,
`src/hot-reload/modules/presentation/attachment.cpp` (new),
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
