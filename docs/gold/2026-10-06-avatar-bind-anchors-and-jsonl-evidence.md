# Gold behavior: avatar bind fixes are proven through live JSONL evidence

- Date recorded: `2026-10-06`
- Status: `GOLD / REFERENCE` — evidence-first avatar and hybrid debugging
- Scope: girl-avatar body frames, hybrid AimBody, full ragdoll, and shared
  `events.jsonl` observability
- Related changelog:
  `docs/changelog/2026-10-06/20261006_023509-avatar-hybrid-transform-logging.md`

## The behavior worth preserving

When a visible avatar problem is ambiguous, the AI should make the runtime
observable before making a speculative visual change. The diagnostic must write
through the shared structured logger to the active run's canonical:

```text
logs/<date>/<run>/events.jsonl
```

The AI then runs the actual executable, finds the exact active journal, and
uses the records to rule out owners instead of inferring from source alone.

The working loop is:

```text
visible symptom
-> identify the active executable and events.jsonl
-> add a bounded owner-level diagnostic
-> run the candidate and inspect live records
-> identify the first divergent transform or state
-> make the smallest owner-correct fix
-> rebuild and inspect the same evidence path again
-> separate runtime proof from human visual acceptance
```

## The avatar case

The girl avatar initially appeared rotated in hybrid mode, and later one leg
appeared separated from the body. The useful records were not generic debug
strings. `AIMBODY_AVATAR_BIND_ROOT`, `AIMBODY_AVATAR_BIND_PART`, and
`AIMBODY_LIVE_SAMPLE` identified:

- the active avatar name, `abusivegirl3`;
- the loaded skeleton node and parent relationships;
- mesh-local transforms and determinant, including the intentionally mirrored
  right leg;
- camera forward, pitch, yaw, and player-root yaw;
- hybrid target, physical body, and final skeleton orientations;
- target-to-physics and target-to-final rotation deltas; and
- the actual parent/child attachment anchors and rest length used by the body.

The evidence showed that final skeleton writeback matched the physics body, so
the final renderer was not inventing the lean. It also showed that the girl
model's root bind correction differed from the default model. Source tracing
then found that hybrid sync clears non-body ancestor nodes and that generic
ragdoll attachment offsets were being reused for an avatar with custom leg
bind geometry.

## The owner-correct behavior

Normal-play AimBody, full local ragdoll, rebinds, corpses, and replicated bodies
derive their attachment anchors from the avatar's actual bind pose and capsule
geometry. Generic ragdoll configuration still owns limits and physical tuning,
but a universal default-avatar anchor must not override an avatar's authored
body placement.

The hybrid capture path restores authored ancestor bind transforms before
capturing procedural targets. The final physics sync may still neutralize those
ancestors while the physical body owns the rendered pose; the two boundaries
have different responsibilities and must not be conflated.

## Logging contract

Diagnostics for this path must:

1. use `StructuredLogger`, not a gameplay-local file writer;
2. flush to the active `events.jsonl` stream;
3. include avatar identity, simulation tick, and the transform stage;
4. sample live state at a bounded rate rather than logging every render frame;
5. record enough input, target, physics, and final state to identify the first
   divergence; and
6. state what the evidence proves and what still needs human visual review.

Bind records answer “what frame did this avatar load?” Live samples answer
“where did the running transform diverge?” Neither build success nor a single
  bind record is proof that the visible behavior is fixed.

## AI collaboration lesson

The important improvement was not merely adding more logging. It was making the
logging observable in the exact running executable and then reading the live
JSONL. That let the AI use actual runtime values to eliminate the final mesh
writeback as the cause, distinguish the intentional right-leg mirror from a
whole-body rotation, and find the avatar-specific attachment-frame issue.

This is the preferred behavior for future visual, animation, physics, and
network investigations: instrument first at the existing owner, run the real
candidate, inspect the canonical journal, and only then change behavior.

## Acceptance boundary

This document records the evidence-first method and the implemented ownership
direction. A gold runtime fix is complete only after a human observes the girl
avatar upright in hybrid mode and confirms that entering full ragdoll keeps both
legs attached while the corresponding JSONL anchor and rotation samples remain
consistent.
