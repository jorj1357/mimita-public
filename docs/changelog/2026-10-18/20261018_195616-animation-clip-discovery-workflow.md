# Animation clip include workflow and discovery follow-up

- Timestamp: 2026-10-18T19:56:16Z (2026-10-18 15:56:16 EDT)
- User-specified effective note time: 2026-10-18 15:55 EDT
- Branch: `20261011zt`
- HEAD: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
- Result: `PASS`

## Outcome

Documented the current requirement to include each Melee action clip in the active avatar's `animations.json`, a short missing-animation checklist that checks this profile link, and an explicit OPEN follow-up to discover clips automatically. The note does not claim automatic discovery exists or choose collision/actor-scope behavior for that future system.

## File change

### `docs/specs/animations/animations.md`, lines 124–150

- Before:

```markdown
- Exactly one animation format version number at the top of every file.

## `avatar.json` rig contract
```

- After:

```markdown
- Exactly one animation format version number at the top of every file.

### Melee action clip availability

**Current workflow — 2026-10-18 15:55 EDT (19:55 UTC):** A clip in
`config/animations/library/` is not automatically part of an avatar's loaded
animation set. Until automatic discovery is implemented, add each Melee action
clip file to the active avatar's `animations.json` `include` list. Keep the
move's `animation_clip_id` in `config/weapons.json` matched to the clip ID in
that included file.

When a Melee action has no visible animation in game, check these links in
order:

1. Find the active avatar's animation profile from `avatar.json` (or its
   default `animations.json` path).
2. Confirm that profile's `include` list contains the file that defines the
   action's `animation_clip_id`.
3. Confirm that included file defines that exact clip ID, with no duplicate
   clip ID in the shared library.
4. Confirm the Melee action in `config/weapons.json` references that same ID.
5. Check the `anim.melee.clip_missing` diagnostic and profile fallback if the
   clip still does not appear.

**Future follow-up — OPEN:** Make shared Melee clips discoverable by the
action's clip ID without manually adding every action clip to each avatar
profile. Define how discovery remains deterministic when clip IDs collide and
how actor-specific profiles restrict eligible clips before replacing the
explicit-include workflow. Automatic discovery is not implemented yet.

## `avatar.json` rig contract
```

## Validation and documentation review

- `git diff --check -- docs/specs/animations/animations.md` passed.
- `docs/skills/documentation-checker-v1.md`: the new note has clear scope and separates current behavior from the OPEN future follow-up. `docs/doc-review-09-03-2026.md` is absent in this checkout.
- The required whole-docs TODO scan found existing unrelated notes, which were not edited: `docs/specs/networking/networking.md:1088` (`Todo: put like server disagree effect where its like a sphere...`), `docs/specs/gamemodes/counterstrike.md:705` (`todo: crouching just does not exist right now...`), and `docs/specs/gamemodes/juggernaut-duel.md:226` (`Networking TODO: give role-based score-limit modes one consistent weapon-set...`). Each should be expanded into a desired behavior/acceptance statement at its owning spec before implementation.
- No code, config, runtime behavior, or automatic discovery implementation changed.
