# SWAT: Close-Quarters Tactical Mode

**Purpose:** capture the direction for a tense, close-range tactical mode with
shared physical movement, weapons, and world interactions.
**Audience:** gamemode, movement, collision, weapon, audio, presentation, and
map contributors.
**Status:** concept specification; scenario, teams, and win condition are not
yet decided.
**Does not:** establish a real-world law-enforcement simulator, choose a
school/public-shooter setting, or define a separate weapon/collision engine.

## Intended experience

The mode should feel slower and more attentive than standard arena combat.
Players listen for threats, check corners, use cover, and physically aim their
weapons. The setting should feel believable without pursuing realism for its
own sake.

The original ideas include a tactical team searching a building, defending a
location, and surviving until an evacuation helicopter arrives. Other notes
describe opposing teams entering a site and later barricading. Those are
different match structures; this document preserves them as candidates instead
of treating either as an agreed rule.

## Shared systems

- Use shared movement and collision with a mode-selected close-quarters
  contact policy. The policy should let an actor slide along small edges and
  pass through gaps that fit the configured squeeze allowance, while still
  blocking genuinely impassable openings.
- Preserve server authority and the shared weapon aim, damage, and replay
  paths.
- Weapon collision may be enabled or disabled through mode data. Physical
  aiming and collision are separate choices; a mode can keep physical aiming
  while selecting the appropriate weapon-contact behavior.
- Sound, camera, blood, and injury presentation use existing audio, camera,
  effects, and accessibility owners.
- Destruction is enabled by the shared default unless this mode explicitly
  opts out. Any destructive breach behavior uses the common destructible-world
  rules.

## Presentation direction

Support careful listening, readable spatial audio, restrained movement, and
close camera framing. Blood and injury effects may be part of the presentation,
but their intensity is presentation-only and must not affect damage rules.
Accessibility controls must allow reduced visual effects and clear non-audio
information.

## Decisions before a playable contract

- Choose the scenario structure: search/clear, defend until evacuation, or a
  defined combination.
- Define teams, actor counts, respawn/downing behavior, and win conditions.
- Select the first fictional map setting. Earlier notes mention a motel, home,
  mall, theater, airport, and neighborhood, and record discomfort with a
  school-shooter framing; do not assume one setting is selected.
- Set movement pace, weapon-contact defaults, and visual injury intensity
  through mode and player presentation settings.

## Acceptance direction

Once those decisions are made, validate narrow corridors, corners, doors,
cover, physical aiming, weapon contact, spatial audio, the selected objective,
and connected-client agreement. Human review must confirm that the setting and
presentation match the intended tone.

## Related specifications

- [Bodycam presentation](bodycam.md)
- [Gamemode runtime](gamemodes.md)
- [Movement](../movement/movement.md)
- [Collision architecture](../../architecture/collision/collision.md)
- [Destructible World](../destructible-world/destructible-world.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
