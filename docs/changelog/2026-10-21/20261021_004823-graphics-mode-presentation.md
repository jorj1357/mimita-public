# Graphics and mode presentation specification

- Result: `PASS_WITH_HUMAN_REVIEW` for documentation/specification work.
- Time: 2026-10-21T00:48:23Z; display time 2026-10-20 20:48:23 EDT (America/New_York).
- Branch: `20261011zt`.
- HEAD at start: `f6d6eeb2`.
- Runtime, build, deployment, and human visual acceptance: not performed; no code or runtime configuration was changed.

## Changes

### `docs/specs/graphics/graphics-presentation.md`

- Replaced the raw 3-line informal note file (the note text itself is preserved under “Original user notes”) with a full presentation contract. The source file before the change began:

  ```text
  2026 10 20 1955

  presnetiatn like
  ```

- New lines 1-24 define purpose, audience, scope, and the request for several mode presentation edits to apply together.
- New lines 26-64 define independent mode controls for damage numbers, actor/world hit effects, hit markers/sounds, hit flinch, blood, killfeed, ragdolls, healthbars, and outlines; numeric lighting and camera values remain proposals with unresolved units/ranges.
- New lines 66-93 define mode, role/actor-preset, and local-user ownership and precedence.
- New lines 95-118 specify a single validated profile reload, last-known-good retention, and effective-value inspection.
- New lines 120-153 record the source audit: the gamemode registry already polls files and retains the prior config on parse failure; mode JSON values still do not all reach effect consumers; Juggernaut has separate role FOVs, no mode aim-FOV setting, and no mode-specific dynamic-light amount demonstrated.
- New lines 154-191 define source and runtime acceptance and leave dynamic-light amount and mode-wide aim-FOV as `NEEDS_SPEC_DECISION` items.
- New lines 193-218 preserve the original user notes verbatim as non-normative context.

Representative exact addition (lines 99-107):

  ```text
  1. Edit several related settings in the active mode file, for example
     `config/gamemodes/juggernaut.json`.
  2. Save once while the game is running.
  3. The mode's configuration owner reads and validates a complete candidate.
  4. If valid, it installs the candidate as one new presentation generation at a
     safe client presentation boundary. All changed options become effective
     together; no frame or event uses a mixture of old and new values.
  ```

### `docs/specs/gamemodes/juggernaut.md`

- Lines 260-270 clarify that actor presets provide role-specific presentation defaults and the mode's explicit match-wide feedback values override those defaults. This resolves the prior wording that assigned all presentation ownership to actor presets.
- Before, lines 263-265 read:

  ```text
  The Juggernaut mode must not duplicate health, movement, FOV, or presentation
  ownership in several JSON files. The mode selects the role; the role resolves
  the actor preset; the actor preset supplies the actor configuration.
  ```

- After, lines 263-270 read:

  ```text
  The Juggernaut mode must not duplicate health, movement, or role-specific FOV
  ownership in several JSON files. The mode selects the role; the role resolves
  the actor preset; the actor preset supplies that role's actor configuration.
  Match-wide gameplay feedback switches belong to the gamemode's `presentation`
  object as specified in
  [`../graphics/graphics-presentation.md`](../graphics/graphics-presentation.md).
  An explicit mode value overrides the actor preset's default for that shared
  feedback option; an omitted value inherits the role default.
  ```

## Evidence and review

- Inspected `config/gamemodes/juggernaut.json`, `config/gamemodes/counterstrike.json`, both Juggernaut actor presets, `src/gamemode/gamemode.cpp`, `src/network/community-match-client.cpp`, `src/combat/actor-preset-weapons.cpp`, and `src/render/dynamic-light-config.cpp`.
- Confirmed parser fields and the current consumers in source. These findings do not claim live visible behavior.
- Read `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, and `docs/operations/task-completion/task-completion.md`.
- `docs/doc-review-09-03-2026.md`, which `docs/ROUTER.md` routes for documentation tasks, is absent from this checkout. Continued with the available routed guidance.
- Documentation checker result: changed documents have a clear purpose and audience; the new Juggernaut spec link resolves. No actionable TODO comment was found in the changed specs. TODO search hits in instruction/checker text are examples of what to search for.
- `git diff --check` passed for both changed specifications. Git reported only its expected LF-to-CRLF working-copy notice for the graphics spec.
- The worktree already contained unrelated user changes in configuration, gameplay, networking, NPC, and Swordsword files. Additional unrelated edits were visible during this session, including `config/gamemodes/sandbox.json` and `docs/specs/npc-behavior/npc-behavior.md`. They were preserved and not included in this change.

## Remaining review

Human review is needed to choose dynamic-light control shape/range, decide whether Juggernaut needs mode-wide aim-FOV, and confirm the intended visual balance. Implementation must then prove reload, last-good retention, effective-value reporting, and actual in-game effects in Juggernaut and Counter-Strike using the dev-loop runtime journal.
