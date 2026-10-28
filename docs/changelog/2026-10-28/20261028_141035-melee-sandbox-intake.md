# Melee Sandbox damage report intake

- Time: 2026-10-28 14:10:35 EDT (2026-10-28T18:10:35Z)
- Branch: `20261011zt`
- HEAD: `933cfb48`
- Result: `PASS_WITH_HUMAN_REVIEW` — investigation and issue intake recorded; cause remains unconfirmed.

## Scope and finding

Recorded the user's report that SwordSword and mace sometimes pass through
targets without damage, chainsaw seems more reliable, and attack attempts can
show “Melee move set could not start.” Desired behavior is reliable normal
physical damage in Sandbox regardless of whether the animation is authored in
the Super Smash Bros. Melee style.

The inspected journal is `logs/2026-10-28/20261028_164809/events.jsonl`.
It records both client and server loading move sets for SwordSword (4 actions),
mace (1 action), and chainsaw (1 action). It also contains repeated Sandbox
`weapon.equip.rejected` events for requested slot 10, reason
`not-owned-or-unknown`. The journal does not provide a matching attack to
contact to damage outcome chain, so it cannot prove that the intended weapon was
equipped during the reported attack or locate the first missing stage. No
client/server disagreement is demonstrated by this run.

Configuration inspection found different contact radii: mace 1.35,
SwordSword 0.08, chainsaw 1.16. Mace's `custom_params` contact damage range is
1–5, while its move action declares 25–250. `damageTickInterval` is 0.01 for
mace and SwordSword and 0.001 for chainsaw; these logs do not establish the
runtime meaning of that field. These are investigation leads, not confirmed
causes. The existing uncommitted changes in `config/weapons/mace/config.json`
were preserved.

## Repository changes

- `docs/specs/open-gameplay-issues.md`, new entry “SwordSword and mace physical
  hits fail inconsistently in Sandbox” (near line 1655): records expected
  behavior, observed log evidence, the unavailable attack/contact/damage
  chain, configuration leads, and the needed trace. Status is UNRESOLVED / CAUSE
  UNCONFIRMED.
- This changelog is the sole session changelog.

No code, weapon configuration, or regression record was changed. The report is
not confirmed as a regression because this run does not reproduce or locate the
failure.

## Documents and review

- `docs/ROUTER.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/weapons/melee/melee-weapons.md`
- `docs/specs/networking/networking.md`
- `docs/specs/animations/animations.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/regressions-v1.md`
- `docs/skills/spec-behavior-review-v1.md` — PASS: specified intent and
  observed evidence were kept distinct; cause and acceptance remain open.
- `docs/skills/logging-checker-v1.md` — PASS_WITH_HUMAN_REVIEW: current journal
  proves move-set loading and equip rejections but lacks per-attempt attack,
  contact, and authoritative damage outcomes needed to localize the failure.

## Validation and remaining evidence

- Documentation diff inspected; no executable or build was changed, so no
  build was run.
- Runtime evidence: existing journal only; no new runtime scenario was run.
- Human gameplay review: not performed.
- Next useful evidence: reproduce one Sandbox encounter with known attacker,
  target, equipped definition, and exact build; correlate equip acceptance,
  melee action start, weapon pose/shape, authoritative contact query, contact
  batch/referee result, and damage application or rejection on client/server.

## Pre-existing workspace changes

Preserved all unrelated edits: modified `config/accounts/default.json`,
`config/collision.json`, `config/weaponcollisions.json`, and
`config/weapons/mace/config.json`; untracked assets under
`assets/objects/things/effects-objects/` and
`assets/objects/weapons/weapon-objects/`, plus
`assets/textureshq/explosionmarkv1.png` and
`assets/textureshq/rocketsmokev1.png`; and the pre-existing deletion of
`docs/specs/open-gameplay-issues-human.md`.
