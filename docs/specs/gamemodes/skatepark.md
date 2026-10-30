# Skatepark

**Purpose:** provide a low-pressure place to ride, practice, and tune
skateboards and BMX bikes.
**Audience:** vehicle, map, animation, and creator-tool contributors.
**Status:** desired mode behavior; the map and handling require playtesting.
**Does not:** require match scoring, a trick catalog, or multiplayer
competition in its first slice.

## Experience

Skatepark is an arcade movement playground. Players can ride alone, explore
ramps and bowls, fall, respawn, and immediately try again. It is the home for
testing skateboard and BMX tuning before those vehicles are used in other
modes.

## First playable slice

- Load one authored skatepark map with broad ramps, bowls, gaps, and varied
  surfaces.
- Spawn the selected skateboard or BMX and make reset/restart easy to reach.
- Use the shared trick-vehicle controls from
  [Skateboard and BMX](../vehicles/vehicles-skateboard.md).
- Keep the mode open-ended; no countdown, match timer, or required score is
  needed for the first slice.
- Allow the player to change vehicle tuning through the shared vehicle
  configuration and editor surfaces.

Trick scoring, leaderboards, rail grinding, shared challenges, cosmetics,
emotes, voice chat, and social mini-games are later additions. They should
reuse the appropriate shared score, chat, asset, and interaction owners.

## Acceptance

1. A player can spawn, ride, jump, fall, reset, and continue without leaving
   the mode.
2. The map exercises flat ground, ramps, curved transitions, edges, and gaps.
3. Board and BMX templates can be tuned independently without mode-specific
   vehicle simulation.
4. Collision and jump behavior remain stable at the fixed gameplay tick.
5. Human playtesting confirms the mode is inviting and quick to retry.

## Related specifications

- [Skateboard and BMX](../vehicles/vehicles-skateboard.md)
- [Vehicles](../vehicles/vehicles.md)
- [Voice chat](../ingame-chat/voice-chat.md)
- [In-App Video Editor](../creator-tools/video-editor.md)
