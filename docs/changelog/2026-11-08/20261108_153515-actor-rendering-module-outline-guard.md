# Hot actor rendering and outline bounds

Timestamp: 2026-11-08T15:35:15Z (10:35:15 EST)
Branch: `20261011zt`
HEAD at start: `b12077119c82f6bf75b586284f822855b8ab065e`

## Scope

Moved selected actor rendering decisions to a reloadable DLL and used the
Payload client journal to address outlines that fill the view. Many unrelated
dirty files and concurrent dev-loop changes were present; they were preserved.

## Source changes

- Added the `actor-rendering` catalog entry, event IDs, plain-data API, and
  module implementation. The module selects part pose transforms, measures
  local mesh batch bounds, decides part visibility and combined-mesh eligibility,
  orders overlay layers, and rejects implausible or camera-inside outlines.
- Routed the player/NPC render path through synchronous module events with a
  host fallback. The EXE retains `Player` state, actor/world-bounds assembly,
  team/config selection, mesh and GPU lifetime, material and GL submission.
  Borrowed mesh positions are used only during the bounds call; scratch part
  arrays are reused.
- Added actor/mesh bounds and generation fields to bounded diagnostics. Kept
  `outline_debug` as the existing command adapter. Set Rendering logging to
  `important` after the investigation to avoid verbose per-frame noise.
- Updated the hot-reload specification and hot-kernel architecture inventory
  with current ownership and remaining host policy.

## Build and hot-reload evidence

- The initial bridge built through the dev loop at
  `2026-11-08T14:54:49.382732Z`. The final bridge built at
  `2026-11-08T15:24:01.772036Z:a977953cee573e2abdeee7a2275d9cc50e6d7b68d61ba98776df26c8f49eeed8`.
  That build reported unchanged source between build start and publish.
- A separate build attempt failed before compilation because the shared build
  lock could not be read; the dev-loop retry succeeded. Build success proves
  compilation and linking only.
- In run `20261108_152452`, client PID 19124 activated module generation 7,
  then a module-only source change activated generation 8 at
  `2026-11-08T15:26:39.875Z`, without an EXE build-identity or PID change.
  An intentional bad DLL candidate failed as generation 9 at
  `15:27:33.220Z`; generation 8 stayed active. The bad source was removed.
  The later EXE build was triggered amid concurrent catalog/config changes;
  `-module.cpp` is excluded from EXE source and watcher inputs.

## Runtime evidence and limits

- `logs/2026-11-08/20261108_152452/client-01/events.jsonl` records actor ID,
  avatar, current position, camera distance, per-part origins, mesh bounds,
  previous-pose displacement, interpolation choice, outline decisions, and
  module generation. Sampled actor meshes had complete bounds. The module
  rejected large previous-pose jumps rather than interpolating them.
- In that run, 16 enemy and 16 friendly outline state transitions were drawn;
  three enemy and five friendly transitions were skipped because the camera
  entered measured actor bounds. The skip event records actor/team, distance,
  reason, and module generation. An early invalid-bounds skip occurred while
  an actor model was not loaded.
- Run `20261108_152945`, build
  `2026-11-08T15:28:28.157458Z:61ad074db97b090b224e6f70cae9443697b9ddd997a20aafc4851c6bbcd79d28`,
  client PID 29288, activated generation 8. With `outline_debug 1`, 506 actor
  summaries included both red and green actors, all with zero missing mesh
  bounds. This included a 19-extra-part `monstergriever` and a large
  `monsterchainsawbossv2`.
- The original giant silhouette was not visually reproduced at the same camera
  pose after the change. The camera-inside guard is a supported narrow fix for
  one observed giant-outline condition, not proof that every possible cause is
  gone. Human visual acceptance remains open. Further host rendering policy
  remains outside this first hot module boundary.

## Review

`git diff --check` passed for the touched tracked files. No synthetic gameplay
test was used as runtime proof. No regression was confirmed.
