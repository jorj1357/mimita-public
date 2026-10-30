# Climbable and transparent surface design intake

- Canonical timestamp: `2026-10-30T03:29:00Z`.
- Display time: `2026-10-29 23:29 EDT` (`America/New_York`).
- Branch: `20261011zt`.
- Base commit: `1b5f3bac`.
- Result: `PASS_WITH_HUMAN_REVIEW` for documentation intake; runtime changes
  were not authorized or made.

## Change

Added a dated intake section to
`docs/specs/open-gameplay-issues.md:59-96`, titled “New intake — 2026-10-29:
authored climbable, pass-through, and transparent surfaces,” and a corresponding
clearly labeled design-intake section in
`docs/specs/entity-editor/entity-editor.md:299-320` under “Authored blocks.”

At the tracker insertion point, the exact surrounding text was `required after
implementation.` followed by `2026 10 29 — regression: Juggernaut Fighter
revolver RMB aim points backward`. The inserted heading is `## New intake —
2026-10-29: authored climbable, pass-through, and transparent surfaces`. It
records the request for an 89-degree
walk-up surface, possible 90-degree-or-greater climbing, object pass-through,
and alpha from `1.0` opaque to `0.0` transparent. In the Entity Editor spec, the
prior text at the insertion point ended with `Arbitrary baked GLB triangles
are not individually editable in v1.` and was followed by `### Payload route
authoring`. The inserted heading is `### Authored surface behavior — design
intake, not implemented`. Both entries keep gameplay collision and visual alpha
independent, call out the existing editor wireframe-only alpha, route future
behavior specification through movement/collision rules, and record unresolved
ownership and acceptance questions. The tracker entry preserves client/server
collision parity, fixed-tick collision, and cached broadphase requirements. No
implementation decision was made.

## Source review

- `docs/specs/entity-editor/entity-editor.md`, “Authored blocks” and “GLB
  object overrides”: prior to this edit, the editor documented static box
  blocks and position-only GLB overrides, but did not specify per-surface
  climbing, pass-through, or material alpha. The new subsection marks these as
  desired editor capabilities and keeps runtime behavior explicitly
  unimplemented.
- `docs/specs/entity-editor/entity-editor.md`, “Block wireframes”: `block_wire_alpha`
  controls editor wireframe presentation only.
- `docs/specs/movement/movement.md`: global step-up remains enabled with an
  initial maximum step height of `0.25f`; no per-object steep-surface override
  is described.
- `docs/architecture/collision/collision.md`: collision follows fixed-tick
  rules, uses cached broadphase gathering, and requires matching client/server
  semantics.
- The previously modified working tree included extensive unrelated user
  edits, including existing changes to the open-issues tracker and entity
  editor spec. They were preserved. This entry does not claim those earlier
  changes.

## Focused review and validation

- Read `docs/ROUTER.md`, the routed movement, collision, and Entity Editor
  documents, `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- `docs/doc-review-09-03-2026.md`, listed by the router, is absent from this
  checkout.
- Specification review: `PASS_WITH_HUMAN_REVIEW`. The editor and movement
  documents provide adjacent concepts but no contract for the requested
  per-face/object properties. Exact semantics for vertical/overhanging climb,
  supported authorable kinds, actor categories, and alpha/material rendering
  remain design decisions.
- Documentation check: `PASS_WITH_HUMAN_REVIEW`. The new links point to
  existing routed docs; the issue is explicitly marked as intake and does not
  conflict with global movement/collision requirements.
- `git diff --check` for both changed specs passed; only the repository's
  LF-to-CRLF working-copy warning appeared.
- Router TODO scan found `docs/architecture/player-npc-systems/npc-movement.md:22`:
  “9 8 2026 1019 est jorj todo - explain”. Suggested expansion: document the
  current NPC movement inputs and behavior, the desired advanced jukes or
  recorded-human-movement reuse, the current limitations, and measurable
  scenarios for evaluating those approaches. This unrelated note was left
  unchanged.
- No code, configuration, tests, build, runtime scenario, or human gameplay
  review was performed; those are not evidence for this documentation-only
  intake.

## Human/design review still needed

Before implementation, select whether properties attach per object, per face,
or both; define climbing on vertical and overhanging faces; choose which actors
and gameplay objects can pass through; define alpha/material behavior; and
review the proposed acceptance cases in the issue entry. The intended owner is
the Map Entity Editor specification, linked to movement and collision rules.
