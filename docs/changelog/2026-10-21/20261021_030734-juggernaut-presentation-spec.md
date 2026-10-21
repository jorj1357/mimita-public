# Juggernaut presentation spec update

- Status: `PASS_WITH_HUMAN_REVIEW`
- Date and time: 2026-10-21T03:07:34Z (2026-10-20 23:07:34 EDT)
- Branch: `20261011zt`
- Base commit: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Final commit: unchanged; no commit created.

## Pre-existing changes

The worktree already contained unrelated edits in configuration, code, and
other specifications. `docs/specs/gamemodes/juggernaut.md` also had the
following user-added note block before this session; it was replaced with a
structured specification addendum:

```text
2026 10 20 2304

force avatars to onl be able to use  liek girl bod or male bod, like jason body or  abusivegirl body cant use any other avatars
juggernauts team can use like big avatars liek brute probably all of them 

disabled damage nmers disable friendly outlines
didsable disable all thigns but the blood and bullet holes 
```

All other pre-existing work was left untouched.

While this edit was in progress, a separate unfinished note appeared at line 1
of the specification: `2026 10 20 2308 like u shoudl bea ble to do it all thru the `.
It was preserved without interpretation. The phrase ends mid-sentence and
needs the author's completion before it can be incorporated as a requirement.

## Specification change

In `docs/specs/gamemodes/juggernaut.md:2-67`, added “Gameplay and presentation
addendum (2026-10-20)” with separately labeled design targets, proposals, and
`NEEDS_SPEC_DECISION` questions. It captures role-appropriate avatar groups,
enemy-only red outlines, hidden damage numbers, retained blood and bullet-hole
impacts, avatar/collision fit, distinct heavy-shotgun audio, and immediate
local weapon feedback while preserving server authority. Avatar examples are
explicitly non-final. Capsule sizing, full presentation switch list, exact
avatar allowlists, and sound asset remain unresolved.

The addendum also states that the invisible-actor and no-health-loss reports
are separate runtime investigations, not proven fixed by a documentation
change.

## Documents and review

- Read `docs/ROUTER.md` and the available routed specification and workflow
  references for gamemodes, graphics presentation, actor presets, avatar
  normalization, and task completion.
- Applied `docs/skills/spec-behavior-review-v1.md`: preserved informal notes as
  bounded design targets and left underspecified choices open.
- Applied `docs/skills/documentation-checker-v1.md`: the routed
  `docs/doc-review-09-03-2026.md` path does not exist in this checkout. The
  focused `docs/` search found only the router and skill's own instructional
  TODO examples, not an actionable document TODO.
- Related contracts read: `docs/specs/graphics/graphics-presentation.md`,
  `docs/specs/actor-presets/actor-presets.md`,
  `docs/specs/avatar/avatar-editor.md`, and
  `docs/specs/gamemodes/gamemodes.md`.

## Validation and acceptance

- `git diff --check -- docs/specs/gamemodes/juggernaut.md`: reports trailing
  whitespace on the concurrent user-added line 1; no whitespace issue was
  reported in the new addendum.
- No tests or build were run; this was a specification-only edit.
- No configuration, runtime, journal, or human visual/gameplay acceptance was
  performed. The open design decisions above need resolution before treating
  the addendum as a complete implementation contract.
