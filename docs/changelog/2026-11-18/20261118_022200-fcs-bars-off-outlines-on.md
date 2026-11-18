# FCS bars off, outlines on

## Change

- FCS presentation: enemy and friendly healthbars disabled; player and friendly
  outlines enabled; enemy outline alpha set to 0.5.
- Restored `config/healthbar.json` distance settings to the previous `0.01 m`
  maximum and `40 m` end fade. The own-healthbar setting remains false.

## Limits and evidence

- The gamemode schema currently supports only `enemy_outline_alpha`; this
  setting does not make friendly outlines translucent. Friendly outlines keep
  their existing opacity. A shared or friendly alpha requires a code/schema
  change.
- Configuration was edited. No build, config-reload journal evidence, or
  in-game visual acceptance was collected in this task.

## Collision explanation / follow-up

- Existing `collision.actor-world` records summarize a blocked actor, contact
  count, correction, and penetration; `collision.bounce.applied` records
  collision normal and before/after velocity. Those records can show that a
  contact changed motion, but they do not consistently provide one event that
  links world contact point, attacker, target, weapon, and applied damage.
- Next diagnostic: extend the existing owner-level contact record to carry the
  collision point, normal, surface/triangle, actor IDs, weapon/action, and
  damage result under one correlation ID; render a short-lived marker and
  direction arrow using the existing debug visualization path. Keep fixed 60
  Hz gameplay and the cached broadphase path unchanged.
- For corner snagging, capture a current human-recorded scenario at the exact
  corner, then compare the recorded input with collision corrections, contact
  normals, penetration, and speed before making a solver change.
