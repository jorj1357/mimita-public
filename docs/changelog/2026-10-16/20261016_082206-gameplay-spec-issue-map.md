# Gameplay specifications and issue notes — 2026-10-16

- Timestamp: 2026-10-16T08:22:06Z
- Result: documentation updated; runtime issues remain unverified

## Goal

Map the user's gameplay observations to existing specifications, clarify missing
requirements, and leave implementation decisions open where intent is not settled.

## Documents reviewed

- `docs/ROUTER.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/spec-behavior-review-v1.md`
- Existing melee, weapon, animation, avatar, movement, networking, ragdoll,
  collision/physical-object, entity-editor, and NPC navigation specifications.
- `config/weapons.json` to confirm the configured weapon ID `hafs` and display
  name `HAFS`.

The router references `docs/doc-review-09-03-2026.md`, which is missing from
this checkout. No code was changed, no tests/build were run, and no gameplay
behavior was claimed as reproduced or fixed.

## Changes

- Added `docs/specs/open-gameplay-issues-2026-10-16.md` as a one-page index with
  statuses and links to current owners.
- Clarified the SSBM-inspired 3D arena-mode proposal and its unresolved choices
  in `docs/specs/gamemodes/supersmashbrosmelee.md`.
- Added a proposed Blender authoring/export contract in
  `docs/specs/animations/blender-animation-authoring.md` and linked it from the
  shared animation spec. Kept Blender conversion in the current animation
  system; armature support, sampling, root motion, and tooling are open choices.
- Added focused user-report notes for Spy Knife PvP, SwordSword, Godball/HAFS,
  Sandbox spawn points, ragdoll/limb collision, Juggernaut4 weapon collisions,
  hospital multi-floor NPC pursuit, and large client/server movement errors.
- Explicitly preserved server authority, world collision, fixed-tick behavior,
  existing Godball world pass-through, and exact-triangle constraints; no blind
  client-position acceptance, collider replacement, or bevel implementation
  was specified.

## Documentation check and open items

The full documentation scan found these actionable existing TODO notes; they
were not broadly rewritten as part of this task:

- `docs/specs/gamemodes/supersmashbrosmelee.md:1` — `todo explainbetter`.
  Suggested replacement: retain the now-added clarified proposal and remove the
  marker once its open decisions have been reviewed.
- `docs/specs/avatar/avatar-editor.md:5` — `also todo clean this doc!!!!!! cant have so much stuff inthis doc just spam`.
  Suggested replacement: split the long editor/normalization details into
  focused linked specifications and keep a short overview here.
- `docs/specs/visuals/visuals.md:1` — `todo explain this  more like`.
  Suggested replacement: state which visual behavior is unclear and link to the
  feature owner and desired in-game example.
- `docs/specs/gamemodes/gamemodes.md:191` — `Todo explain this 9 2 2026 0848`.
  Suggested replacement: explain the exact gamemode rule referenced by that
  bullet or link to its mode specification.
- `docs/specs/moving-physical-objects/moving-physical-objects.md:1` — `todo - explain moving phsical objects`.
  Suggested replacement: the doc has a current physical-objects overview; update
  the marker to identify any remaining specific unclear behavior.
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:1` — `todo explain ragdoll like retrograd`.
  Suggested replacement: define the specific desired Retrograd behavior or
  remove the stale marker after review.
- `docs/specs/colliisons/collisions.md:1` — `todo - explain`.
  Suggested replacement: state the collision behavior being specified and its
  boundaries; preserve the existing exact-triangle requirement.

Other TODOs in gamemodes/avatar docs were not all “explain/clean/add an
explanation” markers; they remain separately scoped. The older free-form
notes stay historical context. The new issue index, not those shorthand lines,
provides the current review list.

## Evidence status

- Spec/source inventory: complete for the paths listed above.
- Documentation edits: present in the working tree.
- Link/build/test/runtime validation: not performed; this was a documentation
  mapping task and AGENTS prohibits treating a build as runtime proof.
- Human review: required for open SSBM rules, HAFS expected behavior, collision
  shape, PvP policy, spawn selection, and correction behavior.
