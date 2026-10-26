# Payload cart and route-order command intake

- **Time:** 2026-10-26T22:46:47Z (18:46:47 EDT, America/New_York).
- **Branch:** `20261011zt`.
- **HEAD at session:** `c393337d` — `reporetdly working image decal shared shader state`.
- **Result:** PASS_WITH_HUMAN_REVIEW. Added the requested command usability intake; no implementation or runtime behavior was changed or verified.

## Change

Updated `docs/specs/open-gameplay-issues.md` under “Cart placement question”
(new “Requested Payload authoring commands” section at line 622).

The existing paragraph said the editor should make the cart-origin/route-start
relationship easier to author and left automatic placement undecided. Added
the player's two command requests:

> The player wants a direct command to snap the cart origin to route point 1's
> position, and a command that assigns a selected route point an exact route
> number without moving the point in the world.

The entry also records the ten-point `..., 8, 10, 9` example, notes that current
docs already list `mapeditor cart_origin_to_route_start` and
`mapeditor route_move up|down`, and specifies the open behavior questions:
whether snapping targets array entry 1 or the first enabled point, and that
absolute reorder should move order while preserving each point's world
position and metadata. It proposes `mapeditor route_order <number>` only as a
design example, not as an implemented command.

## Review and validation

- Inspected `docs/specs/open-gameplay-issues.md` and
  `docs/specs/entity-editor/entity-editor.md`. The latter already documents
  the cart-origin alignment and one-step route reordering commands.
- The issue index was already modified before this session, alongside
  unrelated source/config edits and untracked assets, evidence, and changelogs.
  Those were preserved; this session added only the intake paragraph and this
  changelog.
- Focused documentation review: PASS. The intake distinguishes existing
  commands from the requested exact-order command and records the unresolved
  route-1 selection semantics. No automated test or build was run for this
  documentation-only change.
- Runtime reproduction and human review remain open.
