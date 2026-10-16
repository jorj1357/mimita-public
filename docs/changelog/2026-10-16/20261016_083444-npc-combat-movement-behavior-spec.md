# NPC combat and movement behavior specification

- Timestamp: 2026-10-16T08:34:44Z (2026-10-16 04:34:44 EDT)
- Branch: `20261011zt`
- HEAD: `3a34520009d91d10a571dfd08ca3ee90c9c56639`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Record the requested NPC sustained-fire weapon rotation and reusable,
JSON-authored movement repertoire as desired behavior, then explain the
configuration and implementation pieces currently available.

## Documents and skills reviewed

- User-provided `AGENTS.md`, `docs/ROUTER.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- `docs/specs/movement/movement.md`, `docs/specs/weapons/weapons.md`, and
  `docs/architecture/player-npc-systems/npc-movement.md`.
- `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/documentation-checker-v1.md`.
- The documentation route also requires `docs/doc-review-09-03-2026.md`, but
  that file does not exist in this checkout. No substitute was assumed.

## Changes made

- Added `docs/specs/npc-behavior/npc-behavior.md` (lines 1-232). It defines a
  six accepted-shot revolver visit, two accepted shotgun trigger-shot visit,
  next-fixed-tick switching, ongoing rotation through usable weapons, and
  normal ammo/cooldown authority. It defines a JSON-authored move repertoire
  and the requested contact-triggered dash/down-dash chain, five-dash burst,
  bunny hop, air strafe, and forward-only facing sweep. It also states
  selection, interruption, deterministic reproduction, acceptance cases, and
  what current config can approximate.
- Added one common route in `docs/ROUTER.md:78`:
  - Old: no NPC behavior route in the Common routes table.
  - New:

    ```text
    | NPC behavior, combat profiles, or authored NPC movement | `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md` |
    ```

## Findings

- `config/roles.json` selects behavior profiles and weapon sets; game modes
  may select `npc_behavior_profile`.
- `config/behavior-profiles.json` already has fire cadence, scored weapon
  preferences, and a switch threshold. `src/npc/npc.cpp` uses profile scoring
  and a switch cooldown. Its explicit empty-magazine switch is in the legacy
  no-active-profile branch, so the requested exact active-profile firing cycle
  is not currently guaranteed.
- `src/npc/npc-combat.cpp` uses the equipped weapon runtime and normal
  ammunition/cooldown path. `config/npc-difficulty.json` currently supplies a
  revolver/shotgun/rocket launcher loadout and global switch/dash chance
  controls.
- `config/weapons.json` defines the current revolver magazine as six rounds,
  and the shotgun magazine as two rounds with 15 pellets per attack, matching
  the requested first two weapon segments. Those weapon fields were unchanged
  in the pre-existing `config/weapons.json` edits.
- The existing NPC movement architecture already calls for hand-authored,
  reusable, interruptible movement primitives. The current movement config
  has Source auto-bhop and shared dash/down-dash inputs, but no JSON move
  library/executor for contact-triggered sequences.
- The specification interprets “two shotgun bullets” as two accepted shotgun
  trigger attacks (shells), each using the normal pellet pattern. This wording
  avoids treating individual pellets as separate ammunition-consuming shots.

### Finding

- Severity: medium
- Type: spec-code disagreement
- Specification: `docs/specs/npc-behavior/npc-behavior.md`, section 2.
- Exact quoted requirement: “request the next available weapon on the next 60
  Hz simulation tick.”
- Code path: `src/npc/npc.cpp`, weapon-selection block around lines 1296-1404;
  `src/npc/npc-combat.cpp`, `NpcCombat::tryFire`.
- Actual behavior: active behavior profiles score usable weapons and apply a
  switch cooldown; the explicit empty-magazine fallback is in the legacy
  no-active-profile branch.
- Expected behavior: an opted-in profile advances its fixed shot-count cycle
  and switches to the next usable weapon on the following fixed tick.
- Evidence: current code inspection plus `config/weapons.json` values of six
  revolver rounds and two shotgun rounds/15 pellets per attack.
- Recommended action: add profile-owned visit counts/cycle selection and run
  the cycle through current server-authoritative fire/equip owners.
- Human decision required: none to record this requested behavior. The wording
  treats “two shotgun bullets” as two shotgun attacks, not two pellets.

## Review and validation

- `spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`. The requested
  behavior is now explicit; implementation remains partial. No code was
  changed and no runtime claim is made.
- `documentation-checker-v1.md`: `PASS_WITH_HUMAN_REVIEW`. Added file has
  purpose/audience/does-not scope. All newly referenced specification and
  architecture paths exist. The full-doc TODO search found only instruction
  examples and historical mentions, not an actionable inline TODO requiring
  a suggestion.
- `git diff --check` reported no whitespace errors in the router edit. It did
  report pre-existing trailing whitespace in `config/behavior-profiles.json`
  and `docs/specs/networking/networking.md`; neither file was changed here.
  The new spec was separately checked for trailing whitespace and is clean.
- No build, test, or runtime validation was needed for this documentation-only
  change.

## Pre-existing changes and review still needed

The checkout contained many unrelated modified and untracked files before this
change, including existing edits to `docs/architecture/player-npc-systems/npc-movement.md`
and `docs/specs/movement/movement.md`. Those edits were preserved. This session
added only the new NPC behavior specification, its router row, and this one
changelog.

Human review is still needed to confirm the intended profile data shape and
whether the two-shot shotgun visit is the desired policy for every weapon set
that opts in. Gameplay feel and implementation have not been validated.
