# Clarify editable animation-owned Melee timing

- Timestamp: 2026-10-18T19:02:00Z (2026-10-18 15:02 EDT)
- Result: `SPEC_UPDATED_IMPLEMENTATION_GAP_REMAINS`

## Request and result

Clarified that the clip JSON owns Melee action duration and phase boundaries,
that its marker ticks can be edited after export, and that `weapons.json` does
not provide a fallback phase timeline. Added plain tick-boundary guidance so
authors can tell phase lengths from marker start positions. The existing
zero-based tick convention and exclusive end marker remain authoritative.

## Changed specifications

### `docs/specs/animations/animations.md`

- Before: markers were documented as starts of half-open phase intervals and
  `action.end` was exclusive, but the meaning of each phase label and direct
  post-export JSON editing were not stated in those terms.
- After: lines 254-267 require windup to begin at tick 0, explain how later
  markers set active/recovery starts, and show how changing marker values and
  `durationTicks` changes the phase lengths. Lines 365-373 state clip timing is
  the required/default behavior and invalid clips cannot fall back to weapon
  timing.
- The existing contract remains: a 60-tick clip samples ticks 0–59 and has
  `action.end` at tick 60. Tick 59 is its final sampled pose, not its exclusive
  endpoint.

### `docs/specs/animations/blender-animation-authoring.md`

- Before: marker workflow required four labels but did not plainly say they
  apply once to the whole clip or explain post-export edits.
- After: lines 208-214 specify scene-level shared markers (not per-limb or
  separate phase clips); lines 341-345 state exported JSON marker edits drive
  runtime, require `durationTicks` to match `action.end`, and explain that a
  later Blender re-export writes the Blender timeline marker positions again.

## Spec/code comparison

- The routed runtime specification is `docs/specs/animations/animations.md`;
  `docs/specs/weapons/weapons.md` describes the action's clip reference and
  non-timing combat data; `docs/specs/animations/blender-animation-authoring.md`
  owns the Blender marker-export workflow.
- Current parser `src/combat/weapon-melee-config.cpp::parseAction` still reads
  `phases` from `weapons.json`; `src/combat/melee-client.cpp` and
  `src/network/server-melee-actions.cpp` still derive runtime phases from those
  values. This is a spec-code disagreement, not an implemented feature.
- A raw unrelated TODO remains at `docs/specs/weapons/weapons.md:135`:
  `todo expand this to includ grenades and explosives...`. It was not edited;
  its scope is broader weapon support, not this melee timing contract. Suggested
  follow-up: replace it with a scoped weapon-family and projectile-conversion
  requirements section.

## Validation

- Reviewed the animation, weapons, Melee mode, and Blender authoring contracts
  and the parser/client/server phase owners.
- `git diff --check` passed. No build or runtime test was run because this turn
  changed specifications only.
