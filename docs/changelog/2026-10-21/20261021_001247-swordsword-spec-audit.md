# Task

- Task ID: swordsword-spec-audit
- Summary: Traced current Swordsword input, collision, damage, and animation owners against the requested behavior and assessed whether the specs are implementation-ready.
- Status: NEEDS_SPEC_DECISION
- Date, time, timezone: 2026-10-21T00:12:47Z (2026-10-20 20:12:47 EDT, America/New_York)
- Branch: 20261011zt
- Base commit: 0b5145b26796b94af55c1744c9b00f86cc44bd94
- Final commit: 0b5145b26796b94af55c1744c9b00f86cc44bd94

# Pre-existing changes

- `docs/specs/weapons/swordsword.md` was already modified before this audit. It was read but not edited; the user's clarifications in chat were not written into the spec.
- The checkout had other unrelated pre-existing edits and untracked changelogs. None were changed or removed.

# Requested behavior

Inspect how Swordsword currently works and determine whether its specification is sufficient. User clarified: right-click twice triggers a defensive animation; attacks do not propel the wielder, while contact still knocks the target back; all player/NPC attacker-victim pairings must damage and kill through the shared path; weapon identity should select animation for players and NPCs; the existing model/collision entry is acceptable and no new collision entry is requested.

# Specification alignment

- Reviewed `docs/specs/weapons/swordsword.md`, `docs/specs/weapons/weapons.md`, `docs/specs/weapons/melee-weapons.md`, `docs/specs/networking/networking.md`, `docs/specs/animations/animations.md`, and `docs/architecture/collision/actor-vs-actor-collisions.md`.
- Focused skill: `docs/skills/spec-behavior-review-v1.md` — source/spec comparison complete; implementation requirements need consolidation.
- Finding: `weapons.md` §9 says SwordSword is always active and damages on physical contact, but `server-physical-contact.cpp::buildPhysicalShape` requires a committed `physicalAttackTimeline` and returns no shape outside its active window. This conflicts with the request that input selects animation while contact remains damaging.
- Finding: `weapon-swordsword.cpp::startSlash` adds half dash impulse to wielder velocity; `startLunge` adds full dash impulse. Both contradict the clarified no-wielder-propulsion requirement.
- Finding: server contact damage currently computes fixed slash/lunge base damage and configured knockback; it does not implement the requested contact-force/angle damage scaling or associated impact sound pitch/volume scaling.
- Finding: server physical-contact attacker iteration is over `ServerPlayer` only. It checks player and NPC victims, but has no NPC attacker loop. NPC Swordsword combat in `npc-combat.cpp` is a close-range hitscan approximation, not the shared swept physical-contact path.
- Finding: typed `applyActorDamage` in `server-damage.cpp` currently rejects NPC victims with `phase1-player-victim-required`. The desired all-pair actor damage contract therefore needs a shared victim path as well as NPC attacker support; current general actor-damage support is not complete for NPC-vs-NPC or player-vs-NPC.
- Finding: `config/weapons.json` gives Swordsword `pose_id: hafs`. Player animation chooses poses using the equipped weapon's `poseId` and `weaponPoseState`; the current SwordSword state exposes slash/lunge values, but no double-click variants or held defensive state were found in the inspected input/animation owners. NPC combat uses a different path; no equivalent per-weapon animation selection was found there.
- Existing `config/weaponcollisions.json` already has a Swordsword box. The user confirmed it is acceptable; no new collision entry is needed.

# Exact implementation changes

No source or specification changes. This changelog is the only file created by this session; it had no prior content. The changelog itself records the evidence and conclusions above.

# Validation

- Source inspection only; no build, tests, live runtime scenario, or human playtest was requested or performed.
- Runtime behavior remains unverified. Findings describe source paths and spec alignment, not proof of the current executable's behavior.
- Existing source/spec edits were preserved.

# Human review and next decision

- Human review of the proposed unified contract remains required before implementation. The user's chat clarifications resolve several move/impulse/collision choices, but have not yet been incorporated into `swordsword.md` or reconciled with the always-active contact contract in `weapons.md`.
- The resulting spec should explicitly say: clicks select weapon animations and do not propel the wielder; the physical blade remains damaging independent of attack animation; contact knockback may remain; attacks use a single shared actor damage owner for Player/NPC attacker and victim combinations; weapon definition selects shared player/NPC animation poses; and acceptance proves damage and kills for all four actor pairings.
