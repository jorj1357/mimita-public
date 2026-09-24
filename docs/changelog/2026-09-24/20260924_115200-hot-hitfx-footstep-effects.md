# Hot hitfx footsteps and movement effects

Date: 2026-09-24 11:52:00 EST  
Branch: current working branch  
Status: PASS_WITH_HUMAN_REVIEW

## Request

Restore visible jump/air-jump presentation, enlarge the walking sphere, make
footstep variants change with repeats allowed, and make movement presentation
values live-editable through `config/hitfx.json` while keeping hot C++ as the
replaceable behavior/primitive owner.

## Pre-existing work

The worktree already contained unrelated edits across networking, logging,
hot-runtime ABI, server policy, and prior hot-presentation work. Those edits
were preserved and not attributed to this session. This session changed only
`config/hitfx.json` and
`src/hot-reload/modules/presentation/effect-composition.cpp`.

## Changes

`config/hitfx.json` now has `behaviorSource: "json"` and live `shape`,
`offset`, and `velocity` fields for ground jump, air jump, and footstep
recipes. Footstep `startRadius` remains `0.5`, replacing the hot fallback
`0.08`.

`src/hot-reload/modules/presentation/effect-composition.cpp` now parses those
fields on every hot reload, honors `behaviorSource: "cpp"` by skipping JSON
recipe overrides, maps several JSON shape names to existing hot meshes, uses
the live recipe values for jump/footstep creation, falls back to the cold jump
effect if hot entity creation fails, and gives each footstep a fresh audio
variant seed so `walk1` through `walk4` can repeat or change.

## Exact old/new behavior

Old hot footstep scale:

```cpp
recipeScale(r, 0.08f, 0.0f, lifetime, scale0, growth);
```

New hot footstep scale:

```cpp
recipeScale(r, 0.5f, 0.0f, lifetime, scale0, growth);
```

Old audio call had no event-specific seed:

```cpp
hotEmitRecipeSound(ctx, gameHash("footstep"), pos, 0, true, nullptr);
```

New audio call supplies a fresh per-footstep seed through
`HotAudioOverrideV1` before calling `hotEmitRecipeSound`.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/live-development/hot-audio-contract.md`
- `docs/features/live-code-development/live-code-development.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/regressions-v1.md`

## Validation

- `config/hitfx.json` parsed successfully.
- `git diff --check` passed; only line-ending warnings were emitted.
- Hot build completed successfully: generation 28, status `ok`, producing
  `build/hotreload/mimita-live-g000028.dll`.
- No cold EXE build was run. The running executable was not restarted or
  modified.

## Human review still needed

Load the candidate hot generation in the running game and verify ground jump,
air jump, live radius/velocity edits, supported shape changes, and varied
`walk1`-`walk4` audio. Source/build evidence does not prove visual or audio
acceptance.
