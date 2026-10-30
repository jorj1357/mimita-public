# Bodycam Presentation

**Purpose:** define a reusable body-worn camera presentation profile for
tactical and other compatible modes.
**Audience:** camera, movement, weapon, audio, effects, and gamemode
contributors.
**Status:** desired behavior; existing settings and human visual acceptance
need review.
**Does not:** define a separate combat mode, replace shared camera ownership,
or claim documentary accuracy.

## Presentation goal

The view should feel like a body-worn camera through framing, movement,
exposure, and readable camera motion. It should remain legible and responsive
rather than adding effects that hide threats or make input feel delayed.

The profile may select slower movement tuning and physical weapon aiming for
the associated mode. These are mode rules and shared system settings, not
camera-only effects.

## Shared behavior

- Use the shared replay-aware camera and effects paths so recordings and
  exports reproduce the same presentation.
- Let the mode choose its weapon collision policy independently from physical
  aim. The initial tactical profile keeps physical aim available; contact
  behavior must be deliberately selected and playtested.
- Use shared accessibility settings for motion reduction, flashes, contrast,
  text size, audio levels, and visual injury intensity.
- Keep gameplay collision, weapon damage, and movement at the fixed tick.
  Camera shake and visual effects do not apply gameplay forces.

## Acceptance

Review the profile in live play and replay on a representative map. Verify that
camera movement communicates body motion without obscuring aim, reducing
control accuracy, or causing a performance regression. Check reduced-motion
settings, weapon aiming, weapon contact, sound, and replay export separately.

## Related specifications

- [SWAT close-quarters mode](swat.md)
- [Replay](../replay/replay.md)
- [Effects](../effects/effects.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
