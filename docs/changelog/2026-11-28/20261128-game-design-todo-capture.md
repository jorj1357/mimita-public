# Changelog: 2026-11-28 game design TODO capture

## Summary

Preserved the user's 2026-11-28 game and product notes verbatim in `docs/operations/todo-sort-these/2026-11-28-game-design-todo.md`, then added dated quote-and-interpretation sections to the routed specifications. The source note remains the canonical record; tentative values and open questions are labeled as such.

## Files changed

- `docs/operations/todo-sort-these/2026-11-28-game-design-todo.md` — verbatim note plus index and reading guidance.
- `docs/specs/tas/tas.md` — tick-level input editing, playback invariants, determinism target, and bot/TAS relationship.
- `docs/specs/replay/replay.md` — portable TAS file framing and voice-aware clip capture interpretation.
- `docs/specs/creator-tools/video-editor.md` — short clip workflow for duels and vehicle matches.
- `docs/specs/gamemodes/vehicle-arena.md` — desired human/NPC lineups, freestyle target, and RLBot/SSL import as an option to investigate.
- `docs/specs/gamemodes/gamemodes.md` — MiMITA Combat and separate casual/competitive arcade interpretations.
- `docs/specs/gamemodes/rpg.md` — free, cooperative infinite-dungeon direction distinct from PvP.
- `docs/specs/gamemodes/payload.md` — long, multi-section, textured-map direction.
- `docs/specs/distribution/distribution.md` — coherent package direction with sales model unresolved and specified free modes.

## Evidence and validation

Documentation-only change. Reviewed the router, specification-authoring guide, and routed destination specs. No build or runtime validation was appropriate. No gameplay implementation or runtime claim was made. Exact FPS-independent determinism, bot skill targets, clip/voice capture, balance values, map duration, package price, and storefront model remain future work or open decisions as labeled in the destination docs.
