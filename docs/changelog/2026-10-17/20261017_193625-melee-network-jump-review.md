# Melee implementation-spec update

- UTC: `2026-10-17T19:36:25Z`
- Local display: `2026-10-17 15:36:25 EDT` (`America/New_York`)
- Branch: `20261011zt`
- HEAD observed: `bc4f69817358bcfcc2d4fafabafe7e410402e0a0`
- Result: `PASS_WITH_HUMAN_REVIEW` for specification completeness; no code,
  configuration, build, test, or runtime behavior changed in this session.
- Instructions/review documents: `AGENTS.md`, `docs/ROUTER.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`. The router-listed
  `docs/doc-review-09-03-2026.md` does not exist in this checkout.

## User edits preserved

Preserved pre-existing changes, including `config/accounts/default.json`,
`docs/specs/gamemodes/zombie-tower.md`, the untracked
`config/movement/movement-smash.json`, and its related untracked changelog.
Also preserved the pre-existing deletion of
`docs/specs/gamemodes/mimita-melee-inspired-arena-fighter-spec.md` and prior
documentation changelogs. The movement profile file was referenced by the new
spec but not edited.

## Specification changes

- `docs/specs/gamemodes/supersmashbrosmelee.md`: incorporated the user's
  latest decisions for shield release/break, grab radius/blocking/velocity,
  60-tick spawn protection, random eligible spawn selection, hazards,
  Captain Falcon audio defaults/assets, selectable jump policy, phase impulse
  events, collision support top, client prediction/referee policy, immediate
  JSON move reload, and remaining review questions. Removed the limb-proximity
  requirement and corrected the prior strict server-first wording. The section
  55 questions now distinguish product/tuning choices from implementation
  audits.
- `docs/specs/networking/networking.md`: added a Melee-specific
  prediction-first contact contract using shared lag history and correction.
  Candidate outcomes are accepted when historically plausible and replicated
  identically; impossible outcomes get a reason, correction, visible
  disagreement effect, and diagnostics. Button press order is part of input.
- `docs/specs/movement/movement.md`: added the selectable `press` and
  `hold_scaled` jump-input profile contract and points it to
  `config/movement/movement-smash.json`.
- `docs/specs/weapons/weapons.md`: removed the illustrative limb-proximity
  field, added attack audio volume/spatial defaults, described immediate
  mid-match definition reload, and assigned angle/speed contact-force math to
  shared weapon/combat policy rather than collision geometry.

## Exact wording examples

In the Melee spec, the old grab rule said:

> On a valid hit, move the target toward the grabber's hand/anchor over a configurable number of server ticks; do not teleport them in one tick, hold them in place, or disable their movement/attack/look input.

It now says:

> Its animated hand/limb-attached sphere hitbox defines its range; a valid grab requires only swept hitbox overlap. The initial radius target is 1 meter (2-meter diameter), measured in game meters; do not copy the legacy Force Punch value until its unit is verified. A shield blocks grabs at every shield strength.

In the networking spec, the general statement “The server owns what became real.” remains for ordinary gameplay. New Melee-specific section 45.5 adds the scoped rule:

> For an accepted candidate, the server records and replicates the same contact, percent, and impulse as the canonical outcome for every client; it does not substitute a second independently chosen hit result.

In the movement spec, before there was no Melee selectable jump-input section. New section 9 says:

> `hold_scaled`: the jump still starts immediately on the valid Space press at the configured minimum strength. While Space remains held, the fixed-tick movement owner adds the configured upward boost, scaling from the 1-tick minimum to the 10-tick maximum.

In the weapons spec, the previous move-set text said:

> A client prediction may use the same JSON snapshot, but the server owns accepted action, hit, percent, knockback, and shield result.

It now says:

> The client immediately predicts the action and candidate contact/result; the server uses the Melee referee contract in `../networking/networking.md` to accept plausible history or reject an impossible result.

## Evidence, TODOs, and remaining review

- `git diff --check` passed for the four edited specification files. A full-tree
  check reports trailing whitespace in the user's existing
  `docs/specs/gamemodes/zombie-tower.md:5526`; that file was left untouched.
- Checked cross-references for the networking, movement, and weapons docs.
- The Melee spec still begins with
  `docs/specs/gamemodes/supersmashbrosmelee.md:1`:
  `2026 10 08 1228 jorj todo explainbetter`. Suggested cleanup: retain it only
  under a clearly named historical-notes heading; the current numbered spec
  already supplies the explanation.
- Related collision TODOs remain at
  `docs/specs/colliisons/collisions.md:1` (`2026 10 11 1713 jorj todo explain`)
  and `docs/architecture/collision/actor-vs-actor-collisions.md:1`
  (`2026 10 08 1600 jorj todo explain how actors vs actor collisions work`).
  Suggested expansion: state that collision docs own overlap/support geometry,
  while `weapons.md` owns the speed/angle-to-force, percent, and impulse rule.
- Remaining user design/tuning questions are listed in section 55 of the Melee
  spec. An agent can implement the configurable systems and use clearly marked
  provisional values for playtesting, but exact balanced first-move values and
  force curves still need tuning or user-selected defaults.
- No runtime behavior, multiplayer acceptance, or implementation is claimed.
