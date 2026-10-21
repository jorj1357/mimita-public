# Melee NPC attack loop specification

- Status: `PASS`. Section 34 now states the desired NPC behavior plainly: keep approaching a reachable target, repeatedly attack using the configured post-move gap, and do not run, orbit, or strafe away as a combat tactic.
- Time: 2026-10-21T01:47:24Z (2026-10-20 21:47:24 EDT, America/New_York).
- Branch: `20261011zt`; HEAD: `094e6e5a`.
- This was a specification-only change. No code, configuration, build, or runtime behavior was changed or validated in this session.

## Exact edit

- `docs/specs/gamemodes/supersmashbrosmelee.md:237`, section 34. Before, the paragraph defined the close-combat profile as one that “must close on and attack its target instead of choosing Circle, Strafe, or Retreat.” After, it adds: “While its target is alive and reachable, the Melee NPC must keep closing distance until it can attack, then repeatedly choose and perform attacks with the configured post-move gap. It must not deliberately run away, orbit, or strafe away from the target as a combat tactic. When an attack is out of range, it should approach the target rather than abandon the attack loop.”
- This changelog is new; there was no prior text.

## Validation

- Read the current Melee spec section 34 and the repository time-and-filename standard.
- Documentation review: the added behavior is labeled `DECIDED` and keeps the existing gap semantics: 1–30 fixed ticks of wait after a move finishes, plus that move's own duration.
- No tests or builds were appropriate for this specification-only edit. Runtime implementation still requires separate verification against this clarified requirement.
