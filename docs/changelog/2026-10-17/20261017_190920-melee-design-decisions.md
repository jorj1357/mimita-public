# Melee design decisions and related specifications

- UTC: 2026-10-17T19:09:20Z
- Branch: `20261011zt`
- Scope: documentation only; no implementation or runtime behavior changed.
- Preserved existing user changes in `config/accounts/default.json` and the
  deletion of `docs/specs/gamemodes/mimita-melee-inspired-arena-fighter-spec.md`.

## Updated specifications

- `docs/specs/gamemodes/supersmashbrosmelee.md`: added the Captain Falcon
  action list and one always-active JSON move-set item, map-before-intermission
  lifecycle, configurable per-limb hitboxes, shield and multi-tick grab rules,
  void-only stocks, animation phase retiming, acceptance scenarios, and an
  open-question register for design review.
- `docs/specs/animations/animations.md` and
  `docs/specs/animations/blender-animation-authoring.md`: documented Euler or
  quaternion input conversion into normalized quaternion runtime rotations,
  configurable move-phase clip sampling, and animation versus Melee sound
  ownership.
- `docs/specs/weapons/weapons.md`: documented one Captain Falcon moveset
  definition with child action data and a conceptual JSON example. The schema
  is explicitly a target, not a claim that the current loader supports it.
- `docs/specs/gamemodes/gamemodes.md`: selected maps resolve before the shared
  intermission lifecycle and spawn selection consumes the authoritative map
  JSON override manifest.
- `docs/specs/entity-editor/entity-editor.md`: specified importing GLB spawn
  nodes into editable JSON overrides, disabling/moving/adding points, stable
  source identity, reconciliation, and acceptance coverage.

## Exact wording changes

In `supersmashbrosmelee.md`, the previous open-register wording said:

> **OPEN before final implementation:** exact windup/active/recovery ticks and hitbox dimensions for each move; A/B chord priority when multiple modifiers are held; exact 30-tick double-click boundary and whether the second click cancels any current move; whether Space+left-click uptilt suppresses the jump on that tick; whether the earlier 1–10-tick jump scaling remains or Melee uses the existing movement jump behavior unchanged; shield-break overflow/expiry/release behavior; grab duration/range/escape/velocity policy; hitbox shape and any visual-limb proximity tolerance; custom knockback curve and percent formula; NPC move-selection policy; crouch pose; respawn delay/spawn protection; map spawn selection group; Blender exporter/basis and interpolation policy; lag compensation; and exact packet/log fields.

It now says:

> **OPEN:** The detailed design questions are collected in section 55. The first-version rules already decided are four lives, void-only deaths, 0 percent on respawn, map selected through the normal GUI before intermission, a 15-second intermission, 300 held-tick shield decay, 120-tick shield recovery after break, 300-tick grab immunity, and the listed Captain Falcon input/move candidates. Existing movement and actor-collider values are reused; do not invent Melee-specific jump or collider dimensions.

The percent/stock section previously said the server detects a fighter crossing
the stage's lethal `blast_zone`; it now explicitly says there are no
Melee-specific stage blast bounds and uses only the existing world void-death
rule. The first acceptance scenario now says to launch the fighter into the
void, with no separate stage boundary.

## Validation and remaining decisions

- `git diff --check` passed. No code tests or build were run because this is a
  documentation-only task.
- The source TODO at `docs/specs/gamemodes/supersmashbrosmelee.md:1` remains in
  place; a cleanup suggestion is to move it into a labeled historical note.
- No implementation, runtime, multiplayer, or human acceptance is claimed.
- Decisions still requested from the user are listed in section 55 of the
  Melee specification. Runtime-owner values and the current JSON parser's
  capabilities are implementation audits for the implementing agent.
