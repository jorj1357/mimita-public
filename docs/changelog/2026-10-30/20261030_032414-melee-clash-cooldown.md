# Melee clash cooldown and pacing

- Status: PASS_WITH_HUMAN_REVIEW
- Time: 2026-10-30T03:24:14Z (2026-10-29 23:24:14 America/New_York)
- Branch: `20261011zt`
- HEAD: `1b5f3bac`

## Result

Separated Mode 1 actor-damage cadence from weapon-versus-weapon clash
responses. The shared melee specification and Swordsword specification now
state that clash knockback, sparks, and ricochet audio share a pair-scoped
minimum six-simulation-tick cooldown (100 ms), without throttling actor damage.
Added that fast, responsive melee is the initial priority and slower,
deliberate styles can follow; Mode 2 is not inherently a slow-attack model.

## Exact change

Files and headings:

- `docs/specs/weapons/melee/melee-weapons.md`, “Mode 1 — Continuous Physical
  Contact: No Artificial Melee Cooldown”: actor damage remains up to one hit
  per victim/weapon/tick; weapon clash response is explicitly separated and
  cooldown-gated.
- `docs/specs/weapons/melee/melee-weapons.md`, new “Weapon-versus-weapon clash
  cooldown” subsection: one pair-scoped response per six-tick interval,
  covering opposing impulse and presentation; cooldown does not throttle
  actor damage.
- `docs/specs/weapons/melee/melee-weapons.md`, Mode 2 TODO: fast, responsive
  melee is the first development priority; deliberate slower styles may come
  later; Mode 2 itself does not require slow attacks.
- `docs/specs/weapons/melee/swordsword.md`, “Continuous blade contact”: added
  the matching clash cooldown and fast-first pacing rule.

Before, the shared spec prohibited a generic melee hit cooldown and allowed
one hit per victim/weapon/tick, without distinguishing weapon clashes. The
Swordsword spec required opposing impulse, sparks, and ricochet audio but did
not specify their cadence. After, actor damage keeps the no-throttle rule,
while clash response is cooled down for six fixed simulation ticks.

## Source review and findings

- `src/network/server-physical-contact-actors.cpp` maintains
  `ActorContactRuntime::lastClashTick`; the direct blade-to-blade path checks
  `tick - lastClash >= 6` before applying opposing impulses, broadcasting the
  clash event, and logging `weapon.clash`. This is the source basis for the
  six-tick initial policy.
- The block-contact path in the same file broadcasts a weapon-clash event and
  applies an impulse without the six-tick gate. This is a spec/code gap; this
  documentation change does not modify the implementation.
- `src/network/multiplayer-projectiles.cpp`,
  `mpProcessMeleeHitEventPacket`, displays the clash event with sparks and a
  metal sound. The server cooldown controls those responses for direct
  blade-to-blade clashes.
- The spec update is intended to apply one policy to all weapon-clash paths;
  block contacts still need implementation alignment and runtime verification.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/weapons/melee/melee-weapons.md`, and
  `docs/specs/weapons/melee/swordsword.md`; inspected the owning server and
  client event paths listed above.
- Specification review: PASS_WITH_HUMAN_REVIEW. The documented clash policy
  follows the existing direct-contact interval; block-contact parity remains
  unresolved in code.
- `git diff --check` passed for the changed documents. No build, test, runtime
  scenario, or human gameplay acceptance was performed for this documentation
  update.
- Unrelated worktree changes remain preserved, including pre-existing config,
  submodule, and documentation edits, as well as the prior turn's Mode 1/Mode 2
  clarification and changelog.
